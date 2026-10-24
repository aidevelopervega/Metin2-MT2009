#ifndef __INC_METIN_II_ASYNCSQL_H__
#define __INC_METIN_II_ASYNCSQL_H__

#if __has_include("../../../../server/src/libthecore/include/stdafx.h")
#include "../../../../server/src/libthecore/include/stdafx.h"
#elif __has_include("../libthecore/include/stdafx.h")
#include "../libthecore/include/stdafx.h"
#else
#include <stdafx.h>
#endif

#if __has_include("../../../../server/src/libthecore/include/log.h")
#include "../../../../server/src/libthecore/include/log.h"
#elif __has_include("../libthecore/include/log.h")
#include "../libthecore/include/log.h"
#else
#include <log.h>
#endif

#include <string>
#include <queue>
#include <deque>
#include <vector>
#include <map>
#include <cstdint>
#include <atomic>
#include <thread>
#include <mysql.h>
#include <errmsg.h>
#include <mysqld_error.h>
#include <memory>

#include "Semaphore.h"

#define QUERY_MAX_LEN 8192

typedef struct _SQLResult
{
	_SQLResult()
	   	: pSQLResult(NULL), uiNumRows(0), uiAffectedRows(0), uiInsertID(0),
	   	  ullNumRows(0), ullAffectedRows(0), ullInsertID(0)
	{
	}

	~_SQLResult()
	{
		if (pSQLResult)
		{
			mysql_free_result(pSQLResult);
			pSQLResult = NULL;
		}
	}

	bool		HasValidInsertID() const { return ullInsertID != 0 && ullInsertID <= UINT32_MAX; }
	bool		HasValidAffectedRows() const { return ullAffectedRows != static_cast<my_ulonglong>(-1) && ullAffectedRows <= UINT32_MAX; }
	bool		HasValidNumRows() const { return ullNumRows <= UINT32_MAX; }
	bool		IsOverflow() const { return ullNumRows > UINT32_MAX || (ullAffectedRows != static_cast<my_ulonglong>(-1) && ullAffectedRows > UINT32_MAX) || ullInsertID > UINT32_MAX; }

	MYSQL_RES *	pSQLResult;
	uint32_t		uiNumRows;
	uint32_t		uiAffectedRows;
	uint32_t		uiInsertID;
	uint64_t		ullNumRows;
	uint64_t		ullAffectedRows;
	uint64_t		ullInsertID;
} SQLResult;

typedef struct _SQLMsg
{
	_SQLMsg() : m_pkSQL(NULL), iID(0), uiResultPos(0), pvUserData(NULL), bReturn(false), uiSQLErrno(0), ullSeqID(0)
	{
	}

	bool		IsError() const { return uiSQLErrno != 0; }
	bool		IsEmpty() const { return uiSQLErrno == 0 && (vec_pkResult.empty() || (vec_pkResult[0] && vec_pkResult[0]->ullNumRows == 0)); }

	~_SQLMsg()
	{
		std::vector<SQLResult *>::iterator first = vec_pkResult.begin();
		std::vector<SQLResult *>::iterator past = vec_pkResult.end();

		while (first != past)
			delete *(first++);

		vec_pkResult.clear();
	}

	void Store()
	{
		do
		{
			SQLResult * pRes = new SQLResult;

			pRes->pSQLResult = mysql_store_result(m_pkSQL);
			pRes->ullInsertID = mysql_insert_id(m_pkSQL);
			pRes->ullAffectedRows = mysql_affected_rows(m_pkSQL);

			if (pRes->ullInsertID > UINT32_MAX)
			{
				pRes->uiInsertID = 0;
				if (uiSQLErrno == 0)
					uiSQLErrno = 1264;
			}
			else
			{
				pRes->uiInsertID = static_cast<uint32_t>(pRes->ullInsertID);
			}

			if (pRes->ullAffectedRows == static_cast<my_ulonglong>(-1))
			{
				pRes->uiAffectedRows = static_cast<uint32_t>(-1);
			}
			else if (pRes->ullAffectedRows > static_cast<uint64_t>(UINT32_MAX))
			{
				pRes->uiAffectedRows = static_cast<uint32_t>(-1);
				if (uiSQLErrno == 0)
					uiSQLErrno = 1264;
			}
			else
			{
				pRes->uiAffectedRows = static_cast<uint32_t>(pRes->ullAffectedRows);
			}

			if (pRes->pSQLResult)
			{
				pRes->ullNumRows = mysql_num_rows(pRes->pSQLResult);
				if (pRes->ullNumRows > UINT32_MAX)
				{
					pRes->uiNumRows = 0;
					if (uiSQLErrno == 0)
						uiSQLErrno = 1264;
				}
				else
				{
					pRes->uiNumRows = static_cast<uint32_t>(pRes->ullNumRows);
				}
			}
			else
			{
				pRes->ullNumRows = 0;
				pRes->uiNumRows = 0;
			}

			vec_pkResult.push_back(pRes);
		} while (!mysql_next_result(m_pkSQL));
	}

	SQLResult * Get()
	{
		if (uiResultPos >= vec_pkResult.size())
			return NULL;

		return vec_pkResult[uiResultPos];
	}

	bool Next()
	{
		if (uiResultPos + 1 >= vec_pkResult.size())
			return false;

		++uiResultPos;
		return true;
	}

	MYSQL *			m_pkSQL;
	int				iID;
	std::string			stQuery;

	std::vector<SQLResult *>	vec_pkResult;
	unsigned int		uiResultPos;

	void *			pvUserData;
	bool			bReturn;

	unsigned int		uiSQLErrno;
	uint64_t		ullSeqID;
} SQLMsg;

class CAsyncSQL
{
	public:
		struct TDurableSpoolEntry
		{
			uint64_t ullSeqID{0};
			std::string stQuery;
		};

		enum ESpoolOverflowPolicy
		{
			SPOOL_OVERFLOW_ALERT_AND_DROP_NEW = 0,
			SPOOL_OVERFLOW_FAIL_STOP_SHUTDOWN = 1,
		};

		typedef void (*TSpoolOverflowCallback)(uint64_t ullSeqID, size_t currentSpoolBytes, size_t maxSpoolBytes, uint64_t ullDroppedSpoolFullCount);

		struct TDurableSpoolStats
		{
			uint64_t ullSubmittedCount{0};
			uint64_t ullSpooledCount{0};
			uint64_t ullPendingSpoolCount{0};
			uint64_t ullPendingMemoryCount{0};
			uint64_t ullReplayedCount{0};
			uint64_t ullCommittedCount{0};
			uint64_t ullQuarantinedCount{0};
			uint64_t ullDroppedSpoolFullCount{0};
			uint64_t ullDiskSyncFailureCount{0};
			uint64_t ullUncertainInFlightRetries{0};
			uint64_t ullReconnectAttempts{0};
			uint64_t ullReconnectSuccessCount{0};
			size_t   spoolBytes{0};
		};

		CAsyncSQL();
		virtual ~CAsyncSQL();

		void		Quit();
		void		Destroy();

		bool   		Setup(const char * c_pszHost, const char * c_pszUser, const char * c_pszPassword, const char * c_pszDB, const char * c_pszLocale,
			bool bNoThread = false, int iPort = 0);
		bool		Setup(CAsyncSQL * sql, bool bNoThread = false);

		bool		Connect();
		bool		Reconnect();
		bool		IsConnected() const { return m_bConnected; }
		bool		IsEnd() const { return m_bEnd; }
		bool		QueryLocaleSet();

		void		AsyncQuery(const char * c_pszQuery);
		void		ReturnQuery(const char * c_pszQuery, void * pvUserData);
		virtual std::unique_ptr<SQLMsg>	DirectQuery(const char * c_pszQuery);

		DWORD		CountQuery();
		DWORD		CountResult();

		void		PushResult(SQLMsg * p);
		bool		PopResult(SQLMsg ** pp);

		void		PushFailedQuery(SQLMsg * p);
		bool		PopFailedQuery(SQLMsg ** pp);
		DWORD		CountFailedQuery();

		typedef void (*TFailedQueryCallback)(SQLMsg * pMsg);
		void		SetFailedQueryCallback(TFailedQueryCallback pfn) { m_pfnFailedQueryCallback = pfn; }

		static std::string SanitizeQueryForLog(const std::string & query);
		static std::string SanitizeQueryForLog(const std::string & query, bool bNoBackslashEscapes);

		bool		HasFailedQueryOverflow() const { return m_bFailedQueueOverflow.load(std::memory_order_relaxed); }
		void		ResetFailedQueueOverflow() { m_bFailedQueueOverflow.store(false, std::memory_order_relaxed); }
		void		ResetFailedQueryOverflow() { m_bFailedQueueOverflow.store(false, std::memory_order_relaxed); }
		size_t		GetFailedQueueBytes() const { return m_failedQueueBytes; }
		size_t		GetQueryQueueBytes() const { return m_queryQueueBytes; }

		bool		EnableDurableSpool(const char * c_pszSpoolPath, size_t maxMemoryQueueCount = 10000, size_t maxSpoolBytes = 64 * 1024 * 1024, DWORD dwReconnectIntervalMs = 1000);
		bool		IsDurableSpoolEnabled() const { return m_bDurableSpoolEnabled; }
		void		SetMaxQueryQueueCount(size_t maxCount);
		void		SetMaxSpoolBytes(size_t maxBytes);
		void		SetReconnectIntervalMs(DWORD dwIntervalMs);
		void		SetSpoolOverflowPolicy(ESpoolOverflowPolicy ePolicy) { m_eSpoolOverflowPolicy.store(ePolicy, std::memory_order_relaxed); }
		ESpoolOverflowPolicy GetSpoolOverflowPolicy() const { return m_eSpoolOverflowPolicy.load(std::memory_order_relaxed); }
		void		SetSpoolOverflowCallback(TSpoolOverflowCallback pfn) { m_pfnSpoolOverflowCallback.store(pfn, std::memory_order_relaxed); }
		void		SetNoBackslashEscapes(bool bEnable) { m_bNoBackslashEscapes.store(bEnable, std::memory_order_relaxed); }
		bool		IsNoBackslashEscapes() const { return m_bNoBackslashEscapes.load(std::memory_order_relaxed); }
		TDurableSpoolStats GetDurableSpoolStats();
		bool		FlushAndSpoolOnShutdown(DWORD dwMaxWaitMs = 3000);
		bool		ReplayDurableSpoolNow();
		const std::string & GetSpoolFilePath() const { return m_stSpoolFilePath; }
		static size_t	EscapeStringOfflineSafe(char * dst, size_t dstSize, const char * src, size_t srcSize, bool bNoBackslashEscapes = false);

		void		ChildLoop();

		MYSQL *		GetSQLHandle();

		int			CountQueryFinished();
		void		ResetQueryFinished();

		virtual size_t	EscapeString(char* dst, size_t dstSize, const char *src, size_t srcSize);

	protected:

		void		PushQuery(SQLMsg * p);

		bool		PeekQuery(SQLMsg ** pp);
		bool		PopQuery(int iID);

		bool		PeekQueryFromCopyQueue(SQLMsg ** pp );
		INT			CopyQuery();
		bool		PopQueryFromCopyQueue();

		bool		ResendQuery(SQLMsg * p);

		void		DurableChildLoop();
		bool		AcquireSpoolLockLocked(const std::string & requestedPath);
		void		ReleaseSpoolLockLocked(bool bRemoveLockFile = false);
		bool		LoadSpoolFromDiskLocked();
		bool		SyncSpoolToDiskLocked();
		bool		AppendQuarantineRecordLocked(uint64_t ullSeqID, unsigned int uiErrno, const std::string & stQuery);
		void		DrainMemoryQueueToSpoolLocked();
		bool		EnqueueToSpoolLocked(uint64_t ullSeqID, const std::string & stSanitizedQuery, bool bFront = false);
		static bool	IsConnectionLossError(unsigned int uiErrno);
		static uint32_t ComputeSpoolChecksum(uint64_t ullSeqID, const std::string & stPayload);
		static std::string EscapeSpoolPayload(const std::string & stRaw);
		static bool	UnescapeSpoolPayload(const std::string & stEscaped, std::string & stOut);

	public:
		int			GetCopiedQueryCount();
		void		ResetCopiedQueryCount();
		void		AddCopiedQueryCount( int iCopiedQuery );

		//private:
	protected:
		MYSQL m_hDB;

		std::string	m_stHost;
		std::string	m_stUser;
		std::string	m_stPassword;
		std::string	m_stDB;
		std::string	m_stLocale;

		int	m_iMsgCount;
		int	m_aiPipe[2];
		int m_iPort;

		std::queue<SQLMsg *> m_queue_query;
		std::queue<SQLMsg *> m_queue_query_copy;
		//std::map<int, SQLMsg *>	m_map_kSQLMsgUnfinished;

		std::queue<SQLMsg *> m_queue_result;
		std::queue<SQLMsg *> m_queue_failed;

		TFailedQueryCallback m_pfnFailedQueryCallback{nullptr};
		size_t m_failedQueueBytes{0};
		size_t m_queryQueueBytes{0};
		std::atomic<bool> m_bFailedQueueOverflow{false};

		bool		m_bDurableSpoolEnabled{false};
		bool		m_bSpoolingActive{false};
		std::string	m_stSpoolFilePath;
		std::string	m_stSpoolLockPath;
#ifndef __WIN32__
		int			m_iSpoolLockFd{-1};
#else
		HANDLE		m_hSpoolLockFile{INVALID_HANDLE_VALUE};
#endif
		size_t		m_maxQueryQueueCount{100000};
		size_t		m_maxSpoolBytes{64 * 1024 * 1024};
		DWORD		m_dwReconnectIntervalMs{1000};
		uint64_t	m_ullLastReconnectAttemptMs{0};
		std::deque<TDurableSpoolEntry> m_dequeSpool;
		size_t		m_spoolBytes{0};
		uint64_t	m_ullSubmittedSeq{0};
		uint64_t	m_ullSubmittedCount{0};
		uint64_t	m_ullSpooledCount{0};
		uint64_t	m_ullReplayedCount{0};
		uint64_t	m_ullCommittedCount{0};
		uint64_t	m_ullQuarantinedCount{0};
		uint64_t	m_ullDroppedSpoolFullCount{0};
		uint64_t	m_ullDiskSyncFailureCount{0};
		uint64_t	m_ullUncertainInFlightRetries{0};
		uint64_t	m_ullReconnectAttempts{0};
		uint64_t	m_ullReconnectSuccessCount{0};
		std::atomic<ESpoolOverflowPolicy> m_eSpoolOverflowPolicy{SPOOL_OVERFLOW_ALERT_AND_DROP_NEW};
		std::atomic<TSpoolOverflowCallback> m_pfnSpoolOverflowCallback{nullptr};
		std::atomic<bool> m_bNoBackslashEscapes{false};
		std::atomic<std::thread::id> m_mtxQueryOwner{};

		std::atomic<bool> m_bEnd{false};

#ifndef __WIN32__
		pthread_t m_hThread;
		std::unique_ptr<pthread_mutex_t> m_mtxQuery;
		std::unique_ptr<pthread_mutex_t> m_mtxResult;
#else
		HANDLE m_hThread;
		std::unique_ptr<CRITICAL_SECTION> m_mtxQuery;
		std::unique_ptr<CRITICAL_SECTION> m_mtxResult;
#endif

		CSemaphore m_sem;

		int	m_iQueryFinished;

		unsigned long m_ulThreadID;
		bool m_bConnected;
		int	m_iCopiedQuery;
};

class CAsyncSQL2 : public CAsyncSQL
{
	public:
		void SetLocale ( const std::string & stLocale );
};

#endif
