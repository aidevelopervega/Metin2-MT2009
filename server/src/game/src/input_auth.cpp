#include "stdafx.h"
#include "constants.h"
#include "config.h"
#include "input.h"
#include "desc_client.h"
#include "desc_manager.h"
#include "protocol.h"
#include "locale_service.h"
#include "db.h"
#include "Hardware.h"

#ifndef __WIN32__
	#include "limit_time.h"
#endif

#include "utils.h"
#include "../../libthecore/include/auth_security.h"

#include "../../common/PulseManager.h"

// #define ENABLE_ACCOUNT_W_SPECIALCHARS
bool FN_IS_VALID_LOGIN_STRING(const char *str)
{
	const char*	tmp;

	if (!str || !*str)
		return false;

	size_t len = strlen(str);
	if (len < 2 || len > LOGIN_MAX_LEN)
		return false;

	for (tmp = str; *tmp; ++tmp)
	{
		if (isdigit(static_cast<unsigned char>(*tmp)) || isalpha(static_cast<unsigned char>(*tmp)) || *tmp == '_')
			continue;

#ifdef ENABLE_ACCOUNT_W_SPECIALCHARS

		switch (*tmp)
		{
			case ' ':
			case '_':
			case '-':
			case '.':
			case '!':
			case '@':
			case '#':
			case '$':
			case '%':
			case '^':
			case '&':
			case '*':
			case '(':
			case ')':
				continue;
		}
#endif
		return false;
	}

	return true;
}

bool Login_IsInChannelService(const char* c_login)
{
	if (c_login[0] == '[')
		return true;
	return false;
}

CInputAuth::CInputAuth()
{
}

std::string GetAuthLoginQuery(const char* c_pszEscapedLogin, const char* c_pszEscapedHardware)
{
	char szQuery[1024];
	const char* query = "SELECT "
	    "ac.password,ac.social_id,ac.id,ac.status,hw.status,ac.availDt - NOW() > 0,"
	    "UNIX_TIMESTAMP(ac.premium_expire),"
	    "UNIX_TIMESTAMP(ac.silver_expire),"
	    "UNIX_TIMESTAMP(ac.gold_expire),"
	    "UNIX_TIMESTAMP(ac.safebox_expire),"
	    "UNIX_TIMESTAMP(ac.autoloot_expire),"
	    "UNIX_TIMESTAMP(ac.fish_mind_expire),"
	    "UNIX_TIMESTAMP(ac.marriage_fast_expire),"
	    "UNIX_TIMESTAMP(ac.money_drop_rate_expire),"
	    "UNIX_TIMESTAMP(ac.shop_expire),"
	    "UNIX_TIMESTAMP(ac.create_time) "
	    "FROM account AS ac "
	    "LEFT JOIN hardware_status AS hw ON hw.hwid='%s' "
	    "WHERE ac.login = '%s'";

	snprintf(szQuery, sizeof(szQuery), query, c_pszEscapedHardware, c_pszEscapedLogin);
	return szQuery;
}

void CInputAuth::CheckClientVersion(LPDESC d, const char* c_pData)
{
	TPacketCGCheckVersion* pinfo = (TPacketCGCheckVersion*)c_pData;
	d->SetClientVersion(pinfo->dwVersion);
	return;
}

void CInputAuth::Login(LPDESC d, const char * c_pData)
{
	TPacketCGLogin3 * pinfo = (TPacketCGLogin3 *) c_pData;

	if (!g_bAuthServer)
	{
		sys_err ("CInputAuth class is not for game server. IP %s might be a hacker.",
			inet_ntoa(d->GetAddr().sin_addr));
		d->DelayedDisconnect(5);
		return;
	}

	if (d->GetClientVersion() < g_dwClientVersion)
	{
		LoginFailure(d, "UPDATE");
		return;
	}

	// Strictly verify NUL termination within fixed packet buffers before string operations
	if (!AuthSecurity::IsFieldNullTerminated(pinfo->login, sizeof(pinfo->login), 2, LOGIN_MAX_LEN) ||
	    !AuthSecurity::IsFieldNullTerminated(pinfo->passwd, sizeof(pinfo->passwd), 1, PASSWD_MAX_LEN) ||
	    !AuthSecurity::IsFieldNullTerminated(pinfo->hwid, sizeof(pinfo->hwid), 1, HWID_MAX_LEN))
	{
		AuthSecurity::AuthRateLimiter::Instance().RecordFailure(d->GetHostName(), "");
		LoginFailure(d, "WRONGPWD");
		return;
	}

	char login[LOGIN_MAX_LEN + 1];
	trim_and_lower(pinfo->login, login, sizeof(login));

	// Check login string format before rate limiter & SQL
	if (false == FN_IS_VALID_LOGIN_STRING(login))
	{
		sys_log(0, "InputAuth::Login : IS_NOT_VALID_LOGIN_STRING desc %p", get_pointer(d));
		AuthSecurity::AuthRateLimiter::Instance().RecordFailure(d->GetHostName(), "");
		LoginFailure(d, "WRONGPWD");
		return;
	}

	// Enforce independent IP and Account rate limiting + temporary exponential backoff
	const AuthSecurity::SRateLimitStatus rateStatus =
		AuthSecurity::AuthRateLimiter::Instance().CheckAllowed(d->GetHostName(), login);
	if (rateStatus.decision != AuthSecurity::ERateLimitDecision::Allowed)
	{
		sys_log(0, "InputAuth::Login : rate limited (%u) login(%s) ip(%s) retry_ms(%llu)",
				static_cast<unsigned>(rateStatus.decision),
				login,
				d->GetHostName(),
				static_cast<unsigned long long>(rateStatus.retry_after_ms));
		LoginFailure(d, "FAILURE");
		return;
	}

	// Decode HWID
	char szHardware[HWID_MAX_LEN + 1];
	std::string sDecodedHWID = CheckHardware::instance().DecodeHWID(pinfo->hwid);
	if (!sDecodedHWID.size() || sDecodedHWID[0] == '\0')
	{
		LoginFailure(d, "BLOCK");
		return;
	}
	strlcpy(szHardware, sDecodedHWID.c_str(), sizeof(szHardware));

	sys_log(0, "InputAuth::Login : %s(%zu) desc %p",
			login, strlen(login), get_pointer(d));

	if (g_bNoMoreClient)
	{
		TPacketGCLoginFailure failurePacket;

		failurePacket.header = HEADER_GC_LOGIN_FAILURE;
		strlcpy(failurePacket.szStatus, "SHUTDOWN", sizeof(failurePacket.szStatus));

		d->Packet(&failurePacket, sizeof(failurePacket));
		return;
	}

	if (DESC_MANAGER::instance().FindByLoginName(login))
	{
		LoginFailure(d, "ALREADY");
		return;
	}

	if (!AuthSecurity::TryAcquireInFlightAuthSlot())
	{
		sys_err("InputAuth::Login : max in-flight auth queries (%u) reached, throttling %s",
				AuthSecurity::kMaxInFlightAuthQueries, d->GetHostName());
		LoginFailure(d, "FAILURE");
		return;
	}

	DWORD dwKey = DESC_MANAGER::instance().CreateLoginKey(d);
	DWORD dwPanamaKey = dwKey ^ pinfo->adwClientKey[0] ^ pinfo->adwClientKey[1] ^ pinfo->adwClientKey[2] ^ pinfo->adwClientKey[3];
	d->SetPanamaKey(dwPanamaKey);

	sys_log(0, "InputAuth::Login : login %s key ***MASKED***", login);

	SAuthLoginQueryInfo * pAuthInfo = M2_NEW SAuthLoginQueryInfo;
	thecore_memcpy(&pAuthInfo->packet, pinfo, sizeof(TPacketCGLogin3));
	strlcpy(pAuthInfo->szNormalizedLogin, login, sizeof(pAuthInfo->szNormalizedLogin));
	strlcpy(pAuthInfo->szDecodedHWID, szHardware, sizeof(pAuthInfo->szDecodedHWID));
	strlcpy(pAuthInfo->szHostIP, d->GetHostName(), sizeof(pAuthInfo->szHostIP));
	pAuthInfo->bAcquiredInFlightSlot = true;

	char szEscapedLogin[LOGIN_MAX_LEN * 2 + 1];
	DBManager::instance().EscapeString(szEscapedLogin, sizeof(szEscapedLogin), login, strlen(login));

	char szEscapedHWID[HWID_MAX_LEN * 2 + 1];
	DBManager::instance().EscapeString(szEscapedHWID, sizeof(szEscapedHWID), szHardware, strlen(szHardware));

	// CHANNEL_SERVICE_LOGIN
	if (Login_IsInChannelService(szEscapedLogin))
	{
		sys_log(0, "ChannelServiceLogin [%s]", szEscapedLogin);

		DBManager::instance().ReturnQuery(QID_AUTH_LOGIN, dwKey, pAuthInfo,
				GetAuthLoginQuery(szEscapedLogin, szEscapedHWID).c_str());
	}
	// END_OF_CHANNEL_SERVICE_LOGIN
	else
	{
		DBManager::instance().ReturnQuery(QID_AUTH_LOGIN, dwKey, pAuthInfo,
			GetAuthLoginQuery(szEscapedLogin, szEscapedHWID).c_str());
	}
}

int CInputAuth::Analyze(LPDESC d, BYTE bHeader, const char * c_pData)
{
	if (!g_bAuthServer)
	{
		sys_err ("CInputAuth class is not for game server. IP %s might be a hacker.",
			inet_ntoa(d->GetAddr().sin_addr));
		d->DelayedDisconnect(5);
		return 0;
	}

	if (d && !PulseManager::Instance().IncreaseCount(inet_addr(d->GetHostName()), ePulse::AnalyzeAuth, std::chrono::milliseconds(1000), 7))
	{
		d->DelayedDisconnect(1);
		return 0;
	}

	int iExtraLen = 0;

	if (test_server)
		sys_log(0, " InputAuth Analyze Header[%d] ", bHeader);

	switch (bHeader)
	{
		case HEADER_CG_PONG:
			Pong(d);
			break;

		case HEADER_CG_LOGIN3:
			Login(d, c_pData);
			break;

		case HEADER_CG_CHECK_VERSION:
			CheckClientVersion(d, c_pData);
			break;

		case HEADER_CG_HANDSHAKE:
			break;

		default:
			sys_err("This phase does not handle this header %d (0x%x)(phase: AUTH)", bHeader, bHeader);
			break;
	}

	return iExtraLen;
}
//martysama0134's 4e4e75d8b719b9240e033009cf4d7b0f

// Files shared by GameCore.top
