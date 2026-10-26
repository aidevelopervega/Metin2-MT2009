#include "stdafx.h"
#include "Config.h"
#include "Peer.h"
#include "DBManager.h"
#include "ClientManager.h"
#include "GuildManager.h"
#include "ItemAwardManager.h"
#include "HB.h"
#include "PrivManager.h"
#include "MoneyLog.h"
#include "Marriage.h"
#include "Monarch.h"
#include "ItemIDRangeManager.h"
#include "../../libthecore/include/auth_security.h"
#include <signal.h>

#include "Main.h"

void SetPlayerDBName(const char* c_pszPlayerDBName);
void SetTablePostfix(const char* c_pszTablePostfix);
const char * GetTablePostfix();
int Start();

std::string g_stTablePostfix;
std::string g_stLocaleNameColumn = "name";
std::string g_stLocale = "latin2"; // default: euckr
std::string g_stPlayerDBName = "";

bool g_bHotBackup = false; // default: true
BOOL g_test_server = false;

int g_iPlayerCacheFlushSeconds = 60*7;
int g_iItemCacheFlushSeconds = 60*5;

int g_iLogoutSeconds = 60*10;

int g_log = 1;

// MYSHOP_PRICE_LIST
int g_iItemPriceListTableCacheFlushSeconds = 540;
// END_OF_MYSHOP_PRICE_LIST

#if defined(__FreeBSD__) && defined(__FreeBSD_version) && __FreeBSD_version<1000000
extern const char * _malloc_options;
#endif

extern void WriteVersion();

void emergency_sig(int sig)
{
	if (sig == SIGSEGV)
		sys_log(0, "SIGNAL: SIGSEGV");
	else if (sig == SIGUSR1)
		sys_log(0, "SIGNAL: SIGUSR1");

	if (sig == SIGSEGV)
		abort();
}

static int RunCreateAccountCLI(int argc, char** argv)
{
	if (argc < 3 || argc > 5)
	{
		fprintf(stderr, "Usage: %s --create-account <login> [social_id] [hwid] < password_stdin\n", argv[0]);
		return 2;
	}

	char szPlainPwd[128] = {0};
	if (!fgets(szPlainPwd, sizeof(szPlainPwd), stdin))
	{
		AuthSecurity::SecureZeroMemoryBuffer(szPlainPwd, sizeof(szPlainPwd));
		fprintf(stderr, "ERROR: Failed to read password from stdin.\n");
		return 2;
	}

	size_t pwdLen = strlen(szPlainPwd);
	while (pwdLen > 0 && (szPlainPwd[pwdLen - 1] == '\r' || szPlainPwd[pwdLen - 1] == '\n'))
	{
		szPlainPwd[--pwdLen] = '\0';
	}

	const char* loginArg = argv[2];
	const char* socialIdArg = (argc >= 4) ? argv[3] : "1234567";
	const char* hwidArg = (argc >= 5) ? argv[4] : "";

	if (hwidArg[0] != '\0')
	{
		const size_t hwidLen = strlen(hwidArg);
		if (hwidLen > 64)
		{
			AuthSecurity::SecureZeroMemoryBuffer(szPlainPwd, sizeof(szPlainPwd));
			fprintf(stderr, "ERROR: invalid hwid length (max 64 chars).\n");
			return 2;
		}
		for (size_t i = 0; i < hwidLen; ++i)
		{
			const unsigned char uc = static_cast<unsigned char>(hwidArg[i]);
			if (uc <= 0x20 || uc >= 0x7F || uc == '\'' || uc == '"' || uc == '\\')
			{
				AuthSecurity::SecureZeroMemoryBuffer(szPlainPwd, sizeof(szPlainPwd));
				fprintf(stderr, "ERROR: invalid character in hwid.\n");
				return 2;
			}
		}
	}

	AuthSecurity::SNewAccountCredentials creds;
	std::string prepError;
	const bool prepOk = AuthSecurity::PrepareNewAccountCredentials(
		loginArg,
		std::string_view(szPlainPwd, pwdLen),
		socialIdArg,
		&creds,
		&prepError);
	AuthSecurity::SecureZeroMemoryBuffer(szPlainPwd, sizeof(szPlainPwd));

	if (!prepOk)
	{
		fprintf(stderr, "ERROR: %s\n", prepError.c_str());
		return 2;
	}

	CConfig Config;
	if (!Config.LoadFile("conf.txt"))
	{
		fprintf(stderr, "ERROR: Loading conf.txt failed.\n");
		return 1;
	}

	char szPostfix[256 + 1] = {0};
	if (!Config.GetValue("TABLE_POSTFIX", szPostfix, 256))
		szPostfix[0] = '\0';
	SetTablePostfix(szPostfix);

	char line[256 + 1] = {0};
	if (!Config.GetValue("SQL_ACCOUNT", line, 256))
	{
		fprintf(stderr, "ERROR: SQL_ACCOUNT not configured in conf.txt.\n");
		return 1;
	}

	char szAddr[64] = {0}, szDB[64] = {0}, szUser[64] = {0}, szPassword[64] = {0};
	int iPort = 0;
	if (sscanf(line, " %63s %63s %63s %63s %d ", szAddr, szDB, szUser, szPassword, &iPort) != 5)
	{
		AuthSecurity::SecureZeroMemoryBuffer(szPassword, sizeof(szPassword));
		AuthSecurity::SecureZeroMemoryBuffer(line, sizeof(line));
		fprintf(stderr, "ERROR: Invalid SQL_ACCOUNT configuration format.\n");
		return 1;
	}

	CAsyncSQL sql;
	const bool bConnected = sql.Setup(szAddr, szUser, szPassword, szDB, "utf8mb4", true, iPort);
	AuthSecurity::SecureZeroMemoryBuffer(szPassword, sizeof(szPassword));
	AuthSecurity::SecureZeroMemoryBuffer(line, sizeof(line));

	if (!bConnected)
	{
		fprintf(stderr, "ERROR: Failed to connect to SQL_ACCOUNT database.\n");
		return 1;
	}

	char szSchemaQuery[512];
	snprintf(szSchemaQuery, sizeof(szSchemaQuery),
		"SELECT CHARACTER_MAXIMUM_LENGTH FROM information_schema.COLUMNS "
		"WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 'account%s' AND COLUMN_NAME = 'password'",
		GetTablePostfix());

	std::unique_ptr<SQLMsg> pkSchemaMsg = sql.DirectQuery(szSchemaQuery);
	unsigned long colMaxLen = 0;
	if (pkSchemaMsg && pkSchemaMsg->uiSQLErrno == 0 && pkSchemaMsg->Get() && pkSchemaMsg->Get()->uiNumRows > 0)
	{
		MYSQL_ROW row = mysql_fetch_row(pkSchemaMsg->Get()->pSQLResult);
		if (row && row[0])
			colMaxLen = strtoul(row[0], nullptr, 10);
	}

	if (colMaxLen < creds.argon2id_hash.size())
	{
		fprintf(stderr,
			"ERROR: account%s.password column capacity (%lu) is smaller than Argon2id hash length (%zu); VARCHAR(255) required.\n",
			GetTablePostfix(), colMaxLen, creds.argon2id_hash.size());
		return 1;
	}

	char szEscLogin[LOGIN_MAX_LEN * 2 + 1] = {0};
	char szEscHash[AuthSecurity::kMaxStoredHashLength * 2 + 1] = {0};
	char szEscSocialId[SOCIAL_ID_MAX_LEN * 2 + 1] = {0};

	sql.EscapeString(szEscLogin, sizeof(szEscLogin), creds.login.c_str(), creds.login.size());
	sql.EscapeString(szEscHash, sizeof(szEscHash), creds.argon2id_hash.c_str(), creds.argon2id_hash.size());
	sql.EscapeString(szEscSocialId, sizeof(szEscSocialId), creds.social_id.c_str(), creds.social_id.size());

	char szInsertQuery[1024];
	snprintf(szInsertQuery, sizeof(szInsertQuery),
		"INSERT INTO account%s (login, password, social_id, status, create_time) "
		"VALUES ('%s', '%s', '%s', 'OK', NOW())",
		GetTablePostfix(), szEscLogin, szEscHash, szEscSocialId);

	std::unique_ptr<SQLMsg> pkInsertMsg = sql.DirectQuery(szInsertQuery);
	AuthSecurity::SecureZeroMemoryBuffer(szInsertQuery, sizeof(szInsertQuery));
	AuthSecurity::SecureZeroMemoryBuffer(szEscHash, sizeof(szEscHash));

	if (!pkInsertMsg || pkInsertMsg->uiSQLErrno != 0 || !pkInsertMsg->Get() ||
	    pkInsertMsg->Get()->uiAffectedRows != 1 || pkInsertMsg->Get()->uiInsertID == 0)
	{
		unsigned int errCode = pkInsertMsg ? pkInsertMsg->uiSQLErrno : 0;
		fprintf(stderr, "ERROR: Failed to insert account '%s' (sql_errno=%u).\n", creds.login.c_str(), errCode);
		return 1;
	}

	const uint32_t accountId = pkInsertMsg->Get()->uiInsertID;

	if (hwidArg[0] != '\0')
	{
		char szEscHwid[128 + 1] = {0};
		sql.EscapeString(szEscHwid, sizeof(szEscHwid), hwidArg, strlen(hwidArg));
		char szHwidQuery[512];
		snprintf(szHwidQuery, sizeof(szHwidQuery),
			"INSERT INTO hardware_status (hwid, status) VALUES ('%s', 'OK') "
			"ON DUPLICATE KEY UPDATE status = status",
			szEscHwid);
		std::unique_ptr<SQLMsg> pkHwidMsg = sql.DirectQuery(szHwidQuery);
		if (!pkHwidMsg || pkHwidMsg->uiSQLErrno != 0)
		{
			fprintf(stderr, "ERROR: Failed to register hardware_status for account '%s'.\n", creds.login.c_str());
			return 1;
		}
	}

	printf("ACCOUNT_CREATED id=%u login=%s format=argon2id\n", accountId, creds.login.c_str());
	fflush(stdout);
	return 0;
}

int main(int argc, char** argv)
{
	if (argc >= 2 && strcmp(argv[1], "--create-account") == 0)
		return RunCreateAccountCLI(argc, argv);

	WriteVersion();

#if defined(__FreeBSD__) && defined(__FreeBSD_version) && __FreeBSD_version<1000000
	_malloc_options = "A";
#endif

	CConfig Config;
	CNetPoller poller;
	CDBManager DBManager;
	CClientManager ClientManager;
	PlayerHB player_hb;
	CGuildManager GuildManager;
	CPrivManager PrivManager;
	CMoneyLog MoneyLog;
	ItemAwardManager ItemAwardManager;
	marriage::CManager MarriageManager;
	CMonarch Monarch;
	CItemIDRangeManager ItemIDRangeManager;
	if (!Start())
		return 1;

	GuildManager.Initialize();
	MarriageManager.Initialize();
	ItemIDRangeManager.Build();
	sys_log(0, "Metin2DBCacheServer Start\n");

	CClientManager::instance().MainLoop();

	signal_timer_disable();

	DBManager.Quit();
	int iCount;

	while (1)
	{
		SQLMsg * tmp;
		while ((tmp = CDBManager::instance().PopResult()))
		{
			CClientManager::instance().AnalyzeQueryResult(tmp);
			delete tmp;
		}

		while ((tmp = CDBManager::instance().PopFailedQuery()))
		{
			CClientManager::instance().AnalyzeFailedQuery(tmp);
			delete tmp;
		}

		iCount = 0;

		iCount += static_cast<int>(CDBManager::instance().CountReturnQuery(SQL_PLAYER));
		iCount += static_cast<int>(CDBManager::instance().CountAsyncQuery(SQL_PLAYER));

		if (iCount == 0)
			break;

		usleep(1000);
		sys_log(0, "WAITING_QUERY_COUNT %d", iCount);
	}

	{
		SQLMsg * tmp;
		while ((tmp = CDBManager::instance().PopResult()))
		{
			CClientManager::instance().AnalyzeQueryResult(tmp);
			delete tmp;
		}

		while ((tmp = CDBManager::instance().PopFailedQuery()))
		{
			CClientManager::instance().AnalyzeFailedQuery(tmp);
			delete tmp;
		}
	}

	sys_log(0, "<shutdown> Metin2DBCacheServer Shutdown Complete.");
	thecore_destroy();
	return 0;
}

void emptybeat(LPHEART heart, int pulse)
{
	if (!(pulse % heart->passes_per_sec))
	{
	}
}

//

//
int Start()
{
	if (!CConfig::instance().LoadFile("conf.txt"))
	{
		fprintf(stderr, "Loading conf.txt failed.\n");
		return false;
	}

	if (!CConfig::instance().GetValue("TEST_SERVER", &g_test_server))
	{
		fprintf(stderr, "Real Server\n");
	}
	else
		fprintf(stderr, "Test Server\n");

	if (!CConfig::instance().GetValue("LOG", &g_log))
	{
		fprintf(stderr, "Log Off");
		g_log= 0;
	}
	else
	{
		g_log = 1;
		fprintf(stderr, "Log On");
	}

	int tmpValue;

	int heart_beat = 50;
	if (!CConfig::instance().GetValue("CLIENT_HEART_FPS", &heart_beat))
	{
		fprintf(stderr, "Cannot find CLIENT_HEART_FPS configuration.\n");
		return false;
	}

	log_set_expiration_days(3);

	if (CConfig::instance().GetValue("LOG_KEEP_DAYS", &tmpValue))
	{
		tmpValue = MINMAX(3, tmpValue, 30);
		log_set_expiration_days(static_cast<unsigned int>(tmpValue));
		fprintf(stderr, "Setting log keeping days to %d\n", tmpValue);
	}

	thecore_init(heart_beat, emptybeat);
	signal_timer_enable(60);

	char szBuf[256+1];

	if (CConfig::instance().GetValue("LOCALE", szBuf, 256))
	{
		g_stLocale = szBuf;
		sys_log(0, "LOCALE set to %s", g_stLocale.c_str());

		// CHINA_DISABLE_HOTBACKUP
		if ("gb2312" == g_stLocale)
		{
			sys_log(0, "CIBN_LOCALE: DISABLE_HOTBACKUP");
			g_bHotBackup = false;
		}
		// END_OF_CHINA_DISABLE_HOTBACKUP
	}

	int iDisableHotBackup;
	if (CConfig::instance().GetValue("DISABLE_HOTBACKUP", &iDisableHotBackup))
	{
		if (iDisableHotBackup)
		{
			sys_log(0, "CONFIG: DISABLE_HOTBACKUP");
			g_bHotBackup = false;
		}
	}

	if (!CConfig::instance().GetValue("TABLE_POSTFIX", szBuf, 256))
	{
		sys_log(0, "TABLE_POSTFIX not configured use default"); // @warme012
		szBuf[0] = '\0';
	}

	SetTablePostfix(szBuf);

	if (CConfig::instance().GetValue("PLAYER_CACHE_FLUSH_SECONDS", szBuf, 256))
	{
		str_to_number(g_iPlayerCacheFlushSeconds, szBuf);
		sys_log(0, "PLAYER_CACHE_FLUSH_SECONDS: %d", g_iPlayerCacheFlushSeconds);
	}

	if (CConfig::instance().GetValue("ITEM_CACHE_FLUSH_SECONDS", szBuf, 256))
	{
		str_to_number(g_iItemCacheFlushSeconds, szBuf);
		sys_log(0, "ITEM_CACHE_FLUSH_SECONDS: %d", g_iItemCacheFlushSeconds);
	}

	// MYSHOP_PRICE_LIST
	if (CConfig::instance().GetValue("ITEM_PRICELIST_CACHE_FLUSH_SECONDS", szBuf, 256))
	{
		str_to_number(g_iItemPriceListTableCacheFlushSeconds, szBuf);
		sys_log(0, "ITEM_PRICELIST_CACHE_FLUSH_SECONDS: %d", g_iItemPriceListTableCacheFlushSeconds);
	}
	// END_OF_MYSHOP_PRICE_LIST

	if (CConfig::instance().GetValue("CACHE_FLUSH_LIMIT_PER_SECOND", szBuf, 256))
	{
		DWORD dwVal = 0; str_to_number(dwVal, szBuf);
		CClientManager::instance().SetCacheFlushCountLimit(static_cast<int>(dwVal));
	}

	int iIDStart;
	if (!CConfig::instance().GetValue("PLAYER_ID_START", &iIDStart))
	{
		sys_err("PLAYER_ID_START not configured");
		return false;
	}

	CClientManager::instance().SetPlayerIDStart(iIDStart);

	if (CConfig::instance().GetValue("NAME_COLUMN", szBuf, 256))
	{
		fprintf(stderr, "%s %s", g_stLocaleNameColumn.c_str(), szBuf);
		g_stLocaleNameColumn = szBuf;
	}

	char szAddr[64], szDB[64], szUser[64], szPassword[64];
	int iPort;
	char line[256+1];

	if (CConfig::instance().GetValue("SQL_PLAYER", line, 256))
	{
		sscanf(line, " %s %s %s %s %d ", szAddr, szDB, szUser, szPassword, &iPort);
		sys_log(0, "connecting to MySQL server (player)");

		int iRetry = 5;
		bool bConnected = false;

		do
		{
			if (CDBManager::instance().Connect(SQL_PLAYER, szAddr, iPort, szDB, szUser, szPassword))
			{
				sys_log(0, "   OK");
				bConnected = true;
				break;
			}

			if (thecore_is_shutdowned())
				return false;

			sys_log(0, "   failed, retrying in 5 seconds");
			fprintf(stderr, "   failed, retrying in 5 seconds");
			sleep(5);
		} while (iRetry-- && !thecore_is_shutdowned());
		if (!bConnected)
			return false;
		fprintf(stderr, "Success PLAYER\n");
		SetPlayerDBName(szDB);
	}
	else
	{
		sys_err("SQL_PLAYER not configured");
		return false;
	}

	if (CConfig::instance().GetValue("SQL_ACCOUNT", line, 256))
	{
		sscanf(line, " %s %s %s %s %d ", szAddr, szDB, szUser, szPassword, &iPort);
		sys_log(0, "connecting to MySQL server (account)");

		int iRetry = 5;
		bool bConnected = false;

		do
		{
			if (CDBManager::instance().Connect(SQL_ACCOUNT, szAddr, iPort, szDB, szUser, szPassword))
			{
				sys_log(0, "   OK");
				bConnected = true;
				break;
			}

			if (thecore_is_shutdowned())
				return false;

			sys_log(0, "   failed, retrying in 5 seconds");
			fprintf(stderr, "   failed, retrying in 5 seconds");
			sleep(5);
		} while (iRetry-- && !thecore_is_shutdowned());
		if (!bConnected)
			return false;
		fprintf(stderr, "Success ACCOUNT\n");
	}
	else
	{
		sys_err("SQL_ACCOUNT not configured");
		return false;
	}

	if (CConfig::instance().GetValue("SQL_COMMON", line, 256))
	{
		sscanf(line, " %s %s %s %s %d ", szAddr, szDB, szUser, szPassword, &iPort);
		sys_log(0, "connecting to MySQL server (common)");

		int iRetry = 5;
		bool bConnected = false;

		do
		{
			if (CDBManager::instance().Connect(SQL_COMMON, szAddr, iPort, szDB, szUser, szPassword))
			{
				sys_log(0, "   OK");
				bConnected = true;
				break;
			}

			if (thecore_is_shutdowned())
				return false;

			sys_log(0, "   failed, retrying in 5 seconds");
			fprintf(stderr, "   failed, retrying in 5 seconds");
			sleep(5);
		} while (iRetry-- && !thecore_is_shutdowned());
		if (!bConnected)
			return false;
		fprintf(stderr, "Success COMMON\n");
	}
	else
	{
		sys_err("SQL_COMMON not configured");
		return false;
	}

	if (CConfig::instance().GetValue("SQL_HOTBACKUP", line, 256))
	{
		sscanf(line, " %s %s %s %s %d ", szAddr, szDB, szUser, szPassword, &iPort);
		sys_log(0, "connecting to MySQL server (hotbackup)");

		int iRetry = 5;
		bool bConnected = false;

		do
		{
			if (CDBManager::instance().Connect(SQL_HOTBACKUP, szAddr, iPort, szDB, szUser, szPassword))
			{
				sys_log(0, "   OK");
				bConnected = true;
				break;
			}

			if (thecore_is_shutdowned())
				return false;

			sys_log(0, "   failed, retrying in 5 seconds");
			fprintf(stderr, "   failed, retrying in 5 seconds");
			sleep(5);
		}
		while (iRetry-- && !thecore_is_shutdowned());
		if (!bConnected)
			return false;

		fprintf(stderr, "Success HOTBACKUP\n");
	}
	else
	{
		sys_err("SQL_HOTBACKUP not configured");
		return false;
	}


#ifdef ENABLE_DB_SQL_LOG
	if (CConfig::instance().GetValue("SQL_LOG", line, 256))
	{
		sscanf(line, " %s %s %s %s %d ", szAddr, szDB, szUser, szPassword, &iPort);
		sys_log(0, "connecting to MySQL server (log)");

		int iRetry = 5;
		bool bConnected = false;

		do
		{
			if (CDBManager::instance().Connect(SQL_LOG, szAddr, iPort, szDB, szUser, szPassword))
			{
				sys_log(0, "   OK");
				bConnected = true;
				break;
			}

			if (thecore_is_shutdowned())
				return false;

			sys_log(0, "   failed, retrying in 5 seconds");
			fprintf(stderr, "   failed, retrying in 5 seconds");
			sleep(5);
		} while (iRetry-- && !thecore_is_shutdowned());
		if (!bConnected)
			return false;

		fprintf(stderr, "Success LOG\n");
	}
	else
	{
		sys_err("SQL_LOG not configured");
		return false;
	}
#endif

	if (CConfig::instance().GetValue("SQL_WORLD", line, 256))
	{
		sscanf(line, " %s %s %s %s %d ", szAddr, szDB, szUser, szPassword, &iPort);
		sys_log(0, "connecting to MySQL server (world)");

		int iRetry = 5;
		bool bConnected = false;

		do
		{
			if (CDBManager::instance().Connect(SQL_WORLD, szAddr, iPort, szDB, szUser, szPassword))
			{
				sys_log(0, "   OK");
				bConnected = true;
				break;
			}

			if (thecore_is_shutdowned())
				return false;

			sys_log(0, "   failed, retrying in 5 seconds");
			fprintf(stderr, "   failed, retrying in 5 seconds");
			sleep(5);
		} while (iRetry-- && !thecore_is_shutdowned());
		if (!bConnected)
			return false;
		fprintf(stderr, "Success WORLD\n");
	}
	else
	{
		sys_err("SQL_WORLD not configured");
		return false;
	}

	AuthSecurity::SecureZeroMemoryBuffer(szPassword, sizeof(szPassword));
	AuthSecurity::SecureZeroMemoryBuffer(line, sizeof(line));

	if (!CNetPoller::instance().Create())
	{
		sys_err("Cannot create network poller");
		return false;
	}

	sys_log(0, "ClientManager initialization.. ");

	if (!CClientManager::instance().Initialize())
	{
		sys_log(0, "   failed");
		return false;
	}

	sys_log(0, "   OK");

	if (!PlayerHB::instance().Initialize())
	{
		sys_err("cannot initialize player hotbackup");
		return false;
	}

#ifndef __WIN32__
	signal(SIGUSR1, emergency_sig);
#endif
	signal(SIGSEGV, emergency_sig);
	return true;
}

void SetTablePostfix(const char* c_pszTablePostfix)
{
	if (!c_pszTablePostfix || !*c_pszTablePostfix)
	{
		g_stTablePostfix = "";
		return;
	}

	while (*c_pszTablePostfix == ' ' || *c_pszTablePostfix == '\t' || *c_pszTablePostfix == '\r' || *c_pszTablePostfix == '\n')
		++c_pszTablePostfix;

	g_stTablePostfix = c_pszTablePostfix;
	while (!g_stTablePostfix.empty() &&
	       (g_stTablePostfix.back() == ' ' || g_stTablePostfix.back() == '\t' || g_stTablePostfix.back() == '\r' || g_stTablePostfix.back() == '\n'))
	{
		g_stTablePostfix.pop_back();
	}
}

const char * GetTablePostfix()
{
	return g_stTablePostfix.c_str();
}

void SetPlayerDBName(const char* c_pszPlayerDBName)
{
	if (! c_pszPlayerDBName || ! *c_pszPlayerDBName)
		g_stPlayerDBName = "";
	else
	{
		g_stPlayerDBName = c_pszPlayerDBName;
		g_stPlayerDBName += ".";
	}
}

const char * GetPlayerDBName()
{
	return g_stPlayerDBName.c_str();
}

