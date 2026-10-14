#ifndef __WIN32__
#include <sys/time.h>
#endif

#include <cstdlib>
#include <cstring>
#include <memory>
#include <cctype>

#include "AsyncSQL.h"

// TODO: Consider providing platform-independent mutex class.
#ifndef __WIN32__
#define MUTEX_LOCK(mtx) pthread_mutex_lock(mtx)
#define MUTEX_UNLOCK(mtx) pthread_mutex_unlock(mtx)
#else
#define MUTEX_LOCK(mtx) ::EnterCriticalSection(mtx)
#define MUTEX_UNLOCK(mtx) ::LeaveCriticalSection(mtx)
#endif

std::string CAsyncSQL::SanitizeQueryForLog(const std::string & query)
{
	if (query.empty())
		return query;

	std::string lowerQuery = query;
	for (char & c : lowerQuery)
		c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));

	const char * sensitiveKeys[] = {"password", "pwd", "token", "secret", "social_id"};
	std::string result = query;

	for (const char * key : sensitiveKeys)
	{
		size_t keyLen = std::strlen(key);
		size_t pos = 0;
		while ((pos = lowerQuery.find(key, pos)) != std::string::npos)
		{
			size_t searchPos = pos + keyLen;
			while (searchPos < lowerQuery.size() && (lowerQuery[searchPos] == ' ' || lowerQuery[searchPos] == '\t'))
				++searchPos;

			if (searchPos < lowerQuery.size() && lowerQuery[searchPos] == '=')
			{
				++searchPos;
				while (searchPos < lowerQuery.size() && (lowerQuery[searchPos] == ' ' || lowerQuery[searchPos] == '\t'))
					++searchPos;

				if (lowerQuery.substr(searchPos, 9) == "password(")
				{
					size_t openParen = searchPos + 8;
					size_t closeParen = lowerQuery.find(')', openParen);
					if (closeParen != std::string::npos)
					{
						result.replace(openParen + 1, closeParen - openParen - 1, "'***'");
						lowerQuery.replace(openParen + 1, closeParen - openParen - 1, "'***'");
						pos = openParen + 6;
						continue;
					}
				}
				else if (searchPos < lowerQuery.size() && (lowerQuery[searchPos] == '\'' || lowerQuery[searchPos] == '"'))
				{
					char quoteChar = lowerQuery[searchPos];
					size_t startQuote = searchPos;
					size_t endQuote = startQuote + 1;
					while (endQuote < lowerQuery.size())
					{
						if (lowerQuery[endQuote] == '\\')
							endQuote += 2;
						else if (lowerQuery[endQuote] == quoteChar)
							break;
						else
							++endQuote;
					}
					if (endQuote < lowerQuery.size())
					{
						result.replace(startQuote + 1, endQuote - startQuote - 1, "***");
						lowerQuery.replace(startQuote + 1, endQuote - startQuote - 1, "***");
						pos = startQuote + 5;
						continue;
					}
				}
			}
			pos += keyLen;
		}
	}
	return result;
}

CAsyncSQL::CAsyncSQL()
	: m_stHost(""), m_stUser(""), m_stPassword(""), m_stDB(""), m_stLocale(""),
	m_iMsgCount(0), m_iPort(0), m_bEnd(false),
#ifndef __WIN32__
	m_hThread(0),
#else
	m_hThread(INVALID_HANDLE_VALUE),
#endif
	m_iQueryFinished(0), m_ulThreadID(0), m_bConnected(false), m_iCopiedQuery(0)
{
	memset( &m_hDB, 0, sizeof(m_hDB) );

	m_aiPipe[0] = 0;
	m_aiPipe[1] = 0;

#ifndef __WIN32__
	m_mtxQuery = std::make_unique<pthread_mutex_t>();
	m_mtxResult = std::make_unique<pthread_mutex_t>();
	pthread_mutex_init(m_mtxQuery.get(), NULL);
	pthread_mutex_init(m_mtxResult.get(), NULL);
#else
	m_mtxQuery = std::make_unique<CRITICAL_SECTION>();
	m_mtxResult = std::make_unique<CRITICAL_SECTION>();
	::InitializeCriticalSection(m_mtxQuery.get());
	::InitializeCriticalSection(m_mtxResult.get());
#endif
}

CAsyncSQL::~CAsyncSQL()
{
	Quit();
	Destroy();
}

void CAsyncSQL::Destroy()
{
	if (m_bConnected)
	{
		sys_log(0, "AsyncSQL: closing mysql connection.");
		mysql_close(&m_hDB);
		m_bConnected = false;
		memset(&m_hDB, 0, sizeof(m_hDB));
	}

	if (m_mtxResult)
	{
		MUTEX_LOCK(m_mtxResult.get());
		while (!m_queue_failed.empty())
		{
			delete m_queue_failed.front();
			m_queue_failed.pop();
		}
		while (!m_queue_result.empty())
		{
			delete m_queue_result.front();
			m_queue_result.pop();
		}
		MUTEX_UNLOCK(m_mtxResult.get());
	}
	else
	{
		while (!m_queue_failed.empty())
		{
			delete m_queue_failed.front();
			m_queue_failed.pop();
		}
		while (!m_queue_result.empty())
		{
			delete m_queue_result.front();
			m_queue_result.pop();
		}
	}

	if (m_mtxQuery)
	{
		MUTEX_LOCK(m_mtxQuery.get());
		while (!m_queue_query.empty())
		{
			delete m_queue_query.front();
			m_queue_query.pop();
		}
		while (!m_queue_query_copy.empty())
		{
			delete m_queue_query_copy.front();
			m_queue_query_copy.pop();
		}
		MUTEX_UNLOCK(m_mtxQuery.get());
	}
	else
	{
		while (!m_queue_query.empty())
		{
			delete m_queue_query.front();
			m_queue_query.pop();
		}
		while (!m_queue_query_copy.empty())
		{
			delete m_queue_query_copy.front();
			m_queue_query_copy.pop();
		}
	}

	if (m_mtxQuery)
	{
#ifndef __WIN32__
		pthread_mutex_destroy(m_mtxQuery.get());
#else
		::DeleteCriticalSection(m_mtxQuery.get());
#endif
		m_mtxQuery.reset();
	}

	if (m_mtxResult)
	{
#ifndef __WIN32__
		pthread_mutex_destroy(m_mtxResult.get());
#else
		::DeleteCriticalSection(m_mtxResult.get());
#endif
		m_mtxResult.reset();
	}
}

#ifndef __WIN32__
void * AsyncSQLThread(void * arg)
#else
unsigned int __stdcall AsyncSQLThread(void* arg)
#endif
{
	CAsyncSQL * pSQL = ((CAsyncSQL *) arg);

	int backoff_ms = 1000;
	while (!pSQL->IsEnd())
	{
		if (pSQL->Connect())
			break;

		sys_err("AsyncSQL: initial connection failed, retrying in %d ms...", backoff_ms);
		for (int i = 0; i < (backoff_ms / 100) && !pSQL->IsEnd(); ++i)
		{
#ifndef __WIN32__
			usleep(100000);
#else
			::Sleep(100);
#endif
		}
		if (backoff_ms < 5000)
			backoff_ms += 1000;
	}

	pSQL->ChildLoop();

#ifndef __WIN32__
	return NULL;
#else
	return 0;
#endif
}

bool CAsyncSQL::QueryLocaleSet()
{
	if (0 == m_stLocale.length())
	{
		sys_err("m_stLocale == 0");
		return true;
	}

	else if (m_stLocale == "ascii")
	{
		sys_err("m_stLocale == ascii");
		return true;
	}

	if (mysql_set_character_set(&m_hDB, m_stLocale.c_str()))
	{
		sys_err("cannot set locale %s by 'mysql_set_character_set', errno %u %s", m_stLocale.c_str(), mysql_errno(&m_hDB) , mysql_error(&m_hDB));
		return false;
	}

	sys_log(0, "\t--mysql_set_character_set(%s)", m_stLocale.c_str());

	return true;
}

bool CAsyncSQL::Connect()
{
	const unsigned long client_ver = mysql_get_client_version();
#if defined(MARIADB_BASE_VERSION) || defined(MARIADB_VERSION_ID)
	if (client_ver >= 80000UL && client_ver < 100000UL)
	{
		fprintf(stderr, "AsyncSQL FATAL ABI MISMATCH: compiled with MariaDB headers but linked against MySQL client library (%lu)\n", client_ver);
		return false;
	}
#else
	if (client_ver < 80000UL || client_ver >= 100000UL)
	{
		fprintf(stderr, "AsyncSQL FATAL ABI MISMATCH: compiled with MySQL headers (%d) but linked against non-MySQL client library (%lu)\n", MYSQL_VERSION_ID, client_ver);
		return false;
	}
#endif

	if (0 == mysql_init(&m_hDB))
	{
		fprintf(stderr, "mysql_init failed\n");
		return false;
	}

	if (!m_stLocale.empty())
		mysql_options(&m_hDB, MYSQL_SET_CHARSET_NAME, m_stLocale.c_str());

#ifdef MARIADB_BASE_VERSION
	constexpr bool reconnect = true;
	mysql_options(&m_hDB, MYSQL_OPT_RECONNECT, &reconnect);
#endif

	if (!mysql_real_connect(&m_hDB, m_stHost.c_str(), m_stUser.c_str(), m_stPassword.c_str(), m_stDB.c_str(), m_iPort, NULL, CLIENT_MULTI_STATEMENTS))
	{
		fprintf(stderr, "mysql_real_connect failed: %s\n", mysql_error(&m_hDB));
		return false;
	}

#ifdef MARIADB_BASE_VERSION
	int actual_reconnect_val{};
	mysql_get_option(&m_hDB, MYSQL_OPT_RECONNECT, &actual_reconnect_val);
	fprintf(stdout, "AsyncSQL: connected to %s (reconnect %d) [MARIADB]\n", m_stHost.c_str(), actual_reconnect_val);
#else
	fprintf(stdout, "AsyncSQL: connected to %s [MYSQL]\n", m_stHost.c_str());
#endif

	m_ulThreadID = mysql_thread_id(&m_hDB);
	m_bConnected = true;
	return true;
}

bool CAsyncSQL::Reconnect()
{
	if (m_bConnected)
	{
		mysql_close(&m_hDB);
		m_bConnected = false;
		memset(&m_hDB, 0, sizeof(m_hDB));
	}
	return Connect();
}

bool CAsyncSQL::Setup(CAsyncSQL * sql, bool bNoThread)
{
	return Setup(sql->m_stHost.c_str(),
			sql->m_stUser.c_str(),
			sql->m_stPassword.c_str(),
			sql->m_stDB.c_str(),
			sql->m_stLocale.c_str(),
			bNoThread,
			sql->m_iPort);
}

bool CAsyncSQL::Setup(const char * c_pszHost, const char * c_pszUser, const char * c_pszPassword, const char * c_pszDB, const char * c_pszLocale, bool bNoThread, int iPort)
{
	m_stHost = c_pszHost;
	m_stUser = c_pszUser;
	m_stPassword = c_pszPassword;
	m_stDB = c_pszDB;
	m_iPort = iPort;

	if (c_pszLocale)
	{
		m_stLocale = c_pszLocale;
		sys_log(0, "AsyncSQL: locale %s", m_stLocale.c_str());
	}

	if (!m_mtxQuery)
	{
#ifndef __WIN32__
		m_mtxQuery = std::make_unique<pthread_mutex_t>();
		pthread_mutex_init(m_mtxQuery.get(), NULL);
#else
		m_mtxQuery = std::make_unique<CRITICAL_SECTION>();
		::InitializeCriticalSection(m_mtxQuery.get());
#endif
	}

	if (!m_mtxResult)
	{
#ifndef __WIN32__
		m_mtxResult = std::make_unique<pthread_mutex_t>();
		pthread_mutex_init(m_mtxResult.get(), NULL);
#else
		m_mtxResult = std::make_unique<CRITICAL_SECTION>();
		::InitializeCriticalSection(m_mtxResult.get());
#endif
	}

	if (!bNoThread)
	{
#ifndef __WIN32__
		if (pthread_create(&m_hThread, NULL, AsyncSQLThread, this) != 0)
		{
			m_hThread = 0;
			perror("CAsyncSQL::Setup");
			return false;
		}
#else
		uintptr_t th = ::_beginthreadex(NULL, 0, AsyncSQLThread, this, 0, NULL);
		if (th == 0 || th == (uintptr_t)-1) {
			m_hThread = INVALID_HANDLE_VALUE;
			perror("CAsyncSQL::Setup");
			return false;
		}
		m_hThread = (HANDLE)th;
#endif
		return true;
	}
	else
		return Connect();
}

void CAsyncSQL::Quit()
{
	m_bEnd = true;
	m_sem.Release();

#ifndef __WIN32__
	if (m_hThread)
	{
		pthread_join(m_hThread, NULL);
		m_hThread = 0;
	}
#else
	if (m_hThread != INVALID_HANDLE_VALUE && m_hThread != NULL) {
		::WaitForSingleObject(m_hThread, INFINITE);
		::CloseHandle(m_hThread);
		m_hThread = INVALID_HANDLE_VALUE;
	}
#endif
}

std::unique_ptr<SQLMsg> CAsyncSQL::DirectQuery(const char * c_pszQuery)
{
	auto p = std::make_unique<SQLMsg>();

	p->m_pkSQL = &m_hDB;
	p->iID = ++m_iMsgCount;
	p->stQuery = c_pszQuery ? c_pszQuery : "";

	if (!m_bConnected)
	{
		p->uiSQLErrno = CR_SERVER_GONE_ERROR;
		p->Store();
		return p;
	}

	if (m_ulThreadID != mysql_thread_id(&m_hDB))
	{
		sys_log(0, "MySQL connection was reconnected. querying locale set"); // @warme012
		while (!QueryLocaleSet());
		m_ulThreadID = mysql_thread_id(&m_hDB);
	}

	if (mysql_real_query(&m_hDB, p->stQuery.c_str(), p->stQuery.length()))
	{
		char buf[1024];

		snprintf(buf, sizeof(buf),
				"AsyncSQL::DirectQuery : mysql_query error: %s\nquery: %s",
				mysql_error(&m_hDB), SanitizeQueryForLog(p->stQuery).c_str());

		sys_err(buf);
		p->uiSQLErrno = mysql_errno(&m_hDB);
	}

	p->Store();
	return p;
}

void CAsyncSQL::AsyncQuery(const char * c_pszQuery)
{
	SQLMsg * p = new SQLMsg;

	p->m_pkSQL = &m_hDB;
	p->iID = ++m_iMsgCount;
	p->stQuery = c_pszQuery;

	PushQuery(p);
}

void CAsyncSQL::ReturnQuery(const char * c_pszQuery, void * pvUserData)
{
	SQLMsg * p = new SQLMsg;

	p->m_pkSQL = &m_hDB;
	p->iID = ++m_iMsgCount;
	p->stQuery = c_pszQuery;
	p->bReturn = true;
	p->pvUserData = pvUserData;

	PushQuery(p);
}

void CAsyncSQL::PushResult(SQLMsg * p)
{
	if (!m_mtxResult)
	{
		delete p;
		return;
	}

	MUTEX_LOCK(m_mtxResult.get());
	m_queue_result.push(p);
	MUTEX_UNLOCK(m_mtxResult.get());
}

bool CAsyncSQL::PopResult(SQLMsg ** pp)
{
	if (!m_mtxResult)
		return false;

	MUTEX_LOCK(m_mtxResult.get());

	if (m_queue_result.empty())
	{
		MUTEX_UNLOCK(m_mtxResult.get());
		return false;
	}

	*pp = m_queue_result.front();
	m_queue_result.pop();
	MUTEX_UNLOCK(m_mtxResult.get());
	return true;
}

void CAsyncSQL::PushFailedQuery(SQLMsg * p)
{
	if (!p)
		return;

	if (m_pfnFailedQueryCallback)
	{
		m_pfnFailedQueryCallback(p);
		return;
	}

	if (!m_mtxResult)
	{
		delete p;
		return;
	}

	MUTEX_LOCK(m_mtxResult.get());
	constexpr size_t MAX_FAILED_QUEUE_CAPACITY = 10000;
	constexpr size_t MAX_FAILED_QUEUE_BYTES = 64 * 1024 * 1024; // 64 MB

	size_t queryBytes = sizeof(SQLMsg) + p->stQuery.capacity();

	if (m_queue_failed.size() >= MAX_FAILED_QUEUE_CAPACITY || (m_failedQueueBytes + queryBytes) > MAX_FAILED_QUEUE_BYTES)
	{
		m_bFailedQueueOverflow = true;
		sys_err("CRITICAL ALERT: AsyncSQL::PushFailedQuery queue capacity/memory limit reached (%zu queries, %zu bytes). Backpressure activated; query preserved without silent drop!",
				m_queue_failed.size(), m_failedQueueBytes);
	}

	m_queue_failed.push(p);
	m_failedQueueBytes += queryBytes;
	MUTEX_UNLOCK(m_mtxResult.get());
}

bool CAsyncSQL::PopFailedQuery(SQLMsg ** pp)
{
	if (!m_mtxResult)
		return false;

	MUTEX_LOCK(m_mtxResult.get());

	if (m_queue_failed.empty())
	{
		MUTEX_UNLOCK(m_mtxResult.get());
		return false;
	}

	*pp = m_queue_failed.front();
	m_queue_failed.pop();
	if (*pp)
	{
		size_t queryBytes = sizeof(SQLMsg) + (*pp)->stQuery.capacity();
		if (m_failedQueueBytes >= queryBytes)
			m_failedQueueBytes -= queryBytes;
		else
			m_failedQueueBytes = 0;
	}
	MUTEX_UNLOCK(m_mtxResult.get());
	return true;
}

DWORD CAsyncSQL::CountFailedQuery()
{
	if (!m_mtxResult)
		return 0;

	MUTEX_LOCK(m_mtxResult.get());
	DWORD count = static_cast<DWORD>(m_queue_failed.size());
	MUTEX_UNLOCK(m_mtxResult.get());
	return count;
}

void CAsyncSQL::PushQuery(SQLMsg * p)
{
	if (!p)
		return;

	if (!m_mtxQuery)
	{
		delete p;
		return;
	}

	MUTEX_LOCK(m_mtxQuery.get());
	constexpr size_t MAX_ASYNC_QUERY_QUEUE_CAPACITY = 100000;
	constexpr size_t MAX_ASYNC_QUERY_QUEUE_BYTES = 128 * 1024 * 1024; // 128 MB

	size_t queryBytes = sizeof(SQLMsg) + p->stQuery.capacity();

	if (m_queue_query.size() >= MAX_ASYNC_QUERY_QUEUE_CAPACITY || (m_queryQueueBytes + queryBytes) > MAX_ASYNC_QUERY_QUEUE_BYTES)
	{
		sys_err("CRITICAL: AsyncSQL::PushQuery: async query queue capacity/memory limit reached (%zu queries, %zu bytes); rejecting query to activate backpressure!",
				m_queue_query.size(), m_queryQueueBytes);
		MUTEX_UNLOCK(m_mtxQuery.get());

		p->uiSQLErrno = CR_CONNECTION_ERROR;
		if (p->bReturn)
		{
			p->Store();
			PushResult(p);
		}
		else
		{
			PushFailedQuery(p);
		}
		return;
	}

	m_queue_query.push(p);
	m_queryQueueBytes += queryBytes;
	m_sem.Release();
	MUTEX_UNLOCK(m_mtxQuery.get());
}

bool CAsyncSQL::PeekQuery(SQLMsg ** pp)
{
	if (!m_mtxQuery)
		return false;

	MUTEX_LOCK(m_mtxQuery.get());

	if (m_queue_query.empty())
	{
		MUTEX_UNLOCK(m_mtxQuery.get());
		return false;
	}

	*pp = m_queue_query.front();
	MUTEX_UNLOCK(m_mtxQuery.get());
	return true;
}

bool CAsyncSQL::PopQuery(int iID)
{
	(void)iID;
	if (!m_mtxQuery)
		return false;

	MUTEX_LOCK(m_mtxQuery.get());

	if (m_queue_query.empty())
	{
		MUTEX_UNLOCK(m_mtxQuery.get());
		return false;
	}

	SQLMsg * p = m_queue_query.front();
	if (p)
	{
		size_t queryBytes = sizeof(SQLMsg) + p->stQuery.capacity();
		if (m_queryQueueBytes >= queryBytes)
			m_queryQueueBytes -= queryBytes;
		else
			m_queryQueueBytes = 0;
	}

	m_queue_query.pop();
	MUTEX_UNLOCK(m_mtxQuery.get());
	return true;
}

bool CAsyncSQL::PeekQueryFromCopyQueue(SQLMsg ** pp)
{
	if (m_queue_query_copy.empty())
		return false;

	*pp = m_queue_query_copy.front();
	return true;
}

int CAsyncSQL::CopyQuery()
{
	if (!m_mtxQuery)
		return -1;

	MUTEX_LOCK(m_mtxQuery.get());

	if (m_queue_query.empty())
	{
		MUTEX_UNLOCK(m_mtxQuery.get());
		return -1;
	}

	while (!m_queue_query.empty())
	{
		SQLMsg * p = m_queue_query.front();
		m_queue_query_copy.push(p);
		m_queue_query.pop();
	}

	m_queryQueueBytes = 0;
	int count = m_queue_query_copy.size();

	MUTEX_UNLOCK(m_mtxQuery.get());
	return count;
}

bool CAsyncSQL::PopQueryFromCopyQueue()
{
	if (m_queue_query_copy.empty())
	{
		return false;
	}

	m_queue_query_copy.pop();
	return true;
}
int		CAsyncSQL::GetCopiedQueryCount()
{
	return m_iCopiedQuery;
}
void 	CAsyncSQL::ResetCopiedQueryCount()
{
	m_iCopiedQuery = 0;
}

void	CAsyncSQL::AddCopiedQueryCount(int iCopiedQuery)
{
	m_iCopiedQuery += iCopiedQuery;
}

DWORD CAsyncSQL::CountQuery()
{
	if (!m_mtxQuery)
		return 0;

	MUTEX_LOCK(m_mtxQuery.get());
	DWORD count = static_cast<DWORD>(m_queue_query.size());
	MUTEX_UNLOCK(m_mtxQuery.get());
	return count;
}

DWORD CAsyncSQL::CountResult()
{
	if (!m_mtxResult)
		return 0;

	MUTEX_LOCK(m_mtxResult.get());
	DWORD count = static_cast<DWORD>(m_queue_result.size());
	MUTEX_UNLOCK(m_mtxResult.get());
	return count;
}

void __timediff(struct timeval *a, struct timeval *b, struct timeval *rslt)
{
	if (a->tv_sec < b->tv_sec)
		rslt->tv_sec = rslt->tv_usec = 0;
	else if (a->tv_sec == b->tv_sec)
	{
		if (a->tv_usec < b->tv_usec)
			rslt->tv_sec = rslt->tv_usec = 0;
		else
		{
			rslt->tv_sec = 0;
			rslt->tv_usec = a->tv_usec - b->tv_usec;
		}
	}
	else
	{                      /* a->tv_sec > b->tv_sec */
		rslt->tv_sec = a->tv_sec - b->tv_sec;

		if (a->tv_usec < b->tv_usec)
		{
			rslt->tv_usec = a->tv_usec + 1000000 - b->tv_usec;
			rslt->tv_sec--;
		} else
			rslt->tv_usec = a->tv_usec - b->tv_usec;
	}
}

class cProfiler
{
	public:
		cProfiler()
		{
			m_nInterval = 0 ;

			memset( &prev, 0, sizeof(prev) );
			memset( &now, 0, sizeof(now) );
			memset( &interval, 0, sizeof(interval) );

			Start();
		}

		cProfiler(int nInterval = 100000)
		{
			m_nInterval = nInterval;

			memset( &prev, 0, sizeof(prev) );
			memset( &now, 0, sizeof(now) );
			memset( &interval, 0, sizeof(interval) );

			Start();
		}

		void Start()
		{
			gettimeofday (&prev , (struct timezone *) 0);
		}

		void Stop()
		{
			gettimeofday(&now, (struct timezone*) 0);
			__timediff(&now, &prev, &interval);
		}

		bool IsOk()
		{
			if (interval.tv_sec > (m_nInterval / 1000000))
				return false;

			if (interval.tv_usec > m_nInterval)
				return false;

			return true;
		}

		struct timeval * GetResult() { return &interval; }
		long GetResultSec() { return interval.tv_sec; }
		long GetResultUSec() { return interval.tv_usec; }

	private:
		int m_nInterval;
		struct timeval  prev;
		struct timeval  now;
		struct timeval	interval;
};

void CAsyncSQL::ChildLoop()
{
	cProfiler profiler(500000);

	while (!m_bEnd)
	{
		m_sem.Wait();

		int count = CopyQuery();

		if (count <= 0)
			continue;

		AddCopiedQueryCount(count);

		SQLMsg * p;

		while (count--)
		{
			profiler.Start();

			if (!PeekQueryFromCopyQueue(&p))
				continue;

			if (!m_bConnected)
			{
				p->uiSQLErrno = CR_CONNECTION_ERROR;
				PopQueryFromCopyQueue();
				if (p->bReturn)
				{
					p->Store();
					PushResult(p);
				}
				else
				{
					PushFailedQuery(p);
				}
				++m_iQueryFinished;
				continue;
			}

			if (m_ulThreadID != mysql_thread_id(&m_hDB))
			{
				sys_log(0, "MySQL connection was reconnected. querying locale set"); // @warme012
				while (!QueryLocaleSet());
				m_ulThreadID = mysql_thread_id(&m_hDB);
			}

			if (mysql_real_query(&m_hDB, p->stQuery.c_str(), p->stQuery.length()))
			{
				p->uiSQLErrno = mysql_errno(&m_hDB);

				sys_err("AsyncSQL: query failed: %s (query: %s errno: %d)",
						mysql_error(&m_hDB), SanitizeQueryForLog(p->stQuery).c_str(), p->uiSQLErrno);

				switch (p->uiSQLErrno)
				{
					case CR_SERVER_LOST: // (errno: 2013)
					case CR_SERVER_GONE_ERROR: // (errno: 2006)
						if (Reconnect())
						{
							sys_err("AsyncSQL: connection restored after failure. In-flight query marked as failed to prevent non-idempotent duplicate transaction.");
						}
						break;

					default:
						break;
				}
			}

			profiler.Stop();

			if (!profiler.IsOk())
				sys_log(0, "[QUERY : LONG INTERVAL(OverSec %ld.%ld)] : %s",
						profiler.GetResultSec(), profiler.GetResultUSec(), SanitizeQueryForLog(p->stQuery).c_str());

			PopQueryFromCopyQueue();

			if (p->bReturn)
			{
				p->Store();
				PushResult(p);
			}
			else
			{
				if (p->uiSQLErrno != 0)
					PushFailedQuery(p);
				else
					delete p;
			}

			++m_iQueryFinished;
		}
	}

	SQLMsg * p;

	while (PeekQuery(&p))
	{
		if (!m_bConnected)
		{
			p->uiSQLErrno = CR_CONNECTION_ERROR;
			sys_err("QUERY_FLUSH: connection offline, query marked failed: %s", SanitizeQueryForLog(p->stQuery).c_str());
			PopQuery(p->iID);
			if (p->bReturn)
			{
				p->Store();
				PushResult(p);
			}
			else
			{
				PushFailedQuery(p);
			}
			++m_iQueryFinished;
			continue;
		}

		if (m_ulThreadID != mysql_thread_id(&m_hDB))
		{
			sys_log(0, "MySQL connection was reconnected. querying locale set"); // @warme012
			while (!QueryLocaleSet());
			m_ulThreadID = mysql_thread_id(&m_hDB);
		}

		if (mysql_real_query(&m_hDB, p->stQuery.c_str(), p->stQuery.length()))
		{
			p->uiSQLErrno = mysql_errno(&m_hDB);

			sys_err("AsyncSQL::ChildLoop : mysql_query error: %s:\nquery: %s",
					mysql_error(&m_hDB), SanitizeQueryForLog(p->stQuery).c_str());

			switch (p->uiSQLErrno)
			{
				case CR_SERVER_LOST:
				case CR_SERVER_GONE_ERROR:
					if (Reconnect())
					{
						sys_err("AsyncSQL: connection restored during flush. In-flight query preserved as failed.");
					}
					break;
				default:
					break;
			}
		}

		sys_log(0, "QUERY_FLUSH: %s", SanitizeQueryForLog(p->stQuery).c_str());

		PopQuery(p->iID);

		if (p->bReturn)
		{
			p->Store();
			PushResult(p);
		}
		else
		{
			if (p->uiSQLErrno != 0)
				PushFailedQuery(p);
			else
				delete p;
		}

		++m_iQueryFinished;
	}
}

int CAsyncSQL::CountQueryFinished()
{
	return m_iQueryFinished;
}

void CAsyncSQL::ResetQueryFinished()
{
	m_iQueryFinished = 0;
}

MYSQL * CAsyncSQL::GetSQLHandle()
{
	return &m_hDB;
}

size_t CAsyncSQL::EscapeString(char* dst, size_t dstSize, const char *src, size_t srcSize)
{
	if (!dst || 0 == dstSize)
		return 0;

	if (0 == srcSize || !src)
	{
		dst[0] = '\0';
		return 0;
	}

	if (dstSize < srcSize * 2 + 1)
	{
		char tmp[256];
		size_t tmpLen = sizeof(tmp) > srcSize ? srcSize : sizeof(tmp);
		strlcpy(tmp, src, tmpLen);

		sys_err("FATAL ERROR!! not enough buffer size (dstSize %u srcSize %u src%s: %s)",
				dstSize, srcSize, tmpLen != srcSize ? "(trimmed to 255 characters)" : "", tmp);

		dst[0] = '\0';
		return 0;
	}

	if (!IsConnected() || !GetSQLHandle())
	{
		dst[0] = '\0';
		return 0;
	}

	unsigned long res = 0;
#if !defined(MARIADB_BASE_VERSION) && defined(LIBMYSQL_VERSION_ID) && LIBMYSQL_VERSION_ID >= 50706
	res = mysql_real_escape_string_quote(GetSQLHandle(), dst, src, srcSize, '\'');
#else
	res = mysql_real_escape_string(GetSQLHandle(), dst, src, srcSize);
#endif

	if (res == static_cast<unsigned long>(-1))
	{
		sys_err("AsyncSQL::EscapeString: MySQL escape function returned error (%s)",
				mysql_error(GetSQLHandle()));
		dst[0] = '\0';
		return 0;
	}

	return static_cast<size_t>(res);
}

void CAsyncSQL2::SetLocale(const std::string & stLocale)
{
	m_stLocale = stLocale;
	QueryLocaleSet();
}

bool CAsyncSQL::ResendQuery(SQLMsg * p)
{
	if (!Reconnect())
	{
		sys_err("AsyncSQL: reconnection failed: %s", mysql_error(&m_hDB));
		return false;
	}

	if (m_ulThreadID != mysql_thread_id(&m_hDB))
	{
		sys_log(0, "MySQL connection was reconnected. querying locale set");
		while (!QueryLocaleSet());
		m_ulThreadID = mysql_thread_id(&m_hDB);
	}

	return mysql_real_query(&m_hDB, p->stQuery.c_str(), p->stQuery.length()) == 0;
}
//martysama0134's 4e4e75d8b719b9240e033009cf4d7b0f
