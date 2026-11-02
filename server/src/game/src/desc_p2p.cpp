#include <net/mt2_asio_tls13_transport.h>
#include "stdafx.h"
#include "desc_p2p.h"
#include "protocol.h"
#include "p2p.h"
#include "../../common/CommonDefines.h"

DESC_P2P::~DESC_P2P()
{
}

void DESC_P2P::Destroy()
{
	if (m_sock == INVALID_SOCKET && !m_spTls13Session) {
		return;
	}

	P2P_MANAGER::instance().UnregisterAcceptor(this);

	if (m_sock != INVALID_SOCKET)
	{
		if (m_lpFdw)
			fdwatch_del_fd(m_lpFdw, m_sock);

		sys_log(0, "SYSTEM: closing p2p socket. DESC #%d", m_sock);

		socket_close(m_sock);
		m_sock = INVALID_SOCKET;
	}

	// Chain up to base class Destroy()
	DESC::Destroy();
}

#ifdef ENABLE_PORT_SECURITY
#include "config.h"
#endif
bool DESC_P2P::Setup(LPFDWATCH fdw, socket_t fd, const char * host, WORD wPort)
{
	m_lpFdw = fdw;
	m_stHost = host;
	m_wPort = wPort;
	m_sock = fd;

	if (!(m_lpOutputBuffer = buffer_new(1024 * 1024)))
		return false;

	if (!(m_lpInputBuffer = buffer_new(1024 * 1024)))
		return false;

	fdwatch_add_fd(m_lpFdw, m_sock, this, FDW_READ, false);

	m_iMinInputBufferLen = 1024 * 1024;
#ifdef ENABLE_PORT_SECURITY
	if (strcmp(host, g_szPublicIP)) // refuse if remote host != public ip (only the same machine must be able to connect in here)
	{
		sys_log(0, "SYSTEM: new p2p connection from [%s] to [%s] fd: %d BLOCKED", host, g_szPublicIP, m_sock);
		SetPhase(PHASE_CLOSE);
		return true;
	}
#endif
	SetPhase(PHASE_P2P);

	sys_log(0, "SYSTEM: new p2p connection from [%s] fd: %d", host, m_sock);
	return true;
}

bool DESC_P2P::SetupTls13(LPFDWATCH fdw, std::shared_ptr<mt2::net::AsioTls13Session> spSession)
{
	if (!spSession || !spSession->IsOpen() || !spSession->IsHandshakeComplete())
		return false;

	m_lpFdw = fdw;
	m_sock = INVALID_SOCKET;

	if (!(m_lpOutputBuffer = buffer_new(1024 * 1024)))
		return false;

	if (!(m_lpInputBuffer = buffer_new(1024 * 1024)))
		return false;

	m_iMinInputBufferLen = 1024 * 1024;
	BindTls13Session(std::move(spSession));

#ifdef ENABLE_PORT_SECURITY
	if (g_szPublicIP[0] != '\0' && strcmp(m_stHost.c_str(), g_szPublicIP) && strcmp(m_stHost.c_str(), "127.0.0.1"))
	{
		sys_log(0, "SYSTEM: new TLS 1.3 p2p connection from [%s] to [%s] BLOCKED", m_stHost.c_str(), g_szPublicIP);
		SetPhase(PHASE_CLOSE);
		return true;
	}
#endif
	SetPhase(PHASE_P2P);

	sys_log(0, "SYSTEM: new TLS 1.3 p2p connection from [%s:%u]", m_stHost.c_str(), m_wPort);
	return true;
}

void DESC_P2P::SetPhase(int iPhase)
{
	static CInputP2P s_inputP2P;

	switch (iPhase)
	{
		case PHASE_P2P:
			sys_log(1, "PHASE_P2P");

			if (m_lpInputBuffer)
				buffer_reset(m_lpInputBuffer);

			if (m_lpOutputBuffer)
				buffer_reset(m_lpOutputBuffer);

			m_pInputProcessor = &s_inputP2P;
			break;

		case PHASE_CLOSE:
			m_pInputProcessor = NULL;
			break;

		default:
			sys_err("DESC_P2P::SetPhase : Unknown phase");
			break;
	}

	m_iPhase = iPhase;
}
