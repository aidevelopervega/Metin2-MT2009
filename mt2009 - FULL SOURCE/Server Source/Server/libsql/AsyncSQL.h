#ifndef __INC_METIN_II_ASYNCSQL_H__
#define __INC_METIN_II_ASYNCSQL_H__

#include "../libthecore/include/stdafx.h"
#include "../libthecore/include/log.h"

#include <string>
#include <queue>
#include <vector>
#include <map>
#include <mysql/mysql.h>
#include <mysql/errmsg.h>
#include <mysql/mysqld_error.h>
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
	_SQLMsg() : m_pkSQL(NULL), iID(0), uiResultPos(0), pvUserData(NULL), bReturn(false), uiSQLErrno(0)
	{
	}

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
} SQLMsg;

class CAsyncSQL
{
	public:
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
		std::unique_ptr<SQLMsg>	DirectQuery(const char * c_pszQuery);

		DWORD		CountQuery();
		DWORD		CountResult();

		void		PushResult(SQLMsg * p);
		bool		PopResult(SQLMsg ** pp);

		void		PushFailedQuery(SQLMsg * p);
		bool		PopFailedQuery(SQLMsg ** pp);
		DWORD		CountFailedQuery();

		void		ChildLoop();

		MYSQL *		GetSQLHandle();

		int			CountQueryFinished();
		void		ResetQueryFinished();

		size_t		EscapeString(char* dst, size_t dstSize, const char *src, size_t srcSize);

	protected:

		void		PushQuery(SQLMsg * p);

		bool		PeekQuery(SQLMsg ** pp);
		bool		PopQuery(int iID);

		bool		PeekQueryFromCopyQueue(SQLMsg ** pp );
		INT			CopyQuery();
		bool		PopQueryFromCopyQueue();

		bool		ResendQuery(SQLMsg * p);

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

		volatile bool m_bEnd;

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
//martysama0134's 4e4e75d8b719b9240e033009cf4d7b0f
