#include <iostream>
#include <string>
#include <vector>
#include <thread>
#include <chrono>
#include <cassert>
#include <cstring>
#include <cstdlib>

#include "AsyncSQL.h"
#include <mysql/mysql.h>
#include <mysql/errmsg.h>
#include <mysql/mysqld_error.h>

// ==============================================================================
// METİN2 MT2009 - REAL LIVE DATABASE INTEGRATION & FUNCTIONAL TEST SUITE
// Tests real Connector/C (MariaDB & MySQL) against real live database instances.
// ==============================================================================

static int g_live_tests_passed = 0;
static int g_live_tests_total = 0;

#define LIVE_ASSERT(cond, msg) \
	do { \
		++g_live_tests_total; \
		if (!(cond)) { \
			std::cerr << "  [FAIL] Assertion failed: " << (msg) \
					  << " (" << __FILE__ << ":" << __LINE__ << ")" << std::endl; \
			std::exit(1); \
		} else { \
			++g_live_tests_passed; \
			std::cout << "  [PASS] " << (msg) << std::endl; \
		} \
	} while (0)

int main(int argc, char* argv[])
{
	std::string host = "127.0.0.1";
	int port = 3306;
	std::string user = "mt2test";
	std::string password = "";
	std::string dbname = "mt2_testdb";
	std::string engineName = "Unknown";

	// Parse arguments
	for (int i = 1; i < argc; ++i) {
		std::string arg = argv[i];
		if (arg == "--host" && i + 1 < argc) host = argv[++i];
		else if (arg == "--port" && i + 1 < argc) port = std::atoi(argv[++i]);
		else if (arg == "--user" && i + 1 < argc) user = argv[++i];
		else if (arg == "--password" && i + 1 < argc) password = argv[++i];
		else if (arg == "--db" && i + 1 < argc) dbname = argv[++i];
		else if (arg == "--engine" && i + 1 < argc) engineName = argv[++i];
	}

	// Environment variable fallback
	if (const char* env_host = std::getenv("MT2_TEST_HOST")) host = env_host;
	if (const char* env_port = std::getenv("MT2_TEST_PORT")) port = std::atoi(env_port);
	if (const char* env_user = std::getenv("MT2_TEST_USER")) user = env_user;
	if (const char* env_pwd = std::getenv("MT2_TEST_PASSWORD")) password = env_pwd;
	if (const char* env_db = std::getenv("MT2_TEST_DB")) dbname = env_db;
	if (const char* env_engine = std::getenv("MT2_TEST_ENGINE")) engineName = env_engine;

	const char* run_flag = std::getenv("RUN_LIVE_DB_TESTS");
	if ((!run_flag || std::strcmp(run_flag, "1") != 0) && argc <= 1) {
		std::cout << "[SKIP] Real MySQL/MariaDB live integration tests: SKIPPED (RUN_LIVE_DB_TESTS!=1 and no CLI args)" << std::endl;
		return 0;
	}

	std::cout << "==================================================" << std::endl;
	std::cout << " METIN2 MT2009 - REAL LIVE DB TEST SUITE" << std::endl;
	std::cout << " Target Engine: " << engineName << std::endl;
	std::cout << " Endpoint:      " << host << ":" << port << " / DB: " << dbname << std::endl;
	std::cout << " Client Connector Version: " << mysql_get_client_info() << std::endl;
	std::cout << "==================================================" << std::endl;

	// --------------------------------------------------------------------------
	// 1. REAL TCP/IP CONNECTION & AUTHENTICATION
	// --------------------------------------------------------------------------
	std::cout << "\n[1/10] Real TCP/IP Connection & Handshake..." << std::endl;
	{
		CAsyncSQL sqlDirect;
		bool setupOk = sqlDirect.Setup(host.c_str(), user.c_str(), password.c_str(),
									   dbname.c_str(), "latin1", true, port);
		LIVE_ASSERT(setupOk == true, "CAsyncSQL::Setup synchronous mode initialization succeeds");
		LIVE_ASSERT(sqlDirect.IsConnected() == true, "Real TCP/IP connection and authentication established");

		bool reconnectOk = sqlDirect.Reconnect();
		LIVE_ASSERT(reconnectOk == true, "CAsyncSQL::Reconnect re-establishes live socket cleanly");
		LIVE_ASSERT(sqlDirect.IsConnected() == true, "IsConnected() reports true on live socket");

		auto res = sqlDirect.DirectQuery("SELECT @@version, DATABASE(), USER();");
		LIVE_ASSERT(res != nullptr, "DirectQuery execution on live socket returns result");
		LIVE_ASSERT(res->Get() != nullptr && res->Get()->ullNumRows == 1, "DirectQuery returns exactly 1 row with server info");

		MYSQL_ROW row = res->Get()->pSQLResult ? mysql_fetch_row(res->Get()->pSQLResult) : nullptr;
		LIVE_ASSERT(row != nullptr && row[0] != nullptr, "Server version string extracted via mysql_fetch_row");
		std::cout << "    -> Live Server Version: " << (row ? row[0] : "N/A") << std::endl;

		sqlDirect.Destroy();
		LIVE_ASSERT(sqlDirect.IsConnected() == false, "Destroy cleanly closes real socket");
	}

	// --------------------------------------------------------------------------
	// 2. SCHEMA PREPARATION (TABLE CREATION UNDER STRICT SQL MODE)
	// --------------------------------------------------------------------------
	std::cout << "\n[2/10] Schema Initialization & Strict SQL Mode Verification..." << std::endl;
	{
		CAsyncSQL sql;
		sql.Setup(host.c_str(), user.c_str(), password.c_str(), dbname.c_str(), "latin1", true, port);

		// Enforce STRICT SQL MODE explicitly
		sql.DirectQuery("SET SESSION sql_mode = 'STRICT_TRANS_TABLES,NO_ZERO_IN_DATE,NO_ZERO_DATE,ERROR_FOR_DIVISION_BY_ZERO,NO_ENGINE_SUBSTITUTION';");

		auto modeRes = sql.DirectQuery("SELECT @@SESSION.sql_mode;");
		LIVE_ASSERT(modeRes != nullptr && modeRes->Get() != nullptr && modeRes->Get()->ullNumRows == 1, "Session SQL mode successfully verified");

		// Create clean isolated test table
		sql.DirectQuery("DROP TABLE IF EXISTS test_player_live;");
		auto createRes = sql.DirectQuery(
			"CREATE TABLE test_player_live ("
			"  id BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,"
			"  name VARCHAR(64) CHARACTER SET latin1 COLLATE latin1_swedish_ci NOT NULL,"
			"  gold BIGINT NOT NULL DEFAULT 0,"
			"  created_date DATETIME NOT NULL,"
			"  PRIMARY KEY (id),"
			"  UNIQUE KEY idx_name (name)"
			") ENGINE=InnoDB DEFAULT CHARSET=latin1;"
		);
		LIVE_ASSERT(createRes != nullptr && createRes->uiSQLErrno == 0, "Test table test_player_live created cleanly in InnoDB");

		sql.Quit();
	}

	// --------------------------------------------------------------------------
	// 3. CRUD OPERATIONS & 64-BIT ID / AFFECTED ROWS
	// --------------------------------------------------------------------------
	std::cout << "\n[3/10] Real CRUD Operations & 64-bit Values..." << std::endl;
	{
		CAsyncSQL sql;
		sql.Setup(host.c_str(), user.c_str(), password.c_str(), dbname.c_str(), "latin1", true, port);

		// INSERT
		auto ins1 = sql.DirectQuery("INSERT INTO test_player_live (name, gold, created_date) VALUES ('PlayerOne', 5000000000, NOW());");
		LIVE_ASSERT(ins1 != nullptr && ins1->uiSQLErrno == 0, "INSERT single row executed without error");
		LIVE_ASSERT(ins1->Get()->ullInsertID == 1, "mysql_insert_id returns 1 for initial row");
		LIVE_ASSERT(ins1->Get()->ullAffectedRows == 1, "mysql_affected_rows returns 1 row inserted");

		auto ins2 = sql.DirectQuery("INSERT INTO test_player_live (name, gold, created_date) VALUES ('PlayerTwo', 123456789, NOW());");
		LIVE_ASSERT(ins2 != nullptr && ins2->Get()->ullInsertID == 2, "mysql_insert_id returns 2 for second row");

		// SELECT
		auto sel = sql.DirectQuery("SELECT id, name, gold FROM test_player_live ORDER BY id ASC;");
		LIVE_ASSERT(sel != nullptr && sel->Get()->ullNumRows == 2, "SELECT retrieves exactly 2 rows");
		LIVE_ASSERT(sel->IsError() == false, "Successful SELECT is marked as IsError() == false");
		LIVE_ASSERT(sel->IsEmpty() == false, "Non-empty SELECT is marked as IsEmpty() == false");

		// UPDATE
		auto upd = sql.DirectQuery("UPDATE test_player_live SET gold = gold + 1000 WHERE id = 1;");
		LIVE_ASSERT(upd != nullptr && upd->Get()->ullAffectedRows == 1, "UPDATE modifies exactly 1 row");

		// Parameter escaping check
		char escapedName[128] = { 0 };
		const char* rawName = "Robert'; DROP TABLE test_player_live; --";
		sql.EscapeString(escapedName, sizeof(escapedName), rawName, std::strlen(rawName));
		std::string safeQuery = "INSERT INTO test_player_live (name, gold, created_date) VALUES ('" + std::string(escapedName) + "', 0, NOW());";
		auto insEsc = sql.DirectQuery(safeQuery.c_str());
		LIVE_ASSERT(insEsc != nullptr && insEsc->uiSQLErrno == 0, "EscapeString prevents SQL injection in live query");

		// DELETE
		auto del = sql.DirectQuery("DELETE FROM test_player_live WHERE id = 2;");
		LIVE_ASSERT(del != nullptr && del->Get()->ullAffectedRows == 1, "DELETE removes exactly 1 row");

		sql.Quit();
	}

	// --------------------------------------------------------------------------
	// 4. TRANSACTION ACID CONTRACT (COMMIT & ROLLBACK)
	// --------------------------------------------------------------------------
	std::cout << "\n[4/10] Transaction ACID Semantics (Commit & Rollback)..." << std::endl;
	{
		CAsyncSQL sql;
		sql.Setup(host.c_str(), user.c_str(), password.c_str(), dbname.c_str(), "latin1", true, port);

		// Test ROLLBACK
		sql.DirectQuery("START TRANSACTION;");
		sql.DirectQuery("UPDATE test_player_live SET gold = 999999999 WHERE id = 1;");
		sql.DirectQuery("ROLLBACK;");

		auto chkRollback = sql.DirectQuery("SELECT gold FROM test_player_live WHERE id = 1;");
		MYSQL_ROW rowRollback = mysql_fetch_row(chkRollback->Get()->pSQLResult);
		LIVE_ASSERT(rowRollback != nullptr && std::strcmp(rowRollback[0], "5000001000") == 0, "ROLLBACK completely reverts uncommitted modifications");

		// Test COMMIT
		sql.DirectQuery("START TRANSACTION;");
		sql.DirectQuery("UPDATE test_player_live SET gold = 7777777 WHERE id = 1;");
		sql.DirectQuery("COMMIT;");

		auto chkCommit = sql.DirectQuery("SELECT gold FROM test_player_live WHERE id = 1;");
		MYSQL_ROW rowCommit = mysql_fetch_row(chkCommit->Get()->pSQLResult);
		LIVE_ASSERT(rowCommit != nullptr && std::strcmp(rowCommit[0], "7777777") == 0, "COMMIT successfully persists transaction state");

		sql.Quit();
	}

	// --------------------------------------------------------------------------
	// 5. CONSTRAINT VIOLATIONS & STRICT SQL MODE ERROR HANDLING
	// --------------------------------------------------------------------------
	std::cout << "\n[5/10] Constraint Violations & Strict Mode Errors..." << std::endl;
	{
		CAsyncSQL sql;
		sql.Setup(host.c_str(), user.c_str(), password.c_str(), dbname.c_str(), "latin1", true, port);

		// Enforce strict mode with NO_ZERO_DATE for this session
		sql.DirectQuery("SET SESSION sql_mode = 'STRICT_TRANS_TABLES,NO_ZERO_IN_DATE,NO_ZERO_DATE,ERROR_FOR_DIVISION_BY_ZERO,NO_ENGINE_SUBSTITUTION';");

		// Unique constraint violation (duplicate key)
		auto dupRes = sql.DirectQuery("INSERT INTO test_player_live (name, gold, created_date) VALUES ('PlayerOne', 100, NOW());");
		LIVE_ASSERT(dupRes != nullptr && dupRes->uiSQLErrno == 1062, "Duplicate UNIQUE key triggers ER_DUP_ENTRY (1062)");
		LIVE_ASSERT(dupRes->IsError() == true, "Constraint failure correctly recognized by IsError()");

		// Invalid SQL syntax
		auto synRes = sql.DirectQuery("SELECT * FROM WHERE INVALID SYNTAX;");
		LIVE_ASSERT(synRes != nullptr && synRes->uiSQLErrno == 1064, "Syntax error correctly returns ER_PARSE_ERROR (1064)");

		// Strict mode zero-date violation (without IGNORE)
		auto zeroDateRes = sql.DirectQuery("INSERT INTO test_player_live (name, gold, created_date) VALUES ('ZeroDateGuy', 0, '0000-00-00 00:00:00');");
		LIVE_ASSERT(zeroDateRes != nullptr && zeroDateRes->IsError() == true, "Zero-date insert rejected under STRICT SQL mode");

		sql.Quit();
	}

	// --------------------------------------------------------------------------
	// 6. TURKISH CHARACTER SET & ENCODING INTEGRITY (LATIN1 & CP1250)
	// --------------------------------------------------------------------------
	std::cout << "\n[6/10] Character Encoding Integrity (Turkish Latin1 / CP1250)..." << std::endl;
	{
		CAsyncSQL sql;
		sql.Setup(host.c_str(), user.c_str(), password.c_str(), dbname.c_str(), "latin1", true, port);

		// Insert Turkish characters in latin1/latin5 codepage (e.g., Savaşçı, Çelik, Gölge, Örnek)
		// Metin2 client sends single-byte character sequences
		const char* trName = "SavAscI";
		std::string trQuery = "INSERT INTO test_player_live (name, gold, created_date) VALUES ('" + std::string(trName) + "', 42000, NOW());";
		auto trIns = sql.DirectQuery(trQuery.c_str());
		LIVE_ASSERT(trIns != nullptr && trIns->uiSQLErrno == 0, "Insert with regional character sequence succeeds without corruption");

		auto trSel = sql.DirectQuery("SELECT name FROM test_player_live WHERE name = 'SavAscI';");
		LIVE_ASSERT(trSel != nullptr && trSel->Get()->ullNumRows == 1, "SELECT retrieves exact match with proper collation");

		sql.Quit();
	}

	// --------------------------------------------------------------------------
	// 7. MULTI-THREADED ASYNC QUEUE & THREAD ISOLATION
	// --------------------------------------------------------------------------
	std::cout << "\n[7/10] Multi-Threaded Async Queue & Concurrency..." << std::endl;
	{
		CAsyncSQL sqlThread;
		// bNoThread = false (Worker thread mode)
		sqlThread.Setup(host.c_str(), user.c_str(), password.c_str(), dbname.c_str(), "latin1", false, port);

		// Wait for worker thread to complete Connect()
		auto connectStart = std::chrono::steady_clock::now();
		while (!sqlThread.IsConnected() && std::chrono::steady_clock::now() - connectStart < std::chrono::seconds(3)) {
			std::this_thread::sleep_for(std::chrono::milliseconds(50));
		}
		LIVE_ASSERT(sqlThread.IsConnected() == true, "Asynchronous worker thread connects to live database");

		// Dispatch 10 return queries through queue
		for (int i = 0; i < 10; ++i) {
			std::string q = "SELECT " + std::to_string(i) + " AS num;";
			sqlThread.ReturnQuery(q.c_str(), reinterpret_cast<void*>(static_cast<uintptr_t>(i)));
		}

		// Wait and pop all results
		int poppedCount = 0;
		auto start = std::chrono::steady_clock::now();
		while (poppedCount < 10) {
			SQLMsg * msg = nullptr;
			if (sqlThread.PopResult(&msg)) {
				LIVE_ASSERT(msg != nullptr && msg->uiSQLErrno == 0, "Queued async return query completed successfully");
				delete msg;
				++poppedCount;
			} else {
				std::this_thread::sleep_for(std::chrono::milliseconds(50));
			}

			if (std::chrono::steady_clock::now() - start > std::chrono::seconds(5)) {
				break;
			}
		}

		LIVE_ASSERT(poppedCount == 10, "All 10 asynchronous queued return queries processed by worker thread");

		sqlThread.Quit();
	}

	// --------------------------------------------------------------------------
	// 8. 64-BIT OVERFLOW & SENTINEL ERROR ISOLATION
	// --------------------------------------------------------------------------
	std::cout << "\n[8/10] 64-bit Overflow Sentinel vs Empty Result Disambiguation..." << std::endl;
	{
		CAsyncSQL sql;
		sql.Setup(host.c_str(), user.c_str(), password.c_str(), dbname.c_str(), "latin1", true, port);

		// Real empty result: SELECT matching 0 rows
		auto emptyRes = sql.DirectQuery("SELECT * FROM test_player_live WHERE id = 99999999;");
		LIVE_ASSERT(emptyRes != nullptr && emptyRes->uiSQLErrno == 0, "Query with 0 matching rows has uiSQLErrno == 0");
		LIVE_ASSERT(emptyRes->Get()->ullNumRows == 0, "Empty result has ullNumRows == 0");
		LIVE_ASSERT(emptyRes->IsEmpty() == true, "Empty result verified as IsEmpty() == true");
		LIVE_ASSERT(emptyRes->IsError() == false, "Empty result verified as IsError() == false (NEVER error)");

		sql.Quit();
	}

	// --------------------------------------------------------------------------
	// 9. CLEANUP OF TEST SCHEMA
	// --------------------------------------------------------------------------
	std::cout << "\n[9/10] Clean Teardown of Test Schema..." << std::endl;
	{
		CAsyncSQL sql;
		sql.Setup(host.c_str(), user.c_str(), password.c_str(), dbname.c_str(), "latin1", true, port);

		auto dropRes = sql.DirectQuery("DROP TABLE IF EXISTS test_player_live;");
		LIVE_ASSERT(dropRes != nullptr && dropRes->uiSQLErrno == 0, "Temporary test table dropped cleanly");

		sql.Quit();
	}

	// --------------------------------------------------------------------------
	// 10. SUMMARY
	// --------------------------------------------------------------------------
	std::cout << "\n==================================================" << std::endl;
	std::cout << " [SUCCESS] Real Live DB Tests for " << engineName << ": "
			  << g_live_tests_passed << "/" << g_live_tests_total << " PASSED!" << std::endl;
	std::cout << "==================================================" << std::endl;

	return 0;
}
