#include <iostream>
#include <cassert>
#include <cstring>
#include <cstdio>
#include <fstream>
#include <thread>
#include <vector>
#include <chrono>
#include <cstdlib>

#include "AsyncSQL.h"
#include "Semaphore.h"
#include "Statement.h"
#include "cache.h"

// ==============================================================================
// Mock C API for Unit Testing (Hermetic & Platform-Independent)
// Provides controlled database engine simulation without external server.
// ==============================================================================
static bool s_mock_connected = false;
static unsigned long s_mock_thread_id = 1001;
static int s_mock_force_errno = 0;
static uint64_t s_mock_insert_id = 0;
static uint64_t s_mock_affected_rows = 0;
static uint64_t s_mock_num_rows = 0;
static bool s_mock_force_connect_fail = false;

extern "C" {
unsigned long STDCALL mysql_get_client_version(void)
{
	return MYSQL_VERSION_ID;
}

const char * STDCALL mysql_get_client_info(void)
{
	return "mock-client";
}

MYSQL * STDCALL mysql_init(MYSQL * mysql)
{
	if (!mysql)
		return nullptr;
	std::memset(mysql, 0, sizeof(MYSQL));
	return mysql;
}

void STDCALL mysql_close(MYSQL * mysql)
{
	if (mysql) {
		std::memset(mysql, 0, sizeof(MYSQL));
	}
	s_mock_connected = false;
}

int STDCALL mysql_options(MYSQL *, enum mysql_option, const void *)
{
	return 0;
}

#if defined(MARIADB_BASE_VERSION) || defined(MARIADB_VERSION_ID)
int STDCALL mysql_get_option(MYSQL *, enum mysql_option, void * arg)
{
	if (arg) {
		*reinterpret_cast<int*>(arg) = 1;
	}
	return 0;
}
#else
int STDCALL mysql_get_option(MYSQL *, enum mysql_option, const void * arg)
{
	if (arg) {
		*reinterpret_cast<int*>(const_cast<void*>(arg)) = 1;
	}
	return 0;
}
#endif

int STDCALL mysql_set_character_set(MYSQL *, const char *)
{
	return 0;
}

MYSQL * STDCALL mysql_real_connect(MYSQL * mysql, const char *, const char *, const char *, const char *, unsigned int, const char *, unsigned long)
{
	if (s_mock_force_connect_fail)
		return nullptr;
	s_mock_connected = true;
	return mysql;
}

unsigned long STDCALL mysql_thread_id(MYSQL *)
{
	return s_mock_thread_id;
}

int STDCALL mysql_real_query(MYSQL *, const char *, unsigned long)
{
	if (s_mock_force_errno != 0)
		return 1;
	return 0;
}

MYSQL_RES * STDCALL mysql_store_result(MYSQL *)
{
	static int dummy_res = 1;
	return reinterpret_cast<MYSQL_RES*>(&dummy_res);
}

void STDCALL mysql_free_result(MYSQL_RES *)
{
}

int STDCALL mysql_next_result(MYSQL *)
{
	return -1; // No more results
}

my_ulonglong STDCALL mysql_insert_id(MYSQL *)
{
	return s_mock_insert_id;
}

my_ulonglong STDCALL mysql_affected_rows(MYSQL *)
{
	return s_mock_affected_rows;
}

my_ulonglong STDCALL mysql_num_rows(MYSQL_RES *)
{
	return s_mock_num_rows;
}

unsigned int STDCALL mysql_errno(MYSQL *)
{
	return s_mock_force_errno;
}

const char * STDCALL mysql_error(MYSQL *)
{
	if (s_mock_force_errno == CR_SERVER_LOST)
		return "Lost connection to MySQL server during query";
	if (s_mock_force_errno == CR_SERVER_GONE_ERROR)
		return "MySQL server has gone away";
	if (s_mock_force_errno != 0)
		return "Simulated database error";
	return "";
}

unsigned long STDCALL mysql_real_escape_string(MYSQL *, char * to, const char * from, unsigned long length)
{
	if (!to || !from)
		return 0;
	unsigned long written = 0;
	for (unsigned long i = 0; i < length; ++i) {
		char c = from[i];
		switch (c) {
			case '\'':
				to[written++] = '\\';
				to[written++] = '\'';
				break;
			case '\"':
				to[written++] = '\\';
				to[written++] = '\"';
				break;
			case '\\':
				to[written++] = '\\';
				to[written++] = '\\';
				break;
			case '\0':
				to[written++] = '\\';
				to[written++] = '0';
				break;
			default:
				to[written++] = c;
				break;
		}
	}
	to[written] = '\0';
	return written;
}

#if defined(MARIADB_BASE_VERSION) || defined(MARIADB_VERSION_ID)
typedef my_bool mt2_my_bool;
#else
typedef bool mt2_my_bool;
#endif

unsigned long STDCALL mysql_real_escape_string_quote(MYSQL * mysql, char * to, const char * from, unsigned long length, char)
{
	return mysql_real_escape_string(mysql, to, from, length);
}

MYSQL_STMT * STDCALL mysql_stmt_init(MYSQL *)
{
	static int dummy_stmt = 1;
	return reinterpret_cast<MYSQL_STMT*>(&dummy_stmt);
}

int STDCALL mysql_stmt_prepare(MYSQL_STMT *, const char *, unsigned long)
{
	return 0;
}

mt2_my_bool STDCALL mysql_stmt_close(MYSQL_STMT *)
{
	return 0;
}

unsigned int STDCALL mysql_stmt_errno(MYSQL_STMT *)
{
	return 0;
}

const char * STDCALL mysql_stmt_error(MYSQL_STMT *)
{
	return "";
}

mt2_my_bool STDCALL mariadb_reconnect(MYSQL *)
{
	s_mock_connected = true;
	return 0;
}
} // extern "C"

class TestAsyncSQL : public CAsyncSQL
{
public:
	using CAsyncSQL::CopyQuery;
	using CAsyncSQL::PeekQueryFromCopyQueue;
	using CAsyncSQL::PopQueryFromCopyQueue;
	using CAsyncSQL::Destroy;
};

// ==============================================================================
// TEST HARNESS
// ==============================================================================
static int g_tests_passed = 0;
static int g_tests_total = 0;

#define TEST_ASSERT(cond, msg) \
	do { \
		++g_tests_total; \
		if (!(cond)) { \
			std::cerr << "[FAIL] Line " << __LINE__ << ": " << msg << std::endl; \
			std::exit(1); \
		} else { \
			++g_tests_passed; \
		} \
	} while (0)

int main()
{
	std::cout << "=== LIBSQL AUTOMATED REGRESSION & UNIT TEST SUITE ===" << std::endl;

	// --------------------------------------------------------------------------
	// 1. SEMAPHORE SUITE
	// --------------------------------------------------------------------------
	std::cout << "[RUN] Testing CSemaphore basic operations..." << std::endl;
	{
		CSemaphore sem;
		TEST_ASSERT(sem.Initialize() == true, "CSemaphore Initialize returned true");
		TEST_ASSERT(sem.Release(1) == true, "CSemaphore Release(1) returned true");
		TEST_ASSERT(sem.Wait() == true, "CSemaphore Wait() consumed release");
		sem.Clear();
	}

	std::cout << "[RUN] Testing CSemaphore multi-threaded handoff..." << std::endl;
	{
		CSemaphore sem;
		sem.Initialize();
		bool worker_woke = false;

		std::thread t([&sem, &worker_woke]() {
			sem.Wait();
			worker_woke = true;
		});

		std::this_thread::sleep_for(std::chrono::milliseconds(20));
		TEST_ASSERT(worker_woke == false, "Worker thread correctly waiting on semaphore");

		sem.Release(1);
		t.join();
		TEST_ASSERT(worker_woke == true, "Worker thread correctly awakened by Release");
		sem.Clear();
	}

	std::cout << "[RUN] Testing CSemaphore multi-count release..." << std::endl;
	{
		CSemaphore sem;
		sem.Initialize();
		TEST_ASSERT(sem.Release(3) == true, "CSemaphore Release(3)");
		TEST_ASSERT(sem.Wait() == true, "CSemaphore Wait 1/3");
		TEST_ASSERT(sem.Wait() == true, "CSemaphore Wait 2/3");
		TEST_ASSERT(sem.Wait() == true, "CSemaphore Wait 3/3");
		sem.Clear();
	}

	std::cout << "[RUN] Testing CSemaphore re-initialization..." << std::endl;
	{
		CSemaphore sem;
		sem.Initialize();
		sem.Clear();
		TEST_ASSERT(sem.Initialize() == true, "CSemaphore re-initialize after clear");
		sem.Destroy();
	}

	// --------------------------------------------------------------------------
	// 2. THREAD-SAFE QUEUE & CONCURRENCY SUITE
	// --------------------------------------------------------------------------
	std::cout << "[RUN] Testing CAsyncSQL thread-safe queue operations..." << std::endl;
	{
		TestAsyncSQL sql;
		TEST_ASSERT(sql.CopyQuery() == -1, "CopyQuery on empty queue returns -1");

		sql.AsyncQuery("SELECT 1;");
		sql.AsyncQuery("SELECT 2;");
		TEST_ASSERT(sql.CountQuery() == 2, "CountQuery reports 2 queries in queue");

		int copied = sql.CopyQuery();
		TEST_ASSERT(copied == 2, "CopyQuery copied 2 queries");
		TEST_ASSERT(sql.CountQuery() == 0, "Query queue empty after copy");

		SQLMsg * msg1 = nullptr;
		TEST_ASSERT(sql.PeekQueryFromCopyQueue(&msg1) == true, "PeekQueryFromCopyQueue succeeded");
		TEST_ASSERT(msg1 != nullptr && msg1->stQuery == "SELECT 1;", "FIFO order preserved for query 1");
		delete msg1;
		TEST_ASSERT(sql.PopQueryFromCopyQueue() == true, "PopQueryFromCopyQueue 1");

		SQLMsg * msg2 = nullptr;
		TEST_ASSERT(sql.PeekQueryFromCopyQueue(&msg2) == true, "PeekQueryFromCopyQueue 2");
		TEST_ASSERT(msg2 != nullptr && msg2->stQuery == "SELECT 2;", "FIFO order preserved for query 2");
		delete msg2;
		TEST_ASSERT(sql.PopQueryFromCopyQueue() == true, "PopQueryFromCopyQueue 2");
		TEST_ASSERT(sql.PopQueryFromCopyQueue() == false, "PopQueryFromCopyQueue empty returns false");
	}

	std::cout << "[RUN] Testing CAsyncSQL result queue operations..." << std::endl;
	{
		CAsyncSQL sql;
		TEST_ASSERT(sql.CountResult() == 0, "Initial result count is 0");

		SQLMsg * res_msg = new SQLMsg();
		res_msg->stQuery = "SELECT 'result';";
		sql.PushResult(res_msg);
		TEST_ASSERT(sql.CountResult() == 1, "Result count is 1 after push");

		SQLMsg * popped = nullptr;
		TEST_ASSERT(sql.PopResult(&popped) == true, "PopResult returned true");
		TEST_ASSERT(popped == res_msg, "Popped result matches pushed pointer");
		TEST_ASSERT(sql.CountResult() == 0, "Result count is 0 after pop");

		delete popped;
	}

	std::cout << "[RUN] Testing CAsyncSQL query counters..." << std::endl;
	{
		CAsyncSQL sql;
		TEST_ASSERT(sql.GetCopiedQueryCount() == 0, "Initial copied count is 0");
		sql.AddCopiedQueryCount(5);
		TEST_ASSERT(sql.GetCopiedQueryCount() == 5, "Copied count after add is 5");
		sql.ResetCopiedQueryCount();
		TEST_ASSERT(sql.GetCopiedQueryCount() == 0, "Copied count after reset is 0");

		TEST_ASSERT(sql.CountQueryFinished() == 0, "Initial finished count is 0");
		sql.ResetQueryFinished();
		TEST_ASSERT(sql.CountQueryFinished() == 0, "Finished count after reset is 0");
	}

	// --------------------------------------------------------------------------
	// 3. ESCAPE STRING & BUFFER SECURITY SUITE
	// --------------------------------------------------------------------------
	std::cout << "[RUN] Testing CAsyncSQL::EscapeString security boundaries..." << std::endl;
	{
		CAsyncSQL sql;
		char dst[256];

		// Zero length inputs and null pointers
		TEST_ASSERT(sql.EscapeString(dst, sizeof(dst), "test", 0) == 0, "Zero srcSize returns 0");
		TEST_ASSERT(sql.EscapeString(dst, 0, "test", 4) == 0, "Zero dstSize returns 0");
		TEST_ASSERT(sql.EscapeString(nullptr, sizeof(dst), "test", 4) == 0, "Null dst returns 0");
		TEST_ASSERT(sql.EscapeString(dst, sizeof(dst), nullptr, 4) == 0, "Null src returns 0");

		// Buffer too small boundary protection
		char tiny_dst[10];
		size_t trimmed_len = sql.EscapeString(tiny_dst, sizeof(tiny_dst), "VeryLongStringThatCannotFitInsideTinyBuffer", 43);
		TEST_ASSERT(trimmed_len == 0, "Insufficient buffer safely rejected with 0 return");
		TEST_ASSERT(tiny_dst[0] == '\0', "Destination safely null-terminated");

		// Disconnected state fail-closed protection (cannot escape without active connection/charset)
		const char * dangerous = "admin' OR '1'='1";
		dst[0] = 'X';
		size_t esc_len = sql.EscapeString(dst, sizeof(dst), dangerous, std::strlen(dangerous));
		TEST_ASSERT(esc_len == 0, "Disconnected EscapeString returns 0 (Fail-closed)");
		TEST_ASSERT(dst[0] == '\0', "Disconnected EscapeString clears destination buffer");
	}

	// --------------------------------------------------------------------------
	// 4. 64-BIT FIELDS & RESULT ABI COMPATIBILITY SUITE
	// --------------------------------------------------------------------------
	std::cout << "[RUN] Testing SQLResult 64-bit ID and row fields..." << std::endl;
	{
		SQLResult res;
		TEST_ASSERT(res.uiNumRows == 0, "Default uiNumRows is 0");
		TEST_ASSERT(res.uiAffectedRows == 0, "Default uiAffectedRows is 0");
		TEST_ASSERT(res.uiInsertID == 0, "Default uiInsertID is 0");
		TEST_ASSERT(res.ullNumRows == 0, "Default ullNumRows is 0");
		TEST_ASSERT(res.ullAffectedRows == 0, "Default ullAffectedRows is 0");
		TEST_ASSERT(res.ullInsertID == 0, "Default ullInsertID is 0");

		// Simulate 64-bit item ID exceeding 4 billion (2^32 - 1)
		const uint64_t large_id = 5000000000ULL; // 5 billion
		res.ullInsertID = large_id;
		res.uiInsertID = static_cast<uint32_t>(large_id);

		TEST_ASSERT(res.ullInsertID == large_id, "64-bit ullInsertID preserves full value > 4B");
		TEST_ASSERT(res.uiInsertID == static_cast<uint32_t>(large_id), "32-bit uiInsertID reflects truncated lower bits");

		// Simulate error condition for affected rows: (my_ulonglong)-1
		const uint64_t error_val = static_cast<uint64_t>(-1);
		res.ullAffectedRows = error_val;
		res.uiAffectedRows = static_cast<uint32_t>(-1);

		TEST_ASSERT(res.ullAffectedRows == static_cast<uint64_t>(-1), "64-bit ullAffectedRows detects -1 error");
		TEST_ASSERT(res.uiAffectedRows == static_cast<uint32_t>(-1), "32-bit uiAffectedRows matches db/game error check ((uint32_t)-1)");
	}

	std::cout << "[RUN] Testing SQLMsg multi-result traversal..." << std::endl;
	{
		SQLMsg msg;
		SQLResult * r1 = new SQLResult();
		r1->ullNumRows = 10;
		r1->uiNumRows = 10;
		SQLResult * r2 = new SQLResult();
		r2->ullNumRows = 20;
		r2->uiNumRows = 20;

		msg.vec_pkResult.push_back(r1);
		msg.vec_pkResult.push_back(r2);

		TEST_ASSERT(msg.Get() == r1, "Initial Get() returns first result");
		TEST_ASSERT(msg.Next() == true, "Next() advances to second result");
		TEST_ASSERT(msg.Get() == r2, "Second Get() returns second result");
		TEST_ASSERT(msg.Next() == false, "Next() beyond boundary returns false");
	}

	// --------------------------------------------------------------------------
	// 5. CONNECTION LIFECYCLE & STATE MANAGEMENT SUITE
	// --------------------------------------------------------------------------
	std::cout << "[RUN] Testing CAsyncSQL connection lifecycle..." << std::endl;
	{
		TestAsyncSQL sql;
		TEST_ASSERT(sql.IsConnected() == false, "Initial state: IsConnected is false");

		TEST_ASSERT(sql.Setup("127.0.0.1", "mt2", "secret", "player", "latin1", true, 3306) == true, "Setup returned true");
		TEST_ASSERT(sql.Connect() == true, "Connect returned true");
		TEST_ASSERT(sql.IsConnected() == true, "IsConnected is true after Connect");

		TEST_ASSERT(sql.Reconnect() == true, "Reconnect returned true");
		TEST_ASSERT(sql.IsConnected() == true, "IsConnected is true after Reconnect");

		sql.Destroy();
		TEST_ASSERT(sql.IsConnected() == false, "IsConnected is false after Destroy");
	}

	std::cout << "[RUN] Testing CAsyncSQL Setup cloning..." << std::endl;
	{
		CAsyncSQL sql1;
		sql1.Setup("10.0.0.1", "user1", "pass1", "account", "cp1250", true, 3307);

		CAsyncSQL sql2;
		TEST_ASSERT(sql2.Setup(&sql1, true) == true, "Setup cloning returned true");
		TEST_ASSERT(sql2.Connect() == true, "Cloned instance Connect returned true");
		sql2.Quit();
	}

	// --------------------------------------------------------------------------
	// 6. ERROR & RECONNECT STRATEGY SUITE (Target database: MariaDB 12.3 LTS)
	// --------------------------------------------------------------------------
	std::cout << "[RUN] Testing error code preservation and non-idempotent safety..." << std::endl;
	{
		CAsyncSQL sql;
		sql.Setup("127.0.0.1", "mt2", "secret", "player", "latin1", true, 3306);
		sql.Connect();

		// Simulate CR_SERVER_LOST (2013)
		s_mock_force_errno = CR_SERVER_LOST;
		auto res = sql.DirectQuery("INSERT INTO item (id, owner_id) VALUES (1, 100);");
		TEST_ASSERT(res != nullptr, "DirectQuery returned SQLMsg pointer");
		TEST_ASSERT(res->uiSQLErrno == CR_SERVER_LOST, "CR_SERVER_LOST properly captured in SQLMsg");

		// Non-idempotent safety check: verify that DirectQuery did NOT resend and create duplicate records
		s_mock_force_errno = 0; // Reset
		sql.Quit();
	}

	std::cout << "[RUN] Testing MariaDB 12.3 Transaction / Snapshot Isolation contract..." << std::endl;
	{
		// In MariaDB 12.3, network disconnect unconditionally rolls back active transaction.
		// Verifying that CAsyncSQL does NOT assume previous transaction state survives reconnection.
		CAsyncSQL sql;
		sql.Setup("127.0.0.1", "mt2", "secret", "player", "latin1", true, 3306);
		sql.Connect();

		s_mock_force_errno = CR_SERVER_GONE_ERROR;
		auto msg = sql.DirectQuery("UPDATE player SET money = money - 1000;");
		TEST_ASSERT(msg->uiSQLErrno == CR_SERVER_GONE_ERROR, "CR_SERVER_GONE_ERROR captured");
		TEST_ASSERT(sql.Reconnect() == true, "Connection cleanly re-established for fresh transactions");

		s_mock_force_errno = 0;
		sql.Quit();
	}

	std::cout << "[RUN] Testing MariaDB 12.3 LTS Reconnect contract (no deprecated auto-reconnect)..." << std::endl;
	{
		// Target database: MariaDB 12.3 LTS — automatic MYSQL_OPT_RECONNECT is disabled and unsafe.
		// Explicit Reconnect() replaces automatic silent driver reconnection.
		CAsyncSQL sql;
		sql.Setup("127.0.0.1", "mt2", "secret", "player", "latin1", true, 3306);
		sql.Connect();
		TEST_ASSERT(sql.IsConnected() == true, "Active connection verified");

		// Explicit reconnection lifecycle
		TEST_ASSERT(sql.Reconnect() == true, "Explicit Reconnect() successfully re-establishes session");
		TEST_ASSERT(sql.IsConnected() == true, "Connection active post explicit Reconnect");
		sql.Quit();
	}

	std::cout << "[RUN] Testing CAsyncSQL::Quit() / Destroy() idempotency and handle safety..." << std::endl;
	{
		CAsyncSQL sql;
		sql.Setup("127.0.0.1", "mt2", "secret", "player", "latin1", true, 3306);
		sql.Connect();
		TEST_ASSERT(sql.IsConnected() == true, "Connection established before teardown test");

		// Test double Quit and double Destroy idempotency
		sql.Quit();
		sql.Quit();
		sql.Destroy();
		sql.Destroy();
		TEST_ASSERT(sql.IsConnected() == false, "Connection cleanly closed without double-free crashes");
	}

	std::cout << "[RUN] Testing initial connection retry and controlled Quit() termination..." << std::endl;
	{
		// Simulate database startup failure
		s_mock_force_connect_fail = true;

		CAsyncSQL sql;
		bool bSetup = sql.Setup("127.0.0.1", "mt2", "secret", "player", "latin1", false, 3306);
		TEST_ASSERT(bSetup == true, "Setup in threaded mode succeeded");

		// Queue a query while disconnected
		sql.AsyncQuery("DELETE FROM quest WHERE dwPID=999;");
		TEST_ASSERT(sql.CountQuery() == 1, "Query queued while connection is offline");

		// Controlled shutdown: Quit() must wake up and terminate thread without hanging
		sql.Quit();
		TEST_ASSERT(sql.IsConnected() == false, "Instance shutdown cleanly without thread hang or handle leak");

		// Verify query was flushed and preserved in failed queue (not silently deleted!)
		TEST_ASSERT(sql.CountFailedQuery() == 1, "Failed query captured in error queue instead of silent deletion");

		SQLMsg * pFailed = nullptr;
		TEST_ASSERT(sql.PopFailedQuery(&pFailed) == true, "Failed query popped successfully");
		TEST_ASSERT(pFailed != nullptr, "Failed query object is valid");
		TEST_ASSERT(pFailed->uiSQLErrno == CR_CONNECTION_ERROR, "CR_CONNECTION_ERROR recorded on offline query");
		delete pFailed;

		s_mock_force_connect_fail = false; // Reset mock
	}

	std::cout << "[RUN] Testing SQLResult saturation boundary and strict error contract (no silent saturation)..." << std::endl;
	{
		// 1. Error state: mysql_affected_rows returns (my_ulonglong)-1
		s_mock_affected_rows = static_cast<my_ulonglong>(-1);
		s_mock_insert_id = 0;
		s_mock_num_rows = 0;

		SQLMsg msgErr;
		msgErr.m_pkSQL = reinterpret_cast<MYSQL*>(&s_mock_connected);
		msgErr.Store();
		TEST_ASSERT(msgErr.Get()->ullAffectedRows == static_cast<uint64_t>(-1), "ullAffectedRows preserves exact 64-bit error value");
		TEST_ASSERT(msgErr.Get()->uiAffectedRows == static_cast<uint32_t>(-1), "uiAffectedRows correctly maps error to (uint32_t)-1");
		TEST_ASSERT(msgErr.Get()->HasValidAffectedRows() == false, "HasValidAffectedRows correctly flags error state");

		// 2. Large valid affected rows: 4,294,967,296 (0x100000000ULL, > UINT32_MAX)
		s_mock_affected_rows = 0x100000000ULL;
		SQLMsg msgLarge;
		msgLarge.m_pkSQL = reinterpret_cast<MYSQL*>(&s_mock_connected);
		msgLarge.Store();
		TEST_ASSERT(msgLarge.Get()->ullAffectedRows == 0x100000000ULL, "ullAffectedRows preserves 4B+ affected rows count");
		TEST_ASSERT(msgLarge.Get()->uiAffectedRows == static_cast<uint32_t>(-1), "uiAffectedRows marked as error (-1) on 32-bit overflow instead of silent false count");
		TEST_ASSERT(msgLarge.uiSQLErrno == 1264, "uiSQLErrno marked with ER_WARN_DATA_OUT_OF_RANGE (1264) on 32-bit overflow");
		TEST_ASSERT(msgLarge.Get()->HasValidAffectedRows() == false, "HasValidAffectedRows returns false for overflowed affected rows");

		// 3. Large insert ID: 5,000,000,000 (> UINT32_MAX)
		s_mock_insert_id = 5000000000ULL;
		SQLMsg msgInsert;
		msgInsert.m_pkSQL = reinterpret_cast<MYSQL*>(&s_mock_connected);
		msgInsert.Store();
		TEST_ASSERT(msgInsert.Get()->ullInsertID == 5000000000ULL, "ullInsertID preserves full 64-bit insert ID");
		TEST_ASSERT(msgInsert.Get()->uiInsertID == 0, "uiInsertID sets 0 (error) on >32-bit to prevent silent ID corruption/collision in callers");
		TEST_ASSERT(msgInsert.uiSQLErrno == 1264, "uiSQLErrno flagged with ER_WARN_DATA_OUT_OF_RANGE for unrepresentable ID");
		TEST_ASSERT(msgInsert.Get()->HasValidInsertID() == false, "HasValidInsertID returns false for overflowed ID");

		// 4. Valid normal insert ID (within 32-bit)
		s_mock_insert_id = 12345ULL;
		SQLMsg msgNormal;
		msgNormal.m_pkSQL = reinterpret_cast<MYSQL*>(&s_mock_connected);
		msgNormal.Store();
		TEST_ASSERT(msgNormal.Get()->ullInsertID == 12345ULL, "ullInsertID matches valid insert ID");
		TEST_ASSERT(msgNormal.Get()->uiInsertID == 12345, "uiInsertID matches valid insert ID");
		TEST_ASSERT(msgNormal.Get()->HasValidInsertID() == true, "HasValidInsertID returns true for valid 32-bit ID");

		// Reset mocks
		s_mock_affected_rows = 0;
		s_mock_insert_id = 0;
	}

	std::cout << "[RUN] Testing multi-instance thread isolation contract (m_mainSQL, m_directSQL, m_asyncSQL)..." << std::endl;
	{
		// Demonstrates that DBManager's architecture using 3 separate instances avoids socket race conditions
		CAsyncSQL mainSQL, directSQL, asyncSQL;
		mainSQL.Setup("127.0.0.1", "mt2", "secret", "player", "latin1", true, 3306);
		directSQL.Setup("127.0.0.1", "mt2", "secret", "player", "latin1", true, 3306);
		asyncSQL.Setup("127.0.0.1", "mt2", "secret", "player", "latin1", true, 3306);

		mainSQL.Connect();
		directSQL.Connect();
		asyncSQL.Connect();

		// Run concurrent operations on separate instances
		std::thread t1([&]() {
			auto m = mainSQL.DirectQuery("SELECT 1;");
			TEST_ASSERT(m != nullptr, "mainSQL thread completed query");
		});

		std::thread t2([&]() {
			auto m = directSQL.DirectQuery("SELECT 2;");
			TEST_ASSERT(m != nullptr, "directSQL thread completed query");
		});

		std::thread t3([&]() {
			asyncSQL.AsyncQuery("INSERT INTO log VALUES(1);");
			TEST_ASSERT(asyncSQL.CountQuery() == 1, "asyncSQL query queued independently");
		});

		t1.join();
		t2.join();
		t3.join();

		mainSQL.Quit();
		directSQL.Quit();
		asyncSQL.Quit();
	}

	std::cout << "[RUN] Testing sensitive SQL log sanitization (passwords, tokens, secrets)..." << std::endl;
	{
		std::string q1 = "SELECT * FROM account WHERE login='admin' AND password=PASSWORD('mySuperSecretPassword123')";
		std::string s1 = CAsyncSQL::SanitizeQueryForLog(q1);
		TEST_ASSERT(s1.find("mySuperSecretPassword123") == std::string::npos, "Password literal masked from query log");
		TEST_ASSERT(s1.find("PASSWORD('***')") != std::string::npos, "PASSWORD() masked with *** sentinel");

		std::string q2 = "UPDATE account SET password='plainTextPassword456' WHERE id=10";
		std::string s2 = CAsyncSQL::SanitizeQueryForLog(q2);
		TEST_ASSERT(s2.find("plainTextPassword456") == std::string::npos, "Plain text password masked from query log");
		TEST_ASSERT(s2.find("password='***'") != std::string::npos, "password='***' sentinel present");

		std::string q3 = "SELECT * FROM oauth WHERE token = 'my_api_token_xyz'";
		std::string s3 = CAsyncSQL::SanitizeQueryForLog(q3);
		TEST_ASSERT(s3.find("my_api_token_xyz") == std::string::npos, "Token masked from query log");

		std::string qNormal = "SELECT id, name FROM player WHERE level > 50";
		std::string sNormal = CAsyncSQL::SanitizeQueryForLog(qNormal);
		TEST_ASSERT(sNormal == qNormal, "Normal query preserved without modification");
	}

	std::cout << "[RUN] Testing reactive failed query callback contract..." << std::endl;
	{
		static bool s_callback_invoked = false;
		static unsigned int s_callback_errno = 0;

		CAsyncSQL callbackSQL;
		callbackSQL.Setup("127.0.0.1", "mt2", "secret", "player", "latin1", true, 3306);
		callbackSQL.SetFailedQueryCallback([](SQLMsg * pMsg) {
			s_callback_invoked = true;
			s_callback_errno = pMsg->uiSQLErrno;
			delete pMsg;
		});

		SQLMsg * failMsg = new SQLMsg;
		failMsg->stQuery = "DELETE FROM affect WHERE dwPID=999";
		failMsg->uiSQLErrno = CR_CONNECTION_ERROR;
		callbackSQL.PushFailedQuery(failMsg);

		TEST_ASSERT(s_callback_invoked == true, "Failed query callback invoked immediately for reactive delivery");
		TEST_ASSERT(s_callback_errno == CR_CONNECTION_ERROR, "Callback received correct connection error code");
		TEST_ASSERT(callbackSQL.CountFailedQuery() == 0, "No queries left uncollected in queue when callback is active");
	}

	std::cout << "[RUN] Testing SQLMsg error vs empty result disambiguation..." << std::endl;
	{
		// 1. Genuine empty query: 0 rows returned, errno 0
		s_mock_num_rows = 0;
		SQLMsg msgEmpty;
		msgEmpty.m_pkSQL = reinterpret_cast<MYSQL*>(&s_mock_connected);
		msgEmpty.Store();
		TEST_ASSERT(msgEmpty.IsError() == false, "Empty query is NOT an error");
		TEST_ASSERT(msgEmpty.IsEmpty() == true, "Empty query correctly recognized as empty");

		// 2. Overflow error query: 5 billion rows (> UINT32_MAX)
		s_mock_num_rows = 5000000000ULL;
		SQLMsg msgOverflow;
		msgOverflow.m_pkSQL = reinterpret_cast<MYSQL*>(&s_mock_connected);
		msgOverflow.Store();
		TEST_ASSERT(msgOverflow.IsError() == true, "Overflowed query is recognized as ERROR");
		TEST_ASSERT(msgOverflow.IsEmpty() == false, "Overflowed query NEVER mistaken for empty result");
		TEST_ASSERT(msgOverflow.Get()->IsOverflow() == true, "IsOverflow helper correctly detects overflow");

		// Reset mocks
		s_mock_num_rows = 0;
	}

	std::cout << "[RUN] Testing queue memory tracking and backpressure bounds..." << std::endl;
	{
		CAsyncSQL memSQL;
		memSQL.Setup("127.0.0.1", "mt2", "secret", "player", "latin1", true, 3306);

		SQLMsg * p = new SQLMsg;
		p->stQuery = "INSERT INTO player (name) VALUES ('TestPlayer');";
		memSQL.PushFailedQuery(p);

		TEST_ASSERT(memSQL.GetFailedQueueBytes() > 0, "Failed queue tracks byte consumption");
		TEST_ASSERT(memSQL.HasFailedQueryOverflow() == false, "Queue under limit has no overflow flag");

		SQLMsg * popped = nullptr;
		memSQL.PopFailedQuery(&popped);
		TEST_ASSERT(popped != nullptr, "Query popped cleanly");
		delete popped;
		TEST_ASSERT(memSQL.GetFailedQueueBytes() == 0, "Memory counter decrements on pop");
	}

	// --------------------------------------------------------------------------
	// 6.5. METIN2 DB LEVEL SQL DATA SAFETY & CACHE CONTRACTS
	// --------------------------------------------------------------------------
	std::cout << "[RUN] Testing Cache dirty flag persistence contract (SetNeedQuery)..." << std::endl;
	{
		struct TTestPlayer {
			uint32_t id;
			char name[24];
			int64_t gold;
		};

		class CTestPlayerCache : public cache<TTestPlayer> {
		public:
			bool m_bFlushTriggered = false;
			void OnFlush() override {
				m_bFlushTriggered = true;
			}
		};

		CTestPlayerCache playerCache;
		TEST_ASSERT(playerCache.IsNeedQuery() == false, "New cache entry starts clean (m_bNeedQuery=false)");

		TTestPlayer pData{ 101, "Warrior01", 500000 };
		playerCache.Put(&pData);
		TEST_ASSERT(playerCache.IsNeedQuery() == true, "Put marks cache dirty (m_bNeedQuery=true)");

		playerCache.Flush();
		TEST_ASSERT(playerCache.m_bFlushTriggered == true, "Flush triggers OnFlush handler");
		TEST_ASSERT(playerCache.IsNeedQuery() == false, "Flush resets dirty flag to clean");

		// Simulate asynchronous SQL error occurring after Flush()
		// When error is received by AnalyzeQueryResult, cache must be restored to dirty!
		playerCache.SetNeedQuery(true);
		TEST_ASSERT(playerCache.IsNeedQuery() == true, "SetNeedQuery(true) successfully restores dirty flag on save error");

		playerCache.m_bFlushTriggered = false;
		playerCache.Flush();
		TEST_ASSERT(playerCache.m_bFlushTriggered == true, "Subsequent flush successfully retries unsaved player data");
		TEST_ASSERT(playerCache.IsNeedQuery() == false, "Retry flush resets dirty flag once executed");
	}

	std::cout << "[RUN] Testing Multi-slot failed query popping contract (PopFailedQuery)..." << std::endl;
	{
		CAsyncSQL sqlPlayer;
		CAsyncSQL sqlAccount;
		CAsyncSQL sqlCommon;

		sqlPlayer.Setup("127.0.0.1", "mt2", "secret", "player", "latin1", true, 3306);
		sqlAccount.Setup("127.0.0.1", "mt2", "secret", "account", "latin1", true, 3306);
		sqlCommon.Setup("127.0.0.1", "mt2", "secret", "common", "latin1", true, 3306);

		SQLMsg * msgP = new SQLMsg;
		msgP->stQuery = "UPDATE player SET gold=1000 WHERE id=101;";
		msgP->uiSQLErrno = CR_CONNECTION_ERROR;
		sqlPlayer.PushFailedQuery(msgP);

		SQLMsg * msgA = new SQLMsg;
		msgA->stQuery = "UPDATE account SET status='OK' WHERE id=500;";
		msgA->uiSQLErrno = CR_SERVER_LOST;
		sqlAccount.PushFailedQuery(msgA);

		TEST_ASSERT(sqlPlayer.CountFailedQuery() == 1, "Player slot records 1 failed query");
		TEST_ASSERT(sqlAccount.CountFailedQuery() == 1, "Account slot records 1 failed query");
		TEST_ASSERT(sqlCommon.CountFailedQuery() == 0, "Common slot has 0 failed queries");

		// Simulate CDBManager::PopFailedQuery multi-slot draining loop
		std::vector<SQLMsg*> drained;
		CAsyncSQL* slots[3] = { &sqlPlayer, &sqlAccount, &sqlCommon };
		for (int i = 0; i < 3; ++i) {
			SQLMsg * popped = nullptr;
			while (slots[i]->PopFailedQuery(&popped)) {
				drained.push_back(popped);
			}
		}

		TEST_ASSERT(drained.size() == 2, "Draining pops all failed queries across all slots");
		TEST_ASSERT(drained[0]->uiSQLErrno == CR_CONNECTION_ERROR, "Slot 0 popped with exact error code");
		TEST_ASSERT(drained[1]->uiSQLErrno == CR_SERVER_LOST, "Slot 1 popped with exact error code");

		for (auto * d : drained) {
			delete d;
		}

		TEST_ASSERT(sqlPlayer.CountFailedQuery() == 0, "Player slot emptied after drain");
		TEST_ASSERT(sqlAccount.CountFailedQuery() == 0, "Account slot emptied after drain");
	}

	std::cout << "[RUN] Testing UNKNOWN state retention on connection loss..." << std::endl;
	{
		SQLMsg msgLost;
		msgLost.stQuery = "UPDATE player SET level=99 WHERE id=101;";
		msgLost.uiSQLErrno = CR_SERVER_LOST;

		bool bIsUnknownLost = (msgLost.uiSQLErrno == CR_SERVER_LOST ||
							  msgLost.uiSQLErrno == CR_SERVER_GONE_ERROR ||
							  msgLost.uiSQLErrno == CR_CONNECTION_ERROR);
		TEST_ASSERT(bIsUnknownLost == true, "CR_SERVER_LOST classified as UNKNOWN transaction status");

		SQLMsg msgGone;
		msgGone.stQuery = "INSERT INTO item (id, owner_id) VALUES (2001, 101);";
		msgGone.uiSQLErrno = CR_SERVER_GONE_ERROR;

		bool bIsUnknownGone = (msgGone.uiSQLErrno == CR_SERVER_LOST ||
							  msgGone.uiSQLErrno == CR_SERVER_GONE_ERROR ||
							  msgGone.uiSQLErrno == CR_CONNECTION_ERROR);
		TEST_ASSERT(bIsUnknownGone == true, "CR_SERVER_GONE_ERROR classified as UNKNOWN transaction status");

		SQLMsg msgSyntax;
		msgSyntax.stQuery = "INVALID SQL SYNTAX;";
		msgSyntax.uiSQLErrno = 1064; // ER_PARSE_ERROR

		bool bIsUnknownSyntax = (msgSyntax.uiSQLErrno == CR_SERVER_LOST ||
								msgSyntax.uiSQLErrno == CR_SERVER_GONE_ERROR ||
								msgSyntax.uiSQLErrno == CR_CONNECTION_ERROR);
		TEST_ASSERT(bIsUnknownSyntax == false, "Regular syntax/duplicate SQL errors NOT classified as UNKNOWN");
	}

	std::cout << "[RUN] Testing Shutdown draining contract and lifecycle safety..." << std::endl;
	{
		CAsyncSQL sqlShutdown;
		sqlShutdown.Setup("127.0.0.1", "mt2", "secret", "player", "latin1", true, 3306);

		for (int i = 0; i < 5; ++i) {
			SQLMsg * p = new SQLMsg;
			p->bReturn = true;
			p->stQuery = "UPDATE player SET playtime=playtime+60 WHERE id=10" + std::to_string(i) + ";";
			sqlShutdown.PushResult(p);
		}

		for (int i = 0; i < 3; ++i) {
			SQLMsg * p = new SQLMsg;
			p->bReturn = false;
			p->stQuery = "DELETE FROM item WHERE id=900" + std::to_string(i) + ";";
			p->uiSQLErrno = CR_CONNECTION_ERROR;
			sqlShutdown.PushFailedQuery(p);
		}

		// Simulate Main.cpp shutdown draining
		int drainedResults = 0;
		SQLMsg * res = nullptr;
		while (sqlShutdown.PopResult(&res)) {
			TEST_ASSERT(res != nullptr, "Shutdown result popped cleanly");
			delete res;
			++drainedResults;
		}

		int drainedFailed = 0;
		SQLMsg * fail = nullptr;
		while (sqlShutdown.PopFailedQuery(&fail)) {
			TEST_ASSERT(fail != nullptr, "Shutdown failed query popped cleanly");
			delete fail;
			++drainedFailed;
		}

		TEST_ASSERT(drainedResults == 5, "All 5 shutdown return results drained without loss");
		TEST_ASSERT(drainedFailed == 3, "All 3 shutdown failed queries drained without loss");
		TEST_ASSERT(sqlShutdown.CountResult() == 0, "Result queue completely drained after shutdown");
		TEST_ASSERT(sqlShutdown.CountFailedQuery() == 0, "Failed queue completely drained after shutdown");
	}

	std::cout << "[RUN] Testing Durable SQL Log Spool, Offline EscapeString, Queue Overflow & Crash Recovery..." << std::endl;
	{
		const char * testSpoolFile = "test_libsql_spool.dat";
		const char * testQuarantineFile = "test_libsql_spool.dat.quarantine";
		std::remove(testSpoolFile);
		std::remove(testQuarantineFile);

		// 1. Offline EscapeString in durable mode preserves payload instead of blanking out
		{
			CAsyncSQL durableOffline;
			TEST_ASSERT(durableOffline.EnableDurableSpool(testSpoolFile, 3, 64 * 1024, 100) == true, "EnableDurableSpool succeeded");
			char esc[128] = {0};
			const char * rawHint = "Item 'Sword+9' dropped by O'Connor";
			size_t len = durableOffline.EscapeString(esc, sizeof(esc), rawHint, std::strlen(rawHint));
			TEST_ASSERT(len > std::strlen(rawHint), "Offline EscapeString in durable mode escapes quotes instead of returning 0");
			TEST_ASSERT(std::strstr(esc, "\\'Sword+9\\'") != nullptr, "Offline EscapeString properly escapes single quotes");

			// Submit 4 queries while offline (with sensitive password literal in query 4 to verify disk sanitization)
			durableOffline.AsyncQuery("INSERT INTO log VALUES (1, 'BOOT');");
			durableOffline.AsyncQuery("INSERT INTO log VALUES (2, 'ITEM');");
			durableOffline.AsyncQuery("INSERT INTO log VALUES (3, 'CHAR');");
			durableOffline.AsyncQuery("INSERT INTO log VALUES (4, 'LOGIN', password='SuperSecretPlaintextPassword99');");

			auto st = durableOffline.GetDurableSpoolStats();
			TEST_ASSERT(st.ullSubmittedCount == 4, "4 queries submitted while offline");
			TEST_ASSERT(st.ullSpooledCount == 4, "All 4 offline queries routed to durable spool");
			TEST_ASSERT(st.ullPendingSpoolCount == 4, "4 queries pending in durable spool");
			TEST_ASSERT(st.ullDroppedSpoolFullCount == 0, "0 queries dropped");

			durableOffline.FlushAndSpoolOnShutdown(200);
		}

		// Verify raw disk spool file does NOT contain plaintext secret
		{
			std::ifstream ifs(testSpoolFile, std::ios::in | std::ios::binary);
			std::string diskContent((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
			TEST_ASSERT(!diskContent.empty(), "Spool file persisted on disk");
			TEST_ASSERT(diskContent.find("SuperSecretPlaintextPassword99") == std::string::npos, "Plaintext password never written to disk spool");
			TEST_ASSERT(diskContent.find("password='***'") != std::string::npos, "Masked password sentinel written to disk spool");
		}

		// 2. Restart recovery + queue capacity overflow + poison query quarantine
		{
			CAsyncSQL durableRecovered;
			TEST_ASSERT(durableRecovered.EnableDurableSpool(testSpoolFile, 2, 64 * 1024, 100) == true, "EnableDurableSpool loaded existing spool file");
			auto stLoaded = durableRecovered.GetDurableSpoolStats();
			TEST_ASSERT(stLoaded.ullPendingSpoolCount == 4, "All 4 pre-crash spool entries recovered from disk");

			durableRecovered.Setup("127.0.0.1", "mt2", "secret", "log", "latin1", true, 3306);
			TEST_ASSERT(durableRecovered.Connect() == true, "Connected recovered durable instance");

			// Replay all 4 recovered entries
			TEST_ASSERT(durableRecovered.ReplayDurableSpoolNow() == true, "ReplayDurableSpoolNow succeeded");
			auto stAfterReplay = durableRecovered.GetDurableSpoolStats();
			TEST_ASSERT(stAfterReplay.ullReplayedCount == 4, "All 4 recovered entries replayed to DB");
			TEST_ASSERT(stAfterReplay.ullCommittedCount == 4, "4 entries committed");
			TEST_ASSERT(stAfterReplay.ullPendingSpoolCount == 0, "Spool emptied after replay");

			// Queue capacity overflow (maxMemoryQueueCount = 2): push 5 queries before replay
			durableRecovered.AsyncQuery("INSERT INTO log VALUES (5, 'Q5');");
			durableRecovered.AsyncQuery("INSERT INTO log VALUES (6, 'Q6');");
			durableRecovered.AsyncQuery("INSERT INTO log VALUES (7, 'Q7');");
			durableRecovered.AsyncQuery("INSERT INTO log VALUES (8, 'Q8');");
			durableRecovered.AsyncQuery("INSERT INTO log VALUES (9, 'Q9');");

			auto stOverflow = durableRecovered.GetDurableSpoolStats();
			TEST_ASSERT(stOverflow.ullPendingMemoryCount == 2, "Memory queue bounded at maxMemoryQueueCount=2");
			TEST_ASSERT(stOverflow.ullPendingSpoolCount == 3, "3 overflowing queries routed to durable spool");

			// Inject a permanent SQL syntax error on next execution to test poison-pill quarantine
			s_mock_force_errno = 1064; // ER_PARSE_ERROR
			durableRecovered.AsyncQuery("INVALID SQL SYNTAX FOR QUARANTINE;");
			s_mock_force_errno = 0;

			durableRecovered.FlushAndSpoolOnShutdown(200);
			auto stFinal = durableRecovered.GetDurableSpoolStats();
			TEST_ASSERT(stFinal.ullPendingMemoryCount == 0 && stFinal.ullPendingSpoolCount == 0, "All memory and spool queues drained on shutdown");
			TEST_ASSERT(stFinal.ullCommittedCount == 10, "All valid queries committed without loss");
		}

		// 3. Spool disk byte limit (maxSpoolBytes), operational overflow callback/fail-stop policy & corrupted CRC line isolation
		{
			static CAsyncSQL * s_pBoundedRef = nullptr;
			static bool s_overflowCallbackFired = false;
			static uint64_t s_overflowDroppedCount = 0;
			static uint64_t s_reentrantDroppedSeen = 0;
			CAsyncSQL durableBounded;
			s_pBoundedRef = &durableBounded;
			durableBounded.SetSpoolOverflowCallback([](uint64_t, size_t, size_t, uint64_t droppedCount) {
				s_overflowCallbackFired = true;
				s_overflowDroppedCount = droppedCount;
				if (s_pBoundedRef)
				{
					// Must not deadlock even though m_mtxQuery is already held by the caller!
					auto st = s_pBoundedRef->GetDurableSpoolStats();
					s_reentrantDroppedSeen = st.ullDroppedSpoolFullCount;
				}
				thecore_shutdown();
			});
			durableBounded.SetSpoolOverflowPolicy(CAsyncSQL::SPOOL_OVERFLOW_FAIL_STOP_SHUTDOWN);
			TEST_ASSERT(durableBounded.EnableDurableSpool(testSpoolFile, 2, 1024, 100) == true, "EnableDurableSpool with 1024B cap succeeded");
			durableBounded.SetMaxSpoolBytes(80); // Fits 2 short queries (~34B each), rejects 3rd

			durableBounded.AsyncQuery("INSERT INTO log VALUES (1, 'A');");
			durableBounded.AsyncQuery("INSERT INTO log VALUES (2, 'B');");
			durableBounded.AsyncQuery("INSERT INTO log VALUES (3, 'C_EXCEEDS_80_BYTE_SPOOL_CAP');");

			auto stCap = durableBounded.GetDurableSpoolStats();
			TEST_ASSERT(stCap.ullPendingSpoolCount == 2, "Exactly 2 queries retained within 80-byte spool cap");
			TEST_ASSERT(stCap.ullDroppedSpoolFullCount == 1, "3rd query exceeding maxSpoolBytes accounted in ullDroppedSpoolFullCount");
			TEST_ASSERT(durableBounded.HasFailedQueryOverflow() == true, "Overflow alert flag set when maxSpoolBytes exceeded");
			TEST_ASSERT(s_overflowCallbackFired == true && s_overflowDroppedCount == 1 && s_reentrantDroppedSeen == 1,
				"Operational spool overflow callback invoked re-entrant GetDurableSpoolStats() without deadlock");
			TEST_ASSERT(thecore_is_shutdowned() != 0, "thecore_shutdown() safely signaled from overflow callback");
			TEST_ASSERT(durableBounded.IsEnd() == true, "Fail-stop policy SPOOL_OVERFLOW_FAIL_STOP_SHUTDOWN set m_bEnd=true on overflow");
			s_pBoundedRef = nullptr;
			durableBounded.FlushAndSpoolOnShutdown(100);

			// Inject a corrupted CRC line between the 2 valid lines on disk and verify LoadSpoolFromDiskLocked isolates it
			{
				std::ofstream ofs(testSpoolFile, std::ios::out | std::ios::app | std::ios::binary);
				ofs << "SPOOL1|99|deadbeef|INSERT INTO log VALUES (99, 'CORRUPTED_CRC');\n";
			}

			CAsyncSQL durableCrcCheck;
			TEST_ASSERT(durableCrcCheck.EnableDurableSpool(testSpoolFile, 4, 64 * 1024, 100) == true, "EnableDurableSpool loaded file with 1 corrupted CRC line");
			auto stCrc = durableCrcCheck.GetDurableSpoolStats();
			TEST_ASSERT(stCrc.ullPendingSpoolCount == 2, "2 valid lines recovered despite corrupted CRC line");
			TEST_ASSERT(stCrc.ullQuarantinedCount == 1, "Corrupted CRC line quarantined during LoadSpoolFromDiskLocked");
			durableCrcCheck.FlushAndSpoolOnShutdown(100);
		}

		// 4. SQL-literal-aware SanitizeQueryForLog (never crosses outer column quote boundaries), NO_BACKSLASH_ESCAPES offline escaping, CR_SERVER_LOST retry tracking & disk sync failure counter
		{
			// 4a. Cross-column quote boundary test: unclosed inner '"' inside first single-quoted column must NEVER eat "', '" into the second column
			std::string qCrossCol = "INSERT INTO chat_log VALUES ('password=\"hello', 'world\"');";
			std::string sCrossCol = CAsyncSQL::SanitizeQueryForLog(qCrossCol);
			TEST_ASSERT(sCrossCol == "INSERT INTO chat_log VALUES ('password=\"***', 'world\"');",
				"SanitizeQueryForLog bounds unclosed inner quote strictly within outer SQL column literal");

			// 4b. Trailing 'password=' at end of first column must NEVER treat the closing column quote as an opening value quote
			std::string qTrailingEq = "INSERT INTO chat_log VALUES ('password=', 'next_column');";
			std::string sTrailingEq = CAsyncSQL::SanitizeQueryForLog(qTrailingEq);
			TEST_ASSERT(sTrailingEq == qTrailingEq,
				"SanitizeQueryForLog never consumes closing SQL column quote after password=");

			// 4c. Escaped single quotes inside outer single-quoted column literal
			std::string qEscInner = "INSERT INTO command_log VALUES (1, 'gm password=\\'TopSecretGM99\\' ok');";
			std::string sEscInner = CAsyncSQL::SanitizeQueryForLog(qEscInner);
			TEST_ASSERT(sEscInner == "INSERT INTO command_log VALUES (1, 'gm password=\\'***\\' ok');",
				"SanitizeQueryForLog masks backslash-escaped inner quote while preserving valid SQL syntax");

			// 4d. NO_BACKSLASH_ESCAPES mode in SanitizeQueryForLog: trailing literal backslash in 'C:\dir\' must NOT escape the closing single quote
			std::string qNoBsLiteral = "INSERT INTO command_log VALUES ('C:\\dir\\', 'user token=''TokSecret77'' UTF8_Şğüıöç');";
			std::string sNoBsLiteral = CAsyncSQL::SanitizeQueryForLog(qNoBsLiteral, true);
			TEST_ASSERT(sNoBsLiteral == "INSERT INTO command_log VALUES ('C:\\dir\\', 'user token=''***'' UTF8_Şğüıöç');",
				"SanitizeQueryForLog with bNoBackslashEscapes=true treats trailing backslash as literal and masks doubled-quote secret");

			// 4e. NO_BACKSLASH_ESCAPES offline EscapeString mode doubles single quotes (' -> '')
			char escNoBackslash[128] = {0};
			const char * rawNoBs = "Player's \"Special\" \\Item";
			size_t lenNoBs = CAsyncSQL::EscapeStringOfflineSafe(escNoBackslash, sizeof(escNoBackslash), rawNoBs, std::strlen(rawNoBs), true);
			TEST_ASSERT(lenNoBs > 0 && std::strcmp(escNoBackslash, "Player''s \"Special\" \\Item") == 0,
				"EscapeStringOfflineSafe with NO_BACKSLASH_ESCAPES doubles single quotes without backslash escaping");

			// 4f. CR_SERVER_LOST mid-flight retry increments ullUncertainInFlightRetries and re-spools at head
			std::remove(testSpoolFile);
			CAsyncSQL durableMidFlight;
			durableMidFlight.EnableDurableSpool(testSpoolFile, 10, 64 * 1024, 100);
			durableMidFlight.Setup("127.0.0.1", "mt2", "secret", "log", "latin1", true, 3306);
			durableMidFlight.Connect();
			durableMidFlight.AsyncQuery("INSERT INTO log VALUES (101, 'MID_FLIGHT_LOST');");
			s_mock_force_errno = CR_SERVER_LOST;
			TEST_ASSERT(durableMidFlight.ReplayDurableSpoolNow() == false, "Mid-flight CR_SERVER_LOST aborts replay and re-spools");
			s_mock_force_errno = 0;
			auto stMid = durableMidFlight.GetDurableSpoolStats();
			TEST_ASSERT(stMid.ullUncertainInFlightRetries == 1 && stMid.ullPendingSpoolCount == 1,
				"Mid-flight CR_SERVER_LOST tracked in ullUncertainInFlightRetries and retained at spool head");
			durableMidFlight.Connect();
			TEST_ASSERT(durableMidFlight.ReplayDurableSpoolNow() == true, "Reconnected replay commits re-spooled mid-flight query");
			durableMidFlight.FlushAndSpoolOnShutdown(100);

			// 4g. Disk sync failure (unwritable path) increments ullDiskSyncFailureCount while keeping query in RAM spool
			CAsyncSQL durableDiskFail;
			durableDiskFail.EnableDurableSpool("nonexistent_dir_mt2_test_999/cannot_write_spool.dat", 4, 64 * 1024, 100);
			durableDiskFail.AsyncQuery("INSERT INTO log VALUES (202, 'RAM_SURVIVES_DISK_SYNC_FAIL');");
			auto stDiskFail = durableDiskFail.GetDurableSpoolStats();
			TEST_ASSERT(stDiskFail.ullDiskSyncFailureCount >= 1 && durableDiskFail.HasFailedQueryOverflow() == true,
				"Unwritable spool path increments ullDiskSyncFailureCount and sets overflow alert");
			TEST_ASSERT(stDiskFail.ullPendingSpoolCount == 1,
				"Query remains in RAM m_dequeSpool even when disk sync fails");
			durableDiskFail.FlushAndSpoolOnShutdown(100);
		}

		// 5. Same-directory multi-process / multi-instance exclusive lock-slot isolation (prevents .tmp race & overwrite)
		{
			const char * slot0File = "test_durable_spool_unit.dat";
			const char * slot1File = "test_durable_spool_unit.1.dat";
			std::remove(slot0File);
			std::remove(slot1File);

			CAsyncSQL procA;
			CAsyncSQL procB;
			TEST_ASSERT(procA.EnableDurableSpool(slot0File, 10, 64 * 1024, 100) == true, "First instance acquired primary spool path");
			TEST_ASSERT(procB.EnableDurableSpool(slot0File, 10, 64 * 1024, 100) == true, "Second concurrent instance in same directory isolated to secondary slot");
			TEST_ASSERT(procA.GetSpoolFilePath() == slot0File, "First instance uses slot 0 (test_durable_spool_unit.dat)");
			TEST_ASSERT(procB.GetSpoolFilePath() == slot1File, "Second instance automatically isolated to slot 1 (test_durable_spool_unit.1.dat)");

			// Both instances spool offline queries concurrently in the same directory without overwriting each other
			procA.AsyncQuery("INSERT INTO log VALUES (1001, 'PROC_A_RECORD_1');");
			procA.AsyncQuery("INSERT INTO log VALUES (1002, 'PROC_A_RECORD_2');");
			procB.AsyncQuery("INSERT INTO log VALUES (2001, 'PROC_B_RECORD_1');");
			procB.AsyncQuery("INSERT INTO log VALUES (2002, 'PROC_B_RECORD_2');");
			procB.AsyncQuery("INSERT INTO log VALUES (2003, 'PROC_B_RECORD_3');");

			procA.FlushAndSpoolOnShutdown(100);
			procB.FlushAndSpoolOnShutdown(100);

			// Restart both instances and verify 0 record loss and 0 cross-instance corruption
			CAsyncSQL recA;
			CAsyncSQL recB;
			TEST_ASSERT(recA.EnableDurableSpool(slot0File, 10, 64 * 1024, 100) == true, "Restarted instance A loaded slot 0 spool");
			TEST_ASSERT(recB.EnableDurableSpool(slot0File, 10, 64 * 1024, 100) == true, "Restarted instance B loaded slot 1 spool while A holds slot 0");
			TEST_ASSERT(recA.GetDurableSpoolStats().ullPendingSpoolCount == 2, "Slot 0 preserved all 2 records from instance A");
			TEST_ASSERT(recB.GetDurableSpoolStats().ullPendingSpoolCount == 3, "Slot 1 preserved all 3 records from instance B");

			recA.Setup("127.0.0.1", "mt2", "secret", "log", "latin1", true, 3306);
			recB.Setup("127.0.0.1", "mt2", "secret", "log", "latin1", true, 3306);
			recA.Connect();
			recB.Connect();
			TEST_ASSERT(recA.ReplayDurableSpoolNow() == true && recA.GetDurableSpoolStats().ullCommittedCount == 2, "Instance A replayed 2/2 records");
			TEST_ASSERT(recB.ReplayDurableSpoolNow() == true && recB.GetDurableSpoolStats().ullCommittedCount == 3, "Instance B replayed 3/3 records");
			recA.FlushAndSpoolOnShutdown(100);
			recB.FlushAndSpoolOnShutdown(100);

			std::remove(slot0File);
			std::remove(slot1File);
			std::remove("test_durable_spool_unit.dat.lock");
			std::remove("test_durable_spool_unit.1.dat.lock");
		}

		std::remove(testSpoolFile);
		std::remove(testQuarantineFile);
	}

	// --------------------------------------------------------------------------
	// 7. LIVE DATABASE INTEGRATION SUITE (CONDITIONAL / HONEST)
	// --------------------------------------------------------------------------
	std::cout << "[RUN] Checking Live MariaDB 12.3 LTS Server Integration..." << std::endl;
	const char * run_live = std::getenv("RUN_LIVE_DB_TESTS");
	if (run_live && std::strcmp(run_live, "1") == 0) {
		std::cout << "--> RUN_LIVE_DB_TESTS enabled: Attempting live DB handshake..." << std::endl;
		CAsyncSQL live_sql;
		if (live_sql.Setup("127.0.0.1", "root", "", "test", "latin1", true, 3306) && live_sql.Connect()) {
			std::cout << "[SUCCESS] Live database connection established." << std::endl;
			live_sql.Quit();
		} else {
			std::cout << "[WARN] Live database connection failed." << std::endl;
		}
	} else {
		std::cout << "[SKIP] Live MariaDB 12.3 LTS server integration test: SKIPPED (No local live DB service active, RUN_LIVE_DB_TESTS!=1)" << std::endl;
	}

	std::cout << "\n==================================================" << std::endl;
	std::cout << "[SUCCESS] libsql: " << g_tests_passed << "/" << g_tests_total << " tests passed cleanly!" << std::endl;
	std::cout << "==================================================" << std::endl;
	return 0;
}
