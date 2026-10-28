#ifndef __INC_LOG_MANAGER_H__
#define __INC_LOG_MANAGER_H__

#if __has_include("../../../../server/src/libsql/AsyncSQL.h")
#include "../../../../server/src/libsql/AsyncSQL.h"
#elif __has_include("../../libsql/AsyncSQL.h")
#include "../../libsql/AsyncSQL.h"
#else
#include <AsyncSQL.h>
#endif
#include "any_function.h"
#include "locale_service.h"

// #ifdef ENABLE_NEWSTUFF
enum log_level {LOG_LEVEL_NONE=0, LOG_LEVEL_MIN=1, LOG_LEVEL_MID=2, LOG_LEVEL_MAX=3};
// #endif

#ifdef ENABLE_NEWSTUFF
#define LOG_LEVEL_CHECK_N_RET(x) { if (g_iDbLogLevel < x) return; }
#else
#define LOG_LEVEL_CHECK_N_RET(x) { }
#endif

#ifdef ENABLE_NEWSTUFF
#define LOG_LEVEL_CHECK(x, fnc)	\
	{\
		if (g_iDbLogLevel >= (x))\
			fnc;\
	}
#else
#define LOG_LEVEL_CHECK(x, fnc)	{ fnc; }
#endif

enum GOLDBAR_HOW
{
	PERSONAL_SHOP_BUY	= 1 ,
	PERSONAL_SHOP_SELL	= 2 ,
	SHOP_BUY			= 3 ,
	SHOP_SELL			= 4 ,
	EXCHANGE_TAKE		= 5 ,
	EXCHANGE_GIVE		= 6 ,
	QUEST				= 7 ,
};

class LogManager : public singleton<LogManager>
{
	public:
		LogManager();
		virtual ~LogManager();

		bool		IsConnected();

		bool		Connect(const char * host, const int port, const char * user, const char * pwd, const char * db);

		void		ItemLog(uint64_t dwPID, DWORD x, DWORD y, uint64_t dwItemID, const char * c_pszText, const char * c_pszHint, const char * c_pszIP, DWORD dwVnum);
		void		ItemLog(LPCHARACTER ch, LPITEM item, const char * c_pszText, const char * c_pszHint, bool addItemMetaData=false);
		void		ItemLog(LPCHARACTER ch, uint64_t itemID, int itemVnum, const char * c_pszText, const char * c_pszHint);
		void		ItemSocketLog(uint64_t who, uint64_t itemID, DWORD itemVnum, BYTE socket, int value, const char* what="SOCKET");
		void		ItemAttrLog(uint64_t who, uint64_t itemID, DWORD itemVnum, BYTE type, int value, const char* what="ATTR");
		void		ItemMetaLog(uint64_t who, uint64_t itemID, DWORD itemVnum, const char* value, const char* what);

		void		CharLog(uint64_t dwPID, DWORD x, DWORD y, long long dw, const char * c_pszText, const char * c_pszHint, const char * c_pszIP);
		void		CharLog(LPCHARACTER ch, long long dw, const char * c_pszText, const char * c_pszHint);

		void		LoginLog(bool isLogin, LPCHARACTER ch);
		void		MoneyLog(BYTE type, DWORD vnum, YANG gold);
		void		ExchangeLog(const char* type, LPCHARACTER who, uint64_t targetPid, DWORD vnum, ITEM_COUNT count, uint64_t itemid);
		void		ExchangeGoldLog(const char* type, LPCHARACTER who, uint64_t targetPid, YANG gold);
		void		HackLog(const char * c_pszHackName, const char * c_pszLogin, const char * c_pszName, const char * c_pszIP);
		void		HackLog(const char * c_pszHackName, LPCHARACTER ch);
		void		RequestInfoLog(uint64_t pid, BYTE type, const char * c_pszArg1, const char* c_pszArg2);
		void		GoldBarLog(uint64_t dwPID, uint64_t dwItemID, GOLDBAR_HOW eHow, const char * c_pszHint);
		void		CubeLog(uint64_t dwPID, DWORD x, DWORD y, DWORD item_vnum, uint64_t item_uid, int item_count, bool success);
		void		GMCommandLog(uint64_t dwPID, const char * szName, const char * szIP, BYTE byChannel, const char * szCommand);
		void		SpeedHackLog(uint64_t pid, DWORD x, DWORD y, int hack_count);
		void		ChangeNameLog(uint64_t pid, const char * old_name, const char * new_name, const char * ip);
		void		RefineLog(uint64_t pid, const char * item_name, uint64_t item_id, int item_refine_level, int is_success, const char * how);
		void		LevelLog(LPCHARACTER pChar, unsigned int level, unsigned int playhour);
		void		BootLog(const char * c_pszHostName, BYTE bChannel);
		void		QuestRewardLog(const char * c_pszQuestName, uint64_t dwPID, DWORD dwLevel, long long llValue1, int iValue2, BYTE type);
		void		QuestStateLog(const char * c_pszQuestName, const char* c_pszQuestState, uint64_t dwPID);
		void		DetailLoginLog(bool isLogin, LPCHARACTER ch);
		void		DetailLoginLog(bool isLogin, LPDESC d, uint64_t pid);
		void		DragonSlayLog(uint64_t dwGuildID, DWORD dwDragonVnum, DWORD dwStartTime, DWORD dwEndTime);
		void		ChatLog(DWORD where, uint64_t who_id, const char* who_name, uint64_t whom_id, const char* whom_name, const char* type, const char* msg, const char* ip);
		void		CaptchaLog(LPCHARACTER ch, const char* action, const char* code);
		void		VoucherCodeLog(LPCHARACTER ch, const char* code);
		void		GoldSessionLog(LPCHARACTER ch);
		void		ReportPlayerLog(LPCHARACTER who, LPCHARACTER target, BYTE reason);
		void		FishLog(uint64_t playerId, DWORD itemVnum, ITEM_COUNT count, int rodLevel, int baitVnum);
#ifdef ENABLE_ACCE_COSTUME_SYSTEM
		void		AcceLog(uint64_t dwPID, DWORD x, DWORD y, DWORD item_vnum, uint64_t item_uid, int item_count, int abs_chance, bool success);
#endif
		void		Query(const char* c_pszFormat, ...);

		size_t		EscapeString(char* dst, size_t dstSize, const char *src, size_t srcSize);

		void		ConfigureDurableSpool(const char * c_pszSpoolPath, size_t maxMemoryQueueCount = 10000, size_t maxSpoolBytes = 64 * 1024 * 1024, DWORD dwReconnectIntervalMs = 1000);
		void		SetFailStopOnSpoolOverflow(bool bEnable);
		bool		IsFailStopOnSpoolOverflow() const { return m_sql.GetSpoolOverflowPolicy() == CAsyncSQL::SPOOL_OVERFLOW_FAIL_STOP_SHUTDOWN; }
		bool		HasSpoolOverflowAlert() const { return m_sql.HasFailedQueryOverflow(); }
		void		FlushAndShutdown(DWORD dwMaxWaitMs = 3000);
		const std::string & GetSpoolFilePath() const { return m_sql.GetSpoolFilePath(); }
		CAsyncSQL::TDurableSpoolStats GetDurableSpoolStats() { return m_sql.GetDurableSpoolStats(); }

	private:

		CAsyncSQL	m_sql;
		bool		m_bIsConnect;
		std::string	m_stSpoolPath{"sql_log_spool.dat"};
		size_t		m_maxMemoryQueueCount{10000};
		size_t		m_maxSpoolBytes{64 * 1024 * 1024};
		DWORD		m_dwReconnectIntervalMs{1000};
};

#endif
