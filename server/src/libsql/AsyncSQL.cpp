#ifndef __WIN32__
#include <sys/time.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#else
#include <sys/stat.h>
#include <io.h>
#endif

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <cctype>
#include <mutex>
#include <fstream>
#include <sstream>
#include <chrono>
#include <algorithm>

#include "AsyncSQL.h"

// TODO: Consider providing platform-independent mutex class.
#ifndef __WIN32__
#define MUTEX_LOCK(mtx) pthread_mutex_lock(mtx)
#define MUTEX_UNLOCK(mtx) pthread_mutex_unlock(mtx)
#else
#define MUTEX_LOCK(mtx) ::EnterCriticalSection(mtx)
#define MUTEX_UNLOCK(mtx) ::LeaveCriticalSection(mtx)
#endif

namespace
{
	uint64_t GetMonotonicMs()
	{
		return static_cast<uint64_t>(
			std::chrono::duration_cast<std::chrono::milliseconds>(
				std::chrono::steady_clock::now().time_since_epoch()).count());
	}

	void EnforceSpoolFilePermissions(const std::string & path)
	{
		if (path.empty())
			return;
#ifndef __WIN32__
		::chmod(path.c_str(), S_IRUSR | S_IWUSR);
#else
		::_chmod(path.c_str(), _S_IREAD | _S_IWRITE);
#endif
	}

	bool SyncFileDescriptor(FILE * fp)
	{
		if (!fp)
			return false;
		if (std::fflush(fp) != 0)
			return false;
#ifndef __WIN32__
		int fd = ::fileno(fp);
		if (fd >= 0 && ::fsync(fd) != 0)
			return false;
#else
		int fd = ::_fileno(fp);
		if (fd >= 0 && ::_commit(fd) != 0)
			return false;
#endif
		return true;
	}
	char GetOuterSqlQuoteAt(const std::string & s, size_t targetPos)
	{
		char outerQuote = 0;
		size_t limit = std::min(targetPos, s.size());
		for (size_t i = 0; i < limit; ++i)
		{
			char c = s[i];
			if (outerQuote == 0)
			{
				if (c == '\'' || c == '"')
					outerQuote = c;
			}
			else
			{
				if (c == '\\' && i + 1 < s.size())
				{
					++i;
				}
				else if (c == outerQuote)
				{
					if (i + 1 < s.size() && s[i + 1] == outerQuote)
						++i;
					else
						outerQuote = 0;
				}
			}
		}
		return outerQuote;
	}
}

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

				char outerQuote = GetOuterSqlQuoteAt(result, searchPos);

				if (outerQuote == 0 && lowerQuery.substr(searchPos, 9) == "password(")
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
				else if (outerQuote == 0 && searchPos < lowerQuery.size() && (lowerQuery[searchPos] == '\'' || lowerQuery[searchPos] == '"'))
				{
					char quoteChar = lowerQuery[searchPos];
					size_t startQuote = searchPos;
					size_t endQuote = startQuote + 1;
					while (endQuote < lowerQuery.size())
					{
						if (lowerQuery[endQuote] == '\\' && endQuote + 1 < lowerQuery.size())
							endQuote += 2;
						else if (lowerQuery[endQuote] == quoteChar)
						{
							if (endQuote + 1 < lowerQuery.size() && lowerQuery[endQuote + 1] == quoteChar)
								endQuote += 2;
							else
								break;
						}
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
				else if (outerQuote != 0 && searchPos < lowerQuery.size())
				{
					// Inside an outer SQL string literal delimited by outerQuote:
					// Never cross the closing unescaped outerQuote of this SQL column!
					if (lowerQuery[searchPos] == '\\' && searchPos + 1 < lowerQuery.size() &&
						(lowerQuery[searchPos + 1] == '\'' || lowerQuery[searchPos + 1] == '"'))
					{
						char escQuote = lowerQuery[searchPos + 1];
						size_t valStart = searchPos + 2;
						size_t cur = valStart;
						size_t valEnd = std::string::npos;
						while (cur < lowerQuery.size())
						{
							if (lowerQuery[cur] == '\\' && cur + 1 < lowerQuery.size())
							{
								if (lowerQuery[cur + 1] == escQuote)
								{
									valEnd = cur;
									break;
								}
								cur += 2;
							}
							else if (lowerQuery[cur] == outerQuote)
							{
								if (cur + 1 < lowerQuery.size() && lowerQuery[cur + 1] == outerQuote)
									cur += 2;
								else
									break;
							}
							else
							{
								++cur;
							}
						}
						if (valEnd != std::string::npos)
						{
							result.replace(valStart, valEnd - valStart, "***");
							lowerQuery.replace(valStart, valEnd - valStart, "***");
							pos = valStart + 5;
							continue;
						}
					}
					else if (lowerQuery[searchPos] == outerQuote)
					{
						// Only mask if it is a doubled inner quote (e.g. ''secret'' inside '...');
						// a single outerQuote at searchPos is the end of the SQL column literal itself!
						if (searchPos + 1 < lowerQuery.size() && lowerQuery[searchPos + 1] == outerQuote)
						{
							size_t valStart = searchPos + 2;
							size_t cur = valStart;
							size_t valEnd = std::string::npos;
							while (cur < lowerQuery.size())
							{
								if (lowerQuery[cur] == '\\' && cur + 1 < lowerQuery.size())
								{
									cur += 2;
								}
								else if (lowerQuery[cur] == outerQuote)
								{
									if (cur + 1 < lowerQuery.size() && lowerQuery[cur + 1] == outerQuote)
									{
										valEnd = cur;
										break;
									}
									else
									{
										break;
									}
								}
								else
								{
									++cur;
								}
							}
							if (valEnd != std::string::npos)
							{
								result.replace(valStart, valEnd - valStart, "***");
								lowerQuery.replace(valStart, valEnd - valStart, "***");
								pos = valStart + 5;
								continue;
							}
						}
					}
					else if (lowerQuery[searchPos] == '\'' || lowerQuery[searchPos] == '"')
					{
						// Opposite quote character inside outerQuote (e.g. "secret" inside '...')
						char innerQuote = lowerQuery[searchPos];
						size_t valStart = searchPos + 1;
						size_t cur = valStart;
						size_t valEnd = std::string::npos;
						while (cur < lowerQuery.size())
						{
							if (lowerQuery[cur] == '\\' && cur + 1 < lowerQuery.size())
							{
								cur += 2;
							}
							else if (lowerQuery[cur] == outerQuote)
							{
								if (cur + 1 < lowerQuery.size() && lowerQuery[cur + 1] == outerQuote)
								{
									cur += 2;
								}
								else
								{
									// Hit closing outerQuote before closing innerQuote: bound strictly inside column!
									valEnd = cur;
									break;
								}
							}
							else if (lowerQuery[cur] == innerQuote)
							{
								valEnd = cur;
								break;
							}
							else
							{
								++cur;
							}
						}
						if (valEnd != std::string::npos && valEnd > valStart)
						{
							result.replace(valStart, valEnd - valStart, "***");
							lowerQuery.replace(valStart, valEnd - valStart, "***");
							pos = valStart + 3;
							continue;
						}
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
		if (m_bDurableSpoolEnabled && (!m_queue_query.empty() || !m_queue_query_copy.empty()))
		{
			DrainMemoryQueueToSpoolLocked();
			SyncSpoolToDiskLocked();
		}
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
		m_dequeSpool.clear();
		m_spoolBytes = 0;
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
		m_dequeSpool.clear();
		m_spoolBytes = 0;
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

	if (pSQL->IsDurableSpoolEnabled())
	{
		pSQL->Connect();
		pSQL->ChildLoop();
	}
	else
	{
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
	}

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

#ifdef SERVER_STATUS_NO_BACKSLASH_ESCAPES
	m_bNoBackslashEscapes = (m_hDB.server_status & SERVER_STATUS_NO_BACKSLASH_ESCAPES) != 0;
#endif

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
	{
		static std::once_flag s_abiLogOnce;
		std::call_once(s_abiLogOnce, [client_ver]() {
#if defined(MARIADB_BASE_VERSION) || defined(MARIADB_VERSION_ID)
			const char* header_flavor = "MariaDB";
#else
			const char* header_flavor = "MySQL";
#endif
			fprintf(stdout, "AsyncSQL client ABI OK: header=%s(MYSQL_VERSION_ID=%d) runtime_lib=%s(client_version=%lu)\n",
				header_flavor, static_cast<int>(MYSQL_VERSION_ID), mysql_get_client_info(), client_ver);
			fflush(stdout);
		});
	}

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

#ifdef SERVER_STATUS_NO_BACKSLASH_ESCAPES
	m_bNoBackslashEscapes = (m_hDB.server_status & SERVER_STATUS_NO_BACKSLASH_ESCAPES) != 0;
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

	if (m_bDurableSpoolEnabled && !p->bReturn)
	{
		uint64_t seqId = ++m_ullSubmittedSeq;
		p->ullSeqID = seqId;
		++m_ullSubmittedCount;

		bool bQueueFull = (m_queue_query.size() >= m_maxQueryQueueCount) ||
						  ((m_queryQueueBytes + queryBytes) > MAX_ASYNC_QUERY_QUEUE_BYTES);

		if (!m_bConnected)
		{
			std::string sanitizedQuery = SanitizeQueryForLog(p->stQuery);
			DrainMemoryQueueToSpoolLocked();
			EnqueueToSpoolLocked(seqId, sanitizedQuery, false);
			SyncSpoolToDiskLocked();
			delete p;
			m_sem.Release();
			MUTEX_UNLOCK(m_mtxQuery.get());
			return;
		}

		if (bQueueFull || !m_dequeSpool.empty())
		{
			std::string sanitizedQuery = SanitizeQueryForLog(p->stQuery);
			EnqueueToSpoolLocked(seqId, sanitizedQuery, false);
			SyncSpoolToDiskLocked();
			delete p;
			m_sem.Release();
			MUTEX_UNLOCK(m_mtxQuery.get());
			return;
		}

		m_queue_query.push(p);
		m_queryQueueBytes += queryBytes;
		m_sem.Release();
		MUTEX_UNLOCK(m_mtxQuery.get());
		return;
	}

	size_t effectiveMaxCap = m_bDurableSpoolEnabled ? m_maxQueryQueueCount : MAX_ASYNC_QUERY_QUEUE_CAPACITY;

	if (m_queue_query.size() >= effectiveMaxCap || (m_queryQueueBytes + queryBytes) > MAX_ASYNC_QUERY_QUEUE_BYTES)
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
	DWORD count = static_cast<DWORD>(m_queue_query.size() + m_dequeSpool.size());
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
	if (m_bDurableSpoolEnabled)
	{
		DurableChildLoop();
		return;
	}

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

size_t CAsyncSQL::EscapeStringOfflineSafe(char * dst, size_t dstSize, const char * src, size_t srcSize, bool bNoBackslashEscapes)
{
	if (!dst || dstSize == 0)
		return 0;

	if (!src || srcSize == 0)
	{
		dst[0] = '\0';
		return 0;
	}

	if (dstSize < srcSize * 2 + 1)
	{
		dst[0] = '\0';
		return 0;
	}

	size_t outPos = 0;
	if (bNoBackslashEscapes)
	{
		// When SQL_MODE includes NO_BACKSLASH_ESCAPES, backslash is a literal character
		// and single quotes MUST be escaped by doubling (' -> '').
		for (size_t i = 0; i < srcSize; ++i)
		{
			unsigned char ch = static_cast<unsigned char>(src[i]);
			if (ch == '\'')
			{
				dst[outPos++] = '\'';
				dst[outPos++] = '\'';
			}
			else if (ch == '\0' || ch == '\x1a')
			{
				dst[outPos++] = ' ';
			}
			else
			{
				dst[outPos++] = static_cast<char>(ch);
			}
		}
		dst[outPos] = '\0';
		return outPos;
	}

	for (size_t i = 0; i < srcSize; ++i)
	{
		unsigned char ch = static_cast<unsigned char>(src[i]);
		char esc = 0;
		switch (ch)
		{
			case '\0': esc = '0'; break;
			case '\n': esc = 'n'; break;
			case '\r': esc = 'r'; break;
			case '\\': esc = '\\'; break;
			case '\'': esc = '\''; break;
			case '\"': esc = '\"'; break;
			case '\x1a': esc = 'Z'; break;
			default: break;
		}

		if (esc != 0)
		{
			dst[outPos++] = '\\';
			dst[outPos++] = esc;
		}
		else
		{
			dst[outPos++] = static_cast<char>(ch);
		}
	}

	dst[outPos] = '\0';
	return outPos;
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
		if (m_bDurableSpoolEnabled)
			return EscapeStringOfflineSafe(dst, dstSize, src, srcSize, m_bNoBackslashEscapes);

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
		if (m_bDurableSpoolEnabled)
			return EscapeStringOfflineSafe(dst, dstSize, src, srcSize, m_bNoBackslashEscapes);

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

bool CAsyncSQL::IsConnectionLossError(unsigned int uiErrno)
{
	return uiErrno == CR_SERVER_LOST ||
		   uiErrno == CR_SERVER_GONE_ERROR ||
		   uiErrno == CR_CONNECTION_ERROR ||
		   uiErrno == CR_CONN_HOST_ERROR ||
		   uiErrno == 1053 /* ER_SERVER_SHUTDOWN */ ||
		   uiErrno == 1927 /* ER_CONNECTION_KILLED */;
}

uint32_t CAsyncSQL::ComputeSpoolChecksum(uint64_t ullSeqID, const std::string & stPayload)
{
	uint32_t hash = 2166136261u;
	for (int i = 0; i < 8; ++i)
	{
		uint8_t byte = static_cast<uint8_t>((ullSeqID >> (i * 8)) & 0xFFu);
		hash ^= byte;
		hash *= 16777619u;
	}
	for (unsigned char ch : stPayload)
	{
		hash ^= ch;
		hash *= 16777619u;
	}
	return hash;
}

std::string CAsyncSQL::EscapeSpoolPayload(const std::string & stRaw)
{
	std::string out;
	out.reserve(stRaw.size() + 16);
	for (char c : stRaw)
	{
		switch (c)
		{
			case '\\': out += "\\\\"; break;
			case '\n': out += "\\n"; break;
			case '\r': out += "\\r"; break;
			case '|':  out += "\\p"; break;
			default:   out += c; break;
		}
	}
	return out;
}

bool CAsyncSQL::UnescapeSpoolPayload(const std::string & stEscaped, std::string & stOut)
{
	stOut.clear();
	stOut.reserve(stEscaped.size());
	for (size_t i = 0; i < stEscaped.size(); ++i)
	{
		if (stEscaped[i] == '\\')
		{
			if (i + 1 >= stEscaped.size())
				return false;
			char next = stEscaped[++i];
			switch (next)
			{
				case '\\': stOut += '\\'; break;
				case 'n':  stOut += '\n'; break;
				case 'r':  stOut += '\r'; break;
				case 'p':  stOut += '|';  break;
				default:   return false;
			}
		}
		else
		{
			stOut += stEscaped[i];
		}
	}
	return true;
}

bool CAsyncSQL::EnableDurableSpool(const char * c_pszSpoolPath, size_t maxMemoryQueueCount, size_t maxSpoolBytes, DWORD dwReconnectIntervalMs)
{
	if (!m_mtxQuery)
		return false;

	MUTEX_LOCK(m_mtxQuery.get());
	m_bDurableSpoolEnabled = true;
	m_stSpoolFilePath = (c_pszSpoolPath && *c_pszSpoolPath) ? c_pszSpoolPath : "sql_log_spool.dat";
	m_maxQueryQueueCount = maxMemoryQueueCount > 0 ? maxMemoryQueueCount : 10000;
	m_maxSpoolBytes = maxSpoolBytes > 0 ? maxSpoolBytes : (64 * 1024 * 1024);
	m_dwReconnectIntervalMs = dwReconnectIntervalMs > 0 ? dwReconnectIntervalMs : 1000;

	bool bLoaded = LoadSpoolFromDiskLocked();
	if (m_dequeSpool.empty())
	{
		SyncSpoolToDiskLocked();
	}
	else
	{
		m_bSpoolingActive = true;
		m_sem.Release();
	}
	MUTEX_UNLOCK(m_mtxQuery.get());
	return bLoaded;
}

void CAsyncSQL::SetMaxQueryQueueCount(size_t maxCount)
{
	if (!m_mtxQuery)
		return;
	MUTEX_LOCK(m_mtxQuery.get());
	m_maxQueryQueueCount = maxCount > 0 ? maxCount : 1;
	MUTEX_UNLOCK(m_mtxQuery.get());
}

void CAsyncSQL::SetMaxSpoolBytes(size_t maxBytes)
{
	if (!m_mtxQuery)
		return;
	MUTEX_LOCK(m_mtxQuery.get());
	m_maxSpoolBytes = maxBytes > 0 ? maxBytes : 1024;
	MUTEX_UNLOCK(m_mtxQuery.get());
}

void CAsyncSQL::SetReconnectIntervalMs(DWORD dwIntervalMs)
{
	if (!m_mtxQuery)
		return;
	MUTEX_LOCK(m_mtxQuery.get());
	m_dwReconnectIntervalMs = dwIntervalMs > 0 ? dwIntervalMs : 50;
	MUTEX_UNLOCK(m_mtxQuery.get());
}

CAsyncSQL::TDurableSpoolStats CAsyncSQL::GetDurableSpoolStats()
{
	TDurableSpoolStats stats;
	if (!m_mtxQuery)
		return stats;

	MUTEX_LOCK(m_mtxQuery.get());
	stats.ullSubmittedCount = m_ullSubmittedCount;
	stats.ullSpooledCount = m_ullSpooledCount;
	stats.ullPendingSpoolCount = static_cast<uint64_t>(m_dequeSpool.size());
	stats.ullPendingMemoryCount = static_cast<uint64_t>(m_queue_query.size() + m_queue_query_copy.size());
	stats.ullReplayedCount = m_ullReplayedCount;
	stats.ullCommittedCount = m_ullCommittedCount;
	stats.ullQuarantinedCount = m_ullQuarantinedCount;
	stats.ullDroppedSpoolFullCount = m_ullDroppedSpoolFullCount;
	stats.ullDiskSyncFailureCount = m_ullDiskSyncFailureCount;
	stats.ullUncertainInFlightRetries = m_ullUncertainInFlightRetries;
	stats.ullReconnectAttempts = m_ullReconnectAttempts;
	stats.ullReconnectSuccessCount = m_ullReconnectSuccessCount;
	stats.spoolBytes = m_spoolBytes;
	MUTEX_UNLOCK(m_mtxQuery.get());
	return stats;
}

bool CAsyncSQL::LoadSpoolFromDiskLocked()
{
	m_dequeSpool.clear();
	m_spoolBytes = 0;

	if (m_stSpoolFilePath.empty())
		return false;

	std::ifstream ifs(m_stSpoolFilePath, std::ios::in | std::ios::binary);
	if (!ifs.is_open())
		return true;

	std::string line;
	size_t lineNo = 0;
	while (std::getline(ifs, line))
	{
		++lineNo;
		if (!line.empty() && line.back() == '\r')
			line.pop_back();
		if (line.empty())
			continue;

		size_t p1 = line.find('|');
		size_t p2 = (p1 != std::string::npos) ? line.find('|', p1 + 1) : std::string::npos;
		size_t p3 = (p2 != std::string::npos) ? line.find('|', p2 + 1) : std::string::npos;
		if (p1 == std::string::npos || p2 == std::string::npos || p3 == std::string::npos || line.compare(0, p1, "SPOOL1") != 0)
		{
			sys_err("AsyncSQL::LoadSpoolFromDisk: malformed header at line %zu in %s; quarantining record.", lineNo, m_stSpoolFilePath.c_str());
			++m_ullQuarantinedCount;
			continue;
		}

		std::string seqStr = line.substr(p1 + 1, p2 - p1 - 1);
		std::string crcStr = line.substr(p2 + 1, p3 - p2 - 1);
		if (seqStr.empty() || crcStr.empty())
		{
			sys_err("AsyncSQL::LoadSpoolFromDisk: empty seq/crc at line %zu in %s; quarantining record.", lineNo, m_stSpoolFilePath.c_str());
			++m_ullQuarantinedCount;
			continue;
		}

		char * endSeq = nullptr;
		char * endCrc = nullptr;
		uint64_t seqId = static_cast<uint64_t>(strtoull(seqStr.c_str(), &endSeq, 10));
		uint32_t expectedCrc = static_cast<uint32_t>(strtoul(crcStr.c_str(), &endCrc, 16));
		if (!endSeq || *endSeq != '\0' || !endCrc || *endCrc != '\0')
		{
			sys_err("AsyncSQL::LoadSpoolFromDisk: invalid seq/crc at line %zu in %s; quarantining record.", lineNo, m_stSpoolFilePath.c_str());
			++m_ullQuarantinedCount;
			continue;
		}

		std::string unescaped;
		if (!UnescapeSpoolPayload(line.substr(p3 + 1), unescaped))
		{
			sys_err("AsyncSQL::LoadSpoolFromDisk: corrupt payload escape at line %zu in %s; quarantining record.", lineNo, m_stSpoolFilePath.c_str());
			++m_ullQuarantinedCount;
			continue;
		}

		std::string sanitized = SanitizeQueryForLog(unescaped);
		uint32_t actualCrc = ComputeSpoolChecksum(seqId, unescaped);
		if (actualCrc != expectedCrc)
		{
			sys_err("AsyncSQL::LoadSpoolFromDisk: checksum mismatch at line %zu in %s; quarantining record.", lineNo, m_stSpoolFilePath.c_str());
			++m_ullQuarantinedCount;
			continue;
		}

		if (m_spoolBytes + sanitized.size() > m_maxSpoolBytes)
		{
			sys_err("CRITICAL ALERT: AsyncSQL::LoadSpoolFromDisk: spool file exceeds maxSpoolBytes (%zu > %zu) at line %zu!",
					m_spoolBytes + sanitized.size(), m_maxSpoolBytes, lineNo);
			++m_ullDroppedSpoolFullCount;
			m_bFailedQueueOverflow = true;
			if (m_pfnSpoolOverflowCallback)
				m_pfnSpoolOverflowCallback(seqId, m_spoolBytes, m_maxSpoolBytes, m_ullDroppedSpoolFullCount);
			if (m_eSpoolOverflowPolicy == SPOOL_OVERFLOW_FAIL_STOP_SHUTDOWN)
			{
				m_bEnd = true;
				sys_err("CRITICAL FAIL-STOP: AsyncSQL durable spool overflow policy SPOOL_OVERFLOW_FAIL_STOP_SHUTDOWN triggered during disk load!");
			}
			continue;
		}

		m_dequeSpool.push_back(TDurableSpoolEntry{seqId, sanitized});
		m_spoolBytes += sanitized.size();
		if (seqId > m_ullSubmittedSeq)
			m_ullSubmittedSeq = seqId;
	}

	return true;
}

bool CAsyncSQL::SyncSpoolToDiskLocked()
{
	if (!m_bDurableSpoolEnabled || m_stSpoolFilePath.empty())
		return false;

	std::string tmpPath = m_stSpoolFilePath + ".tmp";

#ifndef __WIN32__
	int fd = ::open(tmpPath.c_str(), O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
	if (fd < 0)
	{
		++m_ullDiskSyncFailureCount;
		m_bFailedQueueOverflow = true;
		sys_err("CRITICAL ALERT: AsyncSQL::SyncSpoolToDiskLocked: failed to open temp spool file %s", tmpPath.c_str());
		return false;
	}
	::fchmod(fd, S_IRUSR | S_IWUSR);
	FILE * fp = ::fdopen(fd, "wb");
	if (!fp)
	{
		::close(fd);
		std::remove(tmpPath.c_str());
		++m_ullDiskSyncFailureCount;
		m_bFailedQueueOverflow = true;
		return false;
	}
#else
	FILE * fp = std::fopen(tmpPath.c_str(), "wb");
	if (!fp)
	{
		++m_ullDiskSyncFailureCount;
		m_bFailedQueueOverflow = true;
		sys_err("CRITICAL ALERT: AsyncSQL::SyncSpoolToDiskLocked: failed to open temp spool file %s", tmpPath.c_str());
		return false;
	}
	EnforceSpoolFilePermissions(tmpPath);
#endif

	bool bWriteOk = true;
	for (const auto & entry : m_dequeSpool)
	{
		std::string sanitized = SanitizeQueryForLog(entry.stQuery);
		uint32_t crc = ComputeSpoolChecksum(entry.ullSeqID, sanitized);
		std::string escaped = EscapeSpoolPayload(sanitized);

		int written = std::fprintf(fp, "SPOOL1|%llu|%08x|%s\n",
								   static_cast<unsigned long long>(entry.ullSeqID),
								   crc,
								   escaped.c_str());
		if (written <= 0 || std::ferror(fp))
		{
			bWriteOk = false;
			break;
		}
	}

	if (bWriteOk && !SyncFileDescriptor(fp))
		bWriteOk = false;

	if (std::fclose(fp) != 0)
		bWriteOk = false;

	if (!bWriteOk)
	{
		std::remove(tmpPath.c_str());
		++m_ullDiskSyncFailureCount;
		m_bFailedQueueOverflow = true;
		sys_err("CRITICAL ALERT: AsyncSQL::SyncSpoolToDiskLocked: disk write/fsync error on %s; existing spool file preserved!", tmpPath.c_str());
		return false;
	}

#ifndef __WIN32__
	if (::rename(tmpPath.c_str(), m_stSpoolFilePath.c_str()) != 0)
	{
		std::remove(tmpPath.c_str());
		++m_ullDiskSyncFailureCount;
		m_bFailedQueueOverflow = true;
		sys_err("CRITICAL ALERT: AsyncSQL::SyncSpoolToDiskLocked: atomic rename failed for %s!", m_stSpoolFilePath.c_str());
		return false;
	}
#else
	if (!::MoveFileExA(tmpPath.c_str(), m_stSpoolFilePath.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
	{
		std::remove(tmpPath.c_str());
		++m_ullDiskSyncFailureCount;
		m_bFailedQueueOverflow = true;
		sys_err("CRITICAL ALERT: AsyncSQL::SyncSpoolToDiskLocked: MoveFileExA failed for %s!", m_stSpoolFilePath.c_str());
		return false;
	}
#endif

	EnforceSpoolFilePermissions(m_stSpoolFilePath);
	return true;
}

bool CAsyncSQL::AppendQuarantineRecordLocked(uint64_t ullSeqID, unsigned int uiErrno, const std::string & stQuery)
{
	if (m_stSpoolFilePath.empty())
		return false;

	std::string qPath = m_stSpoolFilePath + ".quarantine";
	FILE * fp = std::fopen(qPath.c_str(), "ab");
	if (!fp)
		return false;

	EnforceSpoolFilePermissions(qPath);
	std::string sanitized = SanitizeQueryForLog(stQuery);
	std::string escaped = EscapeSpoolPayload(sanitized);
	std::fprintf(fp, "QUARANTINE1|%llu|%u|%s\n",
				 static_cast<unsigned long long>(ullSeqID),
				 uiErrno,
				 escaped.c_str());
	SyncFileDescriptor(fp);
	std::fclose(fp);
	EnforceSpoolFilePermissions(qPath);
	return true;
}

bool CAsyncSQL::EnqueueToSpoolLocked(uint64_t ullSeqID, const std::string & stSanitizedQuery, bool bFront)
{
	if (m_spoolBytes + stSanitizedQuery.size() > m_maxSpoolBytes)
	{
		m_bFailedQueueOverflow = true;
		++m_ullDroppedSpoolFullCount;
		sys_err("CRITICAL ALERT: AsyncSQL durable spool capacity reached (%zu + %zu > %zu bytes)! Query seq=%llu cannot be spooled.",
				m_spoolBytes, stSanitizedQuery.size(), m_maxSpoolBytes, static_cast<unsigned long long>(ullSeqID));
		if (m_pfnSpoolOverflowCallback)
			m_pfnSpoolOverflowCallback(ullSeqID, m_spoolBytes, m_maxSpoolBytes, m_ullDroppedSpoolFullCount);
		if (m_eSpoolOverflowPolicy == SPOOL_OVERFLOW_FAIL_STOP_SHUTDOWN)
		{
			m_bEnd = true;
			sys_err("CRITICAL FAIL-STOP: AsyncSQL durable spool overflow policy SPOOL_OVERFLOW_FAIL_STOP_SHUTDOWN triggered (seq=%llu)!",
					static_cast<unsigned long long>(ullSeqID));
		}
		return false;
	}

	if (bFront)
	{
		m_dequeSpool.push_front(TDurableSpoolEntry{ullSeqID, stSanitizedQuery});
	}
	else
	{
		auto it = m_dequeSpool.end();
		while (it != m_dequeSpool.begin())
		{
			auto prev = it - 1;
			if (prev->ullSeqID <= ullSeqID)
				break;
			it = prev;
		}
		m_dequeSpool.insert(it, TDurableSpoolEntry{ullSeqID, stSanitizedQuery});
	}

	m_spoolBytes += stSanitizedQuery.size();
	++m_ullSpooledCount;
	m_bSpoolingActive = true;
	return true;
}

void CAsyncSQL::DrainMemoryQueueToSpoolLocked()
{
	while (!m_queue_query_copy.empty())
	{
		SQLMsg * p = m_queue_query_copy.front();
		m_queue_query_copy.pop();
		if (p)
		{
			uint64_t seqId = p->ullSeqID != 0 ? p->ullSeqID : ++m_ullSubmittedSeq;
			EnqueueToSpoolLocked(seqId, SanitizeQueryForLog(p->stQuery), false);
			delete p;
		}
	}

	while (!m_queue_query.empty())
	{
		SQLMsg * p = m_queue_query.front();
		m_queue_query.pop();
		if (p)
		{
			uint64_t seqId = p->ullSeqID != 0 ? p->ullSeqID : ++m_ullSubmittedSeq;
			EnqueueToSpoolLocked(seqId, SanitizeQueryForLog(p->stQuery), false);
			delete p;
		}
	}

	m_queryQueueBytes = 0;
}

bool CAsyncSQL::ReplayDurableSpoolNow()
{
	if (!m_bDurableSpoolEnabled || !m_mtxQuery)
		return false;

	while (true)
	{
		MUTEX_LOCK(m_mtxQuery.get());

		if (!m_bConnected)
		{
			if (!m_queue_query.empty() || !m_queue_query_copy.empty())
			{
				DrainMemoryQueueToSpoolLocked();
				SyncSpoolToDiskLocked();
			}
			MUTEX_UNLOCK(m_mtxQuery.get());
			return false;
		}

		bool hasMemory = !m_queue_query.empty();
		bool hasSpool = !m_dequeSpool.empty();
		if (!hasMemory && !hasSpool)
		{
			m_bSpoolingActive = false;
			MUTEX_UNLOCK(m_mtxQuery.get());
			return true;
		}

		bool takeMemory = false;
		if (hasMemory && hasSpool)
		{
			takeMemory = (m_queue_query.front()->ullSeqID < m_dequeSpool.front().ullSeqID);
		}
		else if (hasMemory)
		{
			takeMemory = true;
		}

		if (takeMemory)
		{
			SQLMsg * p = m_queue_query.front();
			m_queue_query.pop();
			size_t queryBytes = sizeof(SQLMsg) + p->stQuery.capacity();
			if (m_queryQueueBytes >= queryBytes)
				m_queryQueueBytes -= queryBytes;
			else
				m_queryQueueBytes = 0;
			++m_iCopiedQuery;
			MUTEX_UNLOCK(m_mtxQuery.get());

			if (m_ulThreadID != mysql_thread_id(&m_hDB))
			{
				while (!QueryLocaleSet())
				{
					if (!m_bConnected)
						break;
				}
				m_ulThreadID = mysql_thread_id(&m_hDB);
			}

			if (mysql_real_query(&m_hDB, p->stQuery.c_str(), p->stQuery.length()))
			{
				p->uiSQLErrno = mysql_errno(&m_hDB);
				sys_err("AsyncSQL: durable query failed: %s (query: %s errno: %u)",
						mysql_error(&m_hDB), SanitizeQueryForLog(p->stQuery).c_str(), p->uiSQLErrno);

				if (IsConnectionLossError(p->uiSQLErrno))
				{
					m_bConnected = false;
					MUTEX_LOCK(m_mtxQuery.get());
					if (p->uiSQLErrno == CR_SERVER_LOST || p->uiSQLErrno == 1053 || p->uiSQLErrno == 1927)
					{
						++m_ullUncertainInFlightRetries;
						sys_err("AsyncSQL: mid-flight disconnect (errno=%u) on seq=%llu; re-spooling for at-least-once delivery (potential duplicate if server committed before ACK loss).",
								p->uiSQLErrno, static_cast<unsigned long long>(p->ullSeqID));
					}
					EnqueueToSpoolLocked(p->ullSeqID, SanitizeQueryForLog(p->stQuery), true);
					DrainMemoryQueueToSpoolLocked();
					SyncSpoolToDiskLocked();
					MUTEX_UNLOCK(m_mtxQuery.get());
					delete p;
					return false;
				}
				else
				{
					MUTEX_LOCK(m_mtxQuery.get());
					++m_ullQuarantinedCount;
					++m_iQueryFinished;
					AppendQuarantineRecordLocked(p->ullSeqID, p->uiSQLErrno, p->stQuery);
					MUTEX_UNLOCK(m_mtxQuery.get());
					delete p;
					continue;
				}
			}

			do
			{
				MYSQL_RES * pRes = mysql_store_result(&m_hDB);
				if (pRes)
					mysql_free_result(pRes);
			} while (!mysql_next_result(&m_hDB));

			delete p;
			MUTEX_LOCK(m_mtxQuery.get());
			++m_ullCommittedCount;
			++m_iQueryFinished;
			MUTEX_UNLOCK(m_mtxQuery.get());
		}
		else
		{
			TDurableSpoolEntry entry = m_dequeSpool.front();
			MUTEX_UNLOCK(m_mtxQuery.get());

			if (m_ulThreadID != mysql_thread_id(&m_hDB))
			{
				while (!QueryLocaleSet())
				{
					if (!m_bConnected)
						break;
				}
				m_ulThreadID = mysql_thread_id(&m_hDB);
			}

			if (mysql_real_query(&m_hDB, entry.stQuery.c_str(), entry.stQuery.length()))
			{
				unsigned int err = mysql_errno(&m_hDB);
				sys_err("AsyncSQL: spool replay query failed: %s (seq: %llu query: %s errno: %u)",
						mysql_error(&m_hDB), static_cast<unsigned long long>(entry.ullSeqID),
						SanitizeQueryForLog(entry.stQuery).c_str(), err);

				if (IsConnectionLossError(err))
				{
					m_bConnected = false;
					MUTEX_LOCK(m_mtxQuery.get());
					if (err == CR_SERVER_LOST || err == 1053 || err == 1927)
					{
						++m_ullUncertainInFlightRetries;
						sys_err("AsyncSQL: mid-flight disconnect (errno=%u) during spool replay on seq=%llu; retaining at spool head for at-least-once delivery.",
								err, static_cast<unsigned long long>(entry.ullSeqID));
					}
					DrainMemoryQueueToSpoolLocked();
					SyncSpoolToDiskLocked();
					MUTEX_UNLOCK(m_mtxQuery.get());
					return false;
				}
				else
				{
					MUTEX_LOCK(m_mtxQuery.get());
					if (!m_dequeSpool.empty() && m_dequeSpool.front().ullSeqID == entry.ullSeqID)
					{
						if (m_spoolBytes >= m_dequeSpool.front().stQuery.size())
							m_spoolBytes -= m_dequeSpool.front().stQuery.size();
						else
							m_spoolBytes = 0;
						m_dequeSpool.pop_front();
					}
					++m_ullQuarantinedCount;
					++m_iQueryFinished;
					AppendQuarantineRecordLocked(entry.ullSeqID, err, entry.stQuery);
					if (m_dequeSpool.empty())
						m_bSpoolingActive = false;
					SyncSpoolToDiskLocked();
					MUTEX_UNLOCK(m_mtxQuery.get());
					continue;
				}
			}

			do
			{
				MYSQL_RES * pRes = mysql_store_result(&m_hDB);
				if (pRes)
					mysql_free_result(pRes);
			} while (!mysql_next_result(&m_hDB));

			MUTEX_LOCK(m_mtxQuery.get());
			if (!m_dequeSpool.empty() && m_dequeSpool.front().ullSeqID == entry.ullSeqID)
			{
				if (m_spoolBytes >= m_dequeSpool.front().stQuery.size())
					m_spoolBytes -= m_dequeSpool.front().stQuery.size();
				else
					m_spoolBytes = 0;
				m_dequeSpool.pop_front();
			}
			++m_ullReplayedCount;
			++m_ullCommittedCount;
			++m_iQueryFinished;
			if (m_dequeSpool.empty())
				m_bSpoolingActive = false;
			SyncSpoolToDiskLocked();
			MUTEX_UNLOCK(m_mtxQuery.get());
		}
	}
}

void CAsyncSQL::DurableChildLoop()
{
	while (!m_bEnd)
	{
		if (!m_bConnected)
		{
			MUTEX_LOCK(m_mtxQuery.get());
			if (!m_queue_query.empty() || !m_queue_query_copy.empty())
			{
				DrainMemoryQueueToSpoolLocked();
				SyncSpoolToDiskLocked();
			}
			DWORD intervalMs = m_dwReconnectIntervalMs;
			uint64_t lastAttemptMs = m_ullLastReconnectAttemptMs;
			MUTEX_UNLOCK(m_mtxQuery.get());

			uint64_t nowMs = GetMonotonicMs();
			if (lastAttemptMs == 0 || (nowMs - lastAttemptMs) >= intervalMs)
			{
				MUTEX_LOCK(m_mtxQuery.get());
				++m_ullReconnectAttempts;
				m_ullLastReconnectAttemptMs = nowMs;
				MUTEX_UNLOCK(m_mtxQuery.get());

				if (Reconnect())
				{
					MUTEX_LOCK(m_mtxQuery.get());
					++m_ullReconnectSuccessCount;
					MUTEX_UNLOCK(m_mtxQuery.get());
					m_ulThreadID = mysql_thread_id(&m_hDB);
					continue;
				}
			}

			DWORD sleepStepMs = std::min<DWORD>(intervalMs, 50);
			if (sleepStepMs < 10)
				sleepStepMs = 10;
#ifndef __WIN32__
			usleep(sleepStepMs * 1000);
#else
			::Sleep(sleepStepMs);
#endif
			continue;
		}

		ReplayDurableSpoolNow();

		if (!m_bConnected || m_bEnd)
			continue;

		MUTEX_LOCK(m_mtxQuery.get());
		bool bIdle = m_queue_query.empty() && m_dequeSpool.empty() && m_bConnected;
		MUTEX_UNLOCK(m_mtxQuery.get());

		if (bIdle && !m_bEnd)
		{
			m_sem.Wait();
		}
	}

	// Controlled shutdown drain: flush to MariaDB if connected, otherwise persist all remaining queries to disk spool
	if (m_bConnected)
	{
		ReplayDurableSpoolNow();
	}

	MUTEX_LOCK(m_mtxQuery.get());
	if (!m_queue_query.empty() || !m_queue_query_copy.empty())
	{
		DrainMemoryQueueToSpoolLocked();
	}
	SyncSpoolToDiskLocked();
	MUTEX_UNLOCK(m_mtxQuery.get());
}

bool CAsyncSQL::FlushAndSpoolOnShutdown(DWORD dwMaxWaitMs)
{
	if (!m_bDurableSpoolEnabled)
	{
		Quit();
		return true;
	}

#ifndef __WIN32__
	bool bHasThread = (m_hThread != 0);
#else
	bool bHasThread = (m_hThread != INVALID_HANDLE_VALUE);
#endif

	if (bHasThread)
	{
		m_sem.Release();
		uint64_t startMs = GetMonotonicMs();
		while ((GetMonotonicMs() - startMs) < dwMaxWaitMs)
		{
			MUTEX_LOCK(m_mtxQuery.get());
			bool bMemEmpty = m_queue_query.empty() && m_queue_query_copy.empty();
			bool bSpoolEmpty = m_dequeSpool.empty();
			bool bConn = m_bConnected;
			MUTEX_UNLOCK(m_mtxQuery.get());

			if (bConn && bMemEmpty && bSpoolEmpty)
				break;
			if (!bConn && bMemEmpty)
				break;

			m_sem.Release();
#ifndef __WIN32__
			usleep(20000);
#else
			::Sleep(20);
#endif
		}

		Quit();
	}
	else
	{
		if (m_bConnected)
			ReplayDurableSpoolNow();
	}

	if (m_mtxQuery)
	{
		MUTEX_LOCK(m_mtxQuery.get());
		if (!m_queue_query.empty() || !m_queue_query_copy.empty())
			DrainMemoryQueueToSpoolLocked();
		SyncSpoolToDiskLocked();
		MUTEX_UNLOCK(m_mtxQuery.get());
	}

	return true;
}
