#ifndef __INC_METIN2_TCP_FAULT_PROXY_HPP__
#define __INC_METIN2_TCP_FAULT_PROXY_HPP__

#include <string>
#include <thread>
#include <atomic>
#include <chrono>
#include <iostream>
#include <algorithm>
#include <cstring>

#ifdef __WIN32__
#include <winsock2.h>
#include <ws2tcpip.h>
using proxy_socket_t = SOCKET;
#define PROXY_INVALID_SOCKET INVALID_SOCKET
#define PROXY_SOCKET_ERROR SOCKET_ERROR
#define PROXY_CLOSE_SOCKET(s) ::closesocket(s)
#else
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
using proxy_socket_t = int;
#define PROXY_INVALID_SOCKET (-1)
#define PROXY_SOCKET_ERROR (-1)
#define PROXY_CLOSE_SOCKET(s) ::close(s)
#endif

#ifdef PASSTHROUGH
#undef PASSTHROUGH
#endif

enum class ProxyFaultMode {
	FAULT_PASSTHROUGH,
	FAULT_SEVER_MID_QUERY,
	FAULT_DROP_SERVER_RESPONSE
};

class TcpFaultProxy {
public:
	TcpFaultProxy()
		: m_listenSock(PROXY_INVALID_SOCKET)
		, m_clientSock(PROXY_INVALID_SOCKET)
		, m_serverSock(PROXY_INVALID_SOCKET)
		, m_listenPort(0)
		, m_targetPort(0)
		, m_running(false)
		, m_faultMode(ProxyFaultMode::FAULT_PASSTHROUGH)
		, m_waitingForResponse(false)
		, m_droppedResponseCaptured(false)
		, m_severMidQueryTriggered(false)
	{
#ifdef __WIN32__
		WSADATA wsaData;
		WSAStartup(MAKEWORD(2, 2), &wsaData);
#endif
	}

	~TcpFaultProxy() {
		Stop();
	}

	int Start(const std::string& targetHost, int targetPort) {
		m_targetHost = targetHost;
		m_targetPort = targetPort;

		m_listenSock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
		if (m_listenSock == PROXY_INVALID_SOCKET) {
			std::cerr << "TcpFaultProxy: socket creation failed" << std::endl;
			return 0;
		}

		int opt = 1;
		setsockopt(m_listenSock, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt));

		sockaddr_in addr;
		std::memset(&addr, 0, sizeof(addr));
		addr.sin_family = AF_INET;
		addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
		addr.sin_port = 0; // OS chooses free ephemeral port

		if (bind(m_listenSock, (sockaddr*)&addr, sizeof(addr)) == PROXY_SOCKET_ERROR) {
			std::cerr << "TcpFaultProxy: bind failed" << std::endl;
			PROXY_CLOSE_SOCKET(m_listenSock);
			m_listenSock = PROXY_INVALID_SOCKET;
			return 0;
		}

		socklen_in len = sizeof(addr);
		if (getsockname(m_listenSock, (sockaddr*)&addr, &len) == PROXY_SOCKET_ERROR) {
			std::cerr << "TcpFaultProxy: getsockname failed" << std::endl;
			PROXY_CLOSE_SOCKET(m_listenSock);
			m_listenSock = PROXY_INVALID_SOCKET;
			return 0;
		}

		m_listenPort = ntohs(addr.sin_port);

		if (listen(m_listenSock, 5) == PROXY_SOCKET_ERROR) {
			std::cerr << "TcpFaultProxy: listen failed" << std::endl;
			PROXY_CLOSE_SOCKET(m_listenSock);
			m_listenSock = PROXY_INVALID_SOCKET;
			return 0;
		}

		m_running = true;
		m_workerThread = std::thread(&TcpFaultProxy::Run, this);

		// Give proxy thread a brief moment to start listening
		std::this_thread::sleep_for(std::chrono::milliseconds(50));
		return m_listenPort;
	}

	void Stop() {
		m_running = false;
		if (m_listenSock != PROXY_INVALID_SOCKET) {
			PROXY_CLOSE_SOCKET(m_listenSock);
			m_listenSock = PROXY_INVALID_SOCKET;
		}
		CloseCurrentConnection();
		if (m_workerThread.joinable()) {
			m_workerThread.join();
		}
	}

	void SetFaultMode(ProxyFaultMode mode) {
		m_faultMode = mode;
		m_waitingForResponse = false;
		m_droppedResponseCaptured = false;
		m_severMidQueryTriggered = false;
	}

	bool WasDroppedResponseCaptured() const { return m_droppedResponseCaptured.load(); }
	bool WasSeverMidQueryTriggered() const { return m_severMidQueryTriggered.load(); }

	bool WaitForDroppedResponse(int timeoutMs = 1000) const {
		for (int i = 0; i < timeoutMs / 10; ++i) {
			if (m_droppedResponseCaptured.load())
				return true;
			std::this_thread::sleep_for(std::chrono::milliseconds(10));
		}
		return m_droppedResponseCaptured.load();
	}

	bool WaitForSeverMidQuery(int timeoutMs = 1000) const {
		for (int i = 0; i < timeoutMs / 10; ++i) {
			if (m_severMidQueryTriggered.load())
				return true;
			std::this_thread::sleep_for(std::chrono::milliseconds(10));
		}
		return m_severMidQueryTriggered.load();
	}

	int GetListenPort() const { return m_listenPort; }

private:
	static void SeverSocket(proxy_socket_t s) {
		if (s != PROXY_INVALID_SOCKET) {
			struct linger sl;
			sl.l_onoff = 1;
			sl.l_linger = 0; // Immediate RST
			setsockopt(s, SOL_SOCKET, SO_LINGER, (const char*)&sl, sizeof(sl));
			PROXY_CLOSE_SOCKET(s);
		}
	}

	void CloseCurrentConnection() {
		proxy_socket_t c = m_clientSock.exchange(PROXY_INVALID_SOCKET);
		proxy_socket_t s = m_serverSock.exchange(PROXY_INVALID_SOCKET);
		if (c != PROXY_INVALID_SOCKET) PROXY_CLOSE_SOCKET(c);
		if (s != PROXY_INVALID_SOCKET) PROXY_CLOSE_SOCKET(s);
	}

	void Run() {
		while (m_running) {
			fd_set fds;
			FD_ZERO(&fds);
			FD_SET(m_listenSock, &fds);
			timeval tv;
			tv.tv_sec = 0;
			tv.tv_usec = 200000; // 200ms

			int sel = select((int)m_listenSock + 1, &fds, nullptr, nullptr, &tv);
			if (sel <= 0 || !m_running) {
				continue;
			}

			sockaddr_in clientAddr;
			socklen_in clientLen = sizeof(clientAddr);
			proxy_socket_t clientFd = accept(m_listenSock, (sockaddr*)&clientAddr, &clientLen);
			if (clientFd == PROXY_INVALID_SOCKET) {
				continue;
			}

			// Connect to target DB
			proxy_socket_t serverFd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
			if (serverFd == PROXY_INVALID_SOCKET) {
				PROXY_CLOSE_SOCKET(clientFd);
				continue;
			}

			int nodelay = 1;
			setsockopt(clientFd, IPPROTO_TCP, TCP_NODELAY, (const char*)&nodelay, sizeof(nodelay));
			setsockopt(serverFd, IPPROTO_TCP, TCP_NODELAY, (const char*)&nodelay, sizeof(nodelay));

			sockaddr_in targetAddr;
			std::memset(&targetAddr, 0, sizeof(targetAddr));
			targetAddr.sin_family = AF_INET;
			targetAddr.sin_addr.s_addr = inet_addr(m_targetHost.c_str());
			targetAddr.sin_port = htons(m_targetPort);

			if (connect(serverFd, (sockaddr*)&targetAddr, sizeof(targetAddr)) == PROXY_SOCKET_ERROR) {
				PROXY_CLOSE_SOCKET(clientFd);
				PROXY_CLOSE_SOCKET(serverFd);
				continue;
			}

			m_clientSock = clientFd;
			m_serverSock = serverFd;

			ForwardLoop(clientFd, serverFd);

			CloseCurrentConnection();
		}
	}

	void ForwardLoop(proxy_socket_t clientFd, proxy_socket_t serverFd) {
		char buf[16384];

		while (m_running) {
			fd_set rfds;
			FD_ZERO(&rfds);
			FD_SET(clientFd, &rfds);
			FD_SET(serverFd, &rfds);

			proxy_socket_t maxFd = std::max(clientFd, serverFd);
			timeval tv;
			tv.tv_sec = 0;
			tv.tv_usec = 100000; // 100ms

			int sel = select((int)maxFd + 1, &rfds, nullptr, nullptr, &tv);
			if (sel < 0) {
#ifdef __WIN32__
				int err = WSAGetLastError();
#else
				int err = errno;
#endif
				std::cerr << "TcpFaultProxy: select error: " << err << std::endl;
				break;
			}
			if (sel == 0) {
				continue;
			}

			// 1. Data from client -> server
			if (FD_ISSET(clientFd, &rfds)) {
				int n = recv(clientFd, buf, sizeof(buf), 0);
				if (n <= 0) {
#ifdef __WIN32__
					int err = WSAGetLastError();
#else
					int err = errno;
#endif
					std::cerr << "TcpFaultProxy: client disconnected (recv returned " << n << ", err=" << err << ")" << std::endl;
					break; // Client closed
				}

				if (m_faultMode == ProxyFaultMode::FAULT_SEVER_MID_QUERY) {
					// Forward query to server so server receives and executes it
					send(serverFd, buf, n, 0);
					// Give server 150ms to be actively in-flight
					std::this_thread::sleep_for(std::chrono::milliseconds(150));
					// Abruptly sever client connection
					SeverSocket(clientFd);
					SeverSocket(serverFd);
					m_severMidQueryTriggered = true;
					m_faultMode = ProxyFaultMode::FAULT_PASSTHROUGH;
					std::cerr << "TcpFaultProxy: FAULT_SEVER_MID_QUERY triggered! Sockets severed." << std::endl;
					break;
				}
				else if (m_faultMode == ProxyFaultMode::FAULT_DROP_SERVER_RESPONSE) {
					// Forward command to server
					send(serverFd, buf, n, 0);
					m_waitingForResponse = true;
				}
				else {
					// Normal forward
					send(serverFd, buf, n, 0);
				}
			}

			// 2. Data from server -> client
			if (FD_ISSET(serverFd, &rfds)) {
				int n = recv(serverFd, buf, sizeof(buf), 0);
				if (n <= 0) {
#ifdef __WIN32__
					int err = WSAGetLastError();
#else
					int err = errno;
#endif
					std::cerr << "TcpFaultProxy: server disconnected (recv returned " << n << ", err=" << err << ")" << std::endl;
					break; // Server closed
				}

				if (m_faultMode == ProxyFaultMode::FAULT_DROP_SERVER_RESPONSE && m_waitingForResponse) {
					// The server has executed the query and returned response!
					// Drop response and immediately sever client connection!
					SeverSocket(clientFd);
					SeverSocket(serverFd);
					m_droppedResponseCaptured = true;
					m_waitingForResponse = false;
					m_faultMode = ProxyFaultMode::FAULT_PASSTHROUGH;
					std::cerr << "TcpFaultProxy: FAULT_DROP_SERVER_RESPONSE triggered! OK packet dropped, sockets severed." << std::endl;
					break;
				}
				else {
					send(clientFd, buf, n, 0);
				}
			}
		}
	}

private:
#ifdef __WIN32__
	using socklen_in = int;
#else
	using socklen_in = socklen_t;
#endif

	proxy_socket_t m_listenSock;
	std::atomic<proxy_socket_t> m_clientSock;
	std::atomic<proxy_socket_t> m_serverSock;
	int m_listenPort;
	std::string m_targetHost;
	int m_targetPort;
	std::atomic<bool> m_running;
	std::thread m_workerThread;

	std::atomic<ProxyFaultMode> m_faultMode;
	std::atomic<bool> m_waitingForResponse;
	std::atomic<bool> m_droppedResponseCaptured;
	std::atomic<bool> m_severMidQueryTriggered;
};

#endif // __INC_METIN2_TCP_FAULT_PROXY_HPP__
