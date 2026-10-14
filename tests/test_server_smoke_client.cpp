#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <cstring>
#include <string>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
typedef SOCKET sock_t;
#define CLOSESOCK closesocket
#else
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
typedef int sock_t;
#define INVALID_SOCKET (-1)
#define CLOSESOCK close
#endif

#pragma pack(push, 1)
struct SmokePacketGCPhase {
	uint8_t header; // 0xFD (HEADER_GC_PHASE)
	uint8_t phase;  // 1 = PHASE_HANDSHAKE, 2 = PHASE_LOGIN
};

struct SmokePacketHandshake {
	uint8_t  bHeader;     // 0xFF (HEADER_GC_HANDSHAKE / HEADER_CG_HANDSHAKE)
	uint32_t dwHandshake;
	uint32_t dwTime;
	int32_t  lDelta;
};
#pragma pack(pop)

static_assert(sizeof(SmokePacketGCPhase) == 2, "SmokePacketGCPhase must be 2 bytes");
static_assert(sizeof(SmokePacketHandshake) == 13, "SmokePacketHandshake must be 13 bytes");

static void sleep_ms(int ms) {
#ifdef _WIN32
	Sleep(static_cast<DWORD>(ms));
#else
	usleep(static_cast<useconds_t>(ms) * 1000);
#endif
}

static bool recv_exact_timeout(sock_t s, uint8_t* buf, size_t len, int timeout_sec) {
	size_t got = 0;
	while (got < len) {
		fd_set rfds;
		FD_ZERO(&rfds);
		FD_SET(s, &rfds);
		struct timeval tv;
		tv.tv_sec = timeout_sec;
		tv.tv_usec = 0;
		int r = select(static_cast<int>(s + 1), &rfds, nullptr, nullptr, &tv);
		if (r <= 0)
			return false;
		int n = recv(s, reinterpret_cast<char*>(buf + got), static_cast<int>(len - got), 0);
		if (n <= 0)
			return false;
		got += static_cast<size_t>(n);
	}
	return true;
}

static int run_mock_db(int port, const char* ready_file, const char* out_file) {
	sock_t srv = socket(AF_INET, SOCK_STREAM, 0);
	if (srv == INVALID_SOCKET) {
		std::fprintf(stderr, "[mock-db] socket() failed\n");
		return 1;
	}
	int opt = 1;
	setsockopt(srv, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&opt), sizeof(opt));

	struct sockaddr_in addr{};
	addr.sin_family = AF_INET;
	addr.sin_port = htons(static_cast<uint16_t>(port));
	addr.sin_addr.s_addr = inet_addr("127.0.0.1");

	if (bind(srv, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) != 0 ||
		listen(srv, 4) != 0) {
		std::fprintf(stderr, "[mock-db] bind/listen failed on 127.0.0.1:%d\n", port);
		CLOSESOCK(srv);
		return 1;
	}

	if (FILE* fp = std::fopen(ready_file, "w")) {
		std::fprintf(fp, "READY\n");
		std::fclose(fp);
	}

	fd_set rfds;
	FD_ZERO(&rfds);
	FD_SET(srv, &rfds);
	struct timeval tv{12, 0};
	if (select(static_cast<int>(srv + 1), &rfds, nullptr, nullptr, &tv) <= 0) {
		std::fprintf(stderr, "[mock-db] timeout waiting for game db_clientdesc connection\n");
		CLOSESOCK(srv);
		return 1;
	}

	struct sockaddr_in cli_addr{};
#ifdef _WIN32
	int cli_len = sizeof(cli_addr);
#else
	socklen_t cli_len = sizeof(cli_addr);
#endif
	sock_t cli = accept(srv, reinterpret_cast<struct sockaddr*>(&cli_addr), &cli_len);
	if (cli == INVALID_SOCKET) {
		std::fprintf(stderr, "[mock-db] accept() failed\n");
		CLOSESOCK(srv);
		return 1;
	}

	size_t total_bytes = 0;
	uint8_t first_header = 0;
	uint8_t buf[4096];

	// Wait up to 3 seconds for initial GD packets (HEADER_GD_BOOT, HEADER_GD_SETUP, HEADER_GD_UPDATE_CHANNELSTATUS)
	for (int iter = 0; iter < 30; ++iter) {
		FD_ZERO(&rfds);
		FD_SET(cli, &rfds);
		struct timeval rtv{0, 100000}; // 100ms
		int sel = select(static_cast<int>(cli + 1), &rfds, nullptr, nullptr, &rtv);
		if (sel > 0) {
			int n = recv(cli, reinterpret_cast<char*>(buf), sizeof(buf), 0);
			if (n <= 0)
				break;
			if (total_bytes == 0 && n > 0)
				first_header = buf[0];
			total_bytes += static_cast<size_t>(n);
		} else if (total_bytes > 0) {
			break;
		}
	}

	if (FILE* fp = std::fopen(out_file, "w")) {
		std::fprintf(fp, "CONNECTED=1\nBYTES=%zu\nFIRST_HEADER=%u\n",
			total_bytes, static_cast<unsigned>(first_header));
		std::fclose(fp);
	}

	// Keep the socket open until the game server closes it (max 10s)
	for (int iter = 0; iter < 100; ++iter) {
		FD_ZERO(&rfds);
		FD_SET(cli, &rfds);
		struct timeval rtv{0, 100000};
		int sel = select(static_cast<int>(cli + 1), &rfds, nullptr, nullptr, &rtv);
		if (sel > 0) {
			int n = recv(cli, reinterpret_cast<char*>(buf), sizeof(buf), 0);
			if (n <= 0)
				break;
		}
	}

	CLOSESOCK(cli);
	CLOSESOCK(srv);
	return (total_bytes > 0) ? 0 : 1;
}

static sock_t connect_with_retry(int port, int max_attempts) {
	for (int attempt = 0; attempt < max_attempts; ++attempt) {
		sock_t s = socket(AF_INET, SOCK_STREAM, 0);
		if (s == INVALID_SOCKET)
			return INVALID_SOCKET;

		struct sockaddr_in addr{};
		addr.sin_family = AF_INET;
		addr.sin_port = htons(static_cast<uint16_t>(port));
		addr.sin_addr.s_addr = inet_addr("127.0.0.1");

		if (connect(s, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) == 0)
			return s;

		CLOSESOCK(s);
		sleep_ms(100);
	}
	return INVALID_SOCKET;
}

static int run_probe_game(int port) {
	sock_t s = connect_with_retry(port, 60);
	if (s == INVALID_SOCKET) {
		std::fprintf(stderr, "[probe-game] FAIL: could not connect to 127.0.0.1:%d\n", port);
		return 1;
	}

	SmokePacketGCPhase phasePkt{};
	SmokePacketHandshake hsPkt{};
	uint8_t initBuf[15]{};
	if (!recv_exact_timeout(s, initBuf, sizeof(initBuf), 5)) {
		std::fprintf(stderr, "[probe-game] FAIL: timeout reading initial 15-byte phase+handshake packet\n");
		CLOSESOCK(s);
		return 1;
	}

	std::memcpy(&phasePkt, initBuf, sizeof(phasePkt));
	std::memcpy(&hsPkt, initBuf + sizeof(phasePkt), sizeof(hsPkt));

	if (phasePkt.header != 0xFD || phasePkt.phase != 1) {
		std::fprintf(stderr, "[probe-game] FAIL: expected TPacketGCPhase(0xFD, 1), got (0x%02X, %u)\n",
			phasePkt.header, phasePkt.phase);
		CLOSESOCK(s);
		return 1;
	}

	if (hsPkt.bHeader != 0xFF || hsPkt.dwHandshake == 0 || hsPkt.lDelta != 0) {
		std::fprintf(stderr, "[probe-game] FAIL: invalid TPacketGCHandshake header=0x%02X handshake=%u time=%u delta=%d\n",
			hsPkt.bHeader, hsPkt.dwHandshake, hsPkt.dwTime, hsPkt.lDelta);
		CLOSESOCK(s);
		return 1;
	}

	std::printf("  [PASS] Received 32-bit wire TPacketGCPhase(0xFD, PHASE_HANDSHAKE=1) + TPacketGCHandshake(0xFF, id=%u, time=%u, delta=%d)\n",
		hsPkt.dwHandshake, hsPkt.dwTime, hsPkt.lDelta);

	// Echo TPacketCGHandshake back to complete handshake and trigger PHASE_LOGIN transition
	SmokePacketHandshake reply = hsPkt;
	reply.lDelta = 0;
	if (send(s, reinterpret_cast<const char*>(&reply), sizeof(reply), 0) != sizeof(reply)) {
		std::fprintf(stderr, "[probe-game] FAIL: could not send TPacketCGHandshake reply\n");
		CLOSESOCK(s);
		return 1;
	}

	fd_set rfds;
	FD_ZERO(&rfds);
	FD_SET(s, &rfds);
	struct timeval tv{4, 0};
	uint8_t respBuf[64]{};
	int sel = select(static_cast<int>(s + 1), &rfds, nullptr, nullptr, &tv);
	int n = (sel > 0) ? recv(s, reinterpret_cast<char*>(respBuf), sizeof(respBuf), 0) : -1;
	if (n <= 0) {
		std::fprintf(stderr, "[probe-game] FAIL: no post-handshake PHASE_LOGIN response received (n=%d)\n", n);
		CLOSESOCK(s);
		return 1;
	}

	std::printf("  [PASS] Handshake reply accepted by game server; received %d post-handshake wire bytes (PHASE_LOGIN transition)\n", n);
	CLOSESOCK(s);
	return 0;
}

static int run_probe_db(int port) {
	sock_t s = connect_with_retry(port, 60);
	if (s == INVALID_SOCKET) {
		std::fprintf(stderr, "[probe-db] FAIL: could not connect to db_r41023 on 127.0.0.1:%d\n", port);
		return 1;
	}
	std::printf("  [PASS] Connected to live db_r41023 TCP listener on 127.0.0.1:%d\n", port);
	CLOSESOCK(s);
	return 0;
}

int main(int argc, char** argv) {
#ifdef _WIN32
	WSADATA wsa;
	WSAStartup(MAKEWORD(2, 2), &wsa);
#endif
	int rc = 1;
	if (argc >= 5 && std::strcmp(argv[1], "--mock-db") == 0) {
		rc = run_mock_db(std::atoi(argv[2]), argv[3], argv[4]);
	} else if (argc >= 3 && std::strcmp(argv[1], "--probe-game") == 0) {
		rc = run_probe_game(std::atoi(argv[2]));
	} else if (argc >= 3 && std::strcmp(argv[1], "--probe-db") == 0) {
		rc = run_probe_db(std::atoi(argv[2]));
	} else {
		std::fprintf(stderr, "Usage:\n"
			"  %s --mock-db <port> <ready_file> <out_file>\n"
			"  %s --probe-game <port>\n"
			"  %s --probe-db <port>\n", argv[0], argv[0], argv[0]);
	}
#ifdef _WIN32
	WSACleanup();
#endif
	return rc;
}
