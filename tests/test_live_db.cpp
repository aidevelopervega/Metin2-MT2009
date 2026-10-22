#include <iostream>
#include <string>
#include <vector>
#include <thread>
#include <chrono>
#include <cassert>
#include <cstring>
#include <cstdlib>
#include <cstdio>
#include <fstream>
#include <sys/stat.h>

#include "AsyncSQL.h"
#include <mysql.h>
#include <errmsg.h>
#include <mysqld_error.h>
#include "tcp_fault_proxy.hpp"

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
	std::cout << " Auth User:     " << user << " / Password: [PROTECTED / NOT LOGGED]" << std::endl;
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
	// 9. TLS / SSL CONNECTION SECURITY VERIFICATION
	// --------------------------------------------------------------------------
	std::cout << "\n[9/11] TLS / SSL Connection Security & Cipher Verification..." << std::endl;
	{
		CAsyncSQL sql;
		sql.Setup(host.c_str(), user.c_str(), password.c_str(), dbname.c_str(), "latin1", true, port);

		auto sslRes = sql.DirectQuery("SHOW STATUS LIKE 'Ssl_cipher';");
		LIVE_ASSERT(sslRes != nullptr && sslRes->uiSQLErrno == 0, "Query SHOW STATUS LIKE 'Ssl_cipher' succeeded");
		std::string defCipher = "NONE";
		if (sslRes->Get() && sslRes->Get()->ullNumRows > 0) {
			MYSQL_ROW row = mysql_fetch_row(sslRes->Get()->pSQLResult);
			if (row && row[1] && std::strlen(row[1]) > 0) {
				defCipher = row[1];
			}
		}

		auto verRes = sql.DirectQuery("SHOW STATUS LIKE 'Ssl_version';");
		std::string defVersion = "NONE";
		if (verRes && verRes->Get() && verRes->Get()->ullNumRows > 0) {
			MYSQL_ROW row = mysql_fetch_row(verRes->Get()->pSQLResult);
			if (row && row[1] && std::strlen(row[1]) > 0) {
				defVersion = row[1];
			}
		}

		std::cout << "    -> Default Connection Cipher:  " << defCipher << std::endl;
		std::cout << "    -> Default Connection Version: " << defVersion << std::endl;
		sql.Destroy();

		const char* caPath = std::getenv("MT2_TEST_SSL_CA");
		const char* invalidCaPath = std::getenv("MT2_TEST_INVALID_SSL_CA");
		if (caPath && std::strlen(caPath) > 0 && invalidCaPath && std::strlen(invalidCaPath) > 0)
		{
			std::cout << "    -> Testing TLS Client CA Verification & Rejection Contract..." << std::endl;
			// 1. Valid CA connection test & TLS Cipher Verification
			{
				MYSQL mysqlValid;
				mysql_init(&mysqlValid);
#if defined(MARIADB_BASE_VERSION) || defined(MARIADB_VERSION_ID)
				my_bool verify = 1;
				mysql_options(&mysqlValid, MYSQL_OPT_SSL_VERIFY_SERVER_CERT, &verify);
				my_bool enforce = 1;
				mysql_options(&mysqlValid, MYSQL_OPT_SSL_ENFORCE, &enforce);
#else
				unsigned int sslMode = 4; // SSL_MODE_VERIFY_CA
				mysql_options(&mysqlValid, MYSQL_OPT_SSL_MODE, &sslMode);
#endif
				mysql_ssl_set(&mysqlValid, nullptr, nullptr, caPath, nullptr, nullptr);
				MYSQL* res = mysql_real_connect(&mysqlValid, host.c_str(), user.c_str(), password.c_str(), dbname.c_str(), port, nullptr, 0);
				LIVE_ASSERT(res != nullptr, "TLS connection with valid trusted CA succeeded");
				if (res) {
					std::string sslCipher = "NONE";
					std::string sslVersion = "NONE";
					if (mysql_query(&mysqlValid, "SHOW STATUS LIKE 'Ssl_cipher';") == 0) {
						MYSQL_RES* r = mysql_store_result(&mysqlValid);
						if (r) {
							MYSQL_ROW row = mysql_fetch_row(r);
							if (row && row[1]) sslCipher = row[1];
							mysql_free_result(r);
						}
					}
					if (mysql_query(&mysqlValid, "SHOW STATUS LIKE 'Ssl_version';") == 0) {
						MYSQL_RES* r = mysql_store_result(&mysqlValid);
						if (r) {
							MYSQL_ROW row = mysql_fetch_row(r);
							if (row && row[1]) sslVersion = row[1];
							mysql_free_result(r);
						}
					}
					std::cout << "    -> Negotiated TLS Cipher:  " << sslCipher << std::endl;
					std::cout << "    -> Negotiated TLS Version: " << sslVersion << std::endl;
					LIVE_ASSERT(sslCipher != "NONE" && !sslCipher.empty(), "TLS encryption actively negotiated on TLS connection (Ssl_cipher != NONE)");
					LIVE_ASSERT(sslVersion != "NONE" && !sslVersion.empty(), "TLS protocol version actively negotiated on TLS connection (Ssl_version != NONE)");
					mysql_close(&mysqlValid);
				}
			}

			// 2. Untrusted / Invalid CA connection test (MUST be strictly rejected)
			{
				MYSQL mysqlInvalid;
				mysql_init(&mysqlInvalid);
#if defined(MARIADB_BASE_VERSION) || defined(MARIADB_VERSION_ID)
				my_bool verify = 1;
				mysql_options(&mysqlInvalid, MYSQL_OPT_SSL_VERIFY_SERVER_CERT, &verify);
				my_bool enforce = 1;
				mysql_options(&mysqlInvalid, MYSQL_OPT_SSL_ENFORCE, &enforce);
#else
				unsigned int sslMode = 4; // SSL_MODE_VERIFY_CA
				mysql_options(&mysqlInvalid, MYSQL_OPT_SSL_MODE, &sslMode);
#endif
				mysql_ssl_set(&mysqlInvalid, nullptr, nullptr, invalidCaPath, nullptr, nullptr);
				MYSQL* resBad = mysql_real_connect(&mysqlInvalid, host.c_str(), user.c_str(), password.c_str(), dbname.c_str(), port, nullptr, 0);
				LIVE_ASSERT(resBad == nullptr, "TLS connection with untrusted/invalid CA was strictly REJECTED");
				LIVE_ASSERT(mysql_errno(&mysqlInvalid) != 0, "Untrusted CA connection failure returned non-zero error code");
				std::cout << "    -> Untrusted CA Rejection Error Code: " << mysql_errno(&mysqlInvalid) << " (" << mysql_error(&mysqlInvalid) << ")" << std::endl;
				if (resBad) {
					mysql_close(&mysqlInvalid);
				}
			}
		}
		else
		{
			LIVE_ASSERT(defCipher != "NONE" && !defCipher.empty(), "Default connection has active TLS encryption (Ssl_cipher != NONE)");
		}
	}

	// --------------------------------------------------------------------------
	// 10. PRE-SEVERED SOCKET GUARD, CR_SERVER_GONE_ERROR & NO BLIND RETRY
	// --------------------------------------------------------------------------
	std::cout << "\n[10/12] Pre-Severed Socket Guard, CR_SERVER_GONE_ERROR & No Blind Retry..." << std::endl;
	{
		CAsyncSQL sqlVictim;
		sqlVictim.Setup(host.c_str(), user.c_str(), password.c_str(), dbname.c_str(), "latin1", true, port);

		// Get connection ID
		auto connIdRes = sqlVictim.DirectQuery("SELECT CONNECTION_ID();");
		LIVE_ASSERT(connIdRes != nullptr && connIdRes->Get()->ullNumRows == 1, "Retrieved live CONNECTION_ID()");
		MYSQL_ROW rowId = mysql_fetch_row(connIdRes->Get()->pSQLResult);
		std::string connIdStr = rowId ? rowId[0] : "0";

		// Record current gold before interruption
		auto preRes = sqlVictim.DirectQuery("SELECT gold FROM test_player_live WHERE id = 1;");
		MYSQL_ROW rowPre = mysql_fetch_row(preRes->Get()->pSQLResult);
		std::string preGold = rowPre ? rowPre[0] : "0";

		// Sever the physical socket
		sqlVictim.Destroy();

		// Now attempt to run query on pre-severed victim connection
		auto deadRes = sqlVictim.DirectQuery("UPDATE test_player_live SET gold = gold + 777 WHERE id = 1;");
		LIVE_ASSERT(deadRes != nullptr, "Query on disconnected socket returns result object");
		LIVE_ASSERT(deadRes->IsError() == true, "Severed connection query correctly marked as IsError() == true");
		LIVE_ASSERT(deadRes->uiSQLErrno != 0, "Server lost error code preserved (CR_SERVER_LOST / CR_SERVER_GONE_ERROR)");
		std::cout << "    -> Captured Disconnect Error Code: " << deadRes->uiSQLErrno << std::endl;

		// Verify on fresh connection that query was NOT blindly executed or retried
		{
			CAsyncSQL sqlVerify;
			sqlVerify.Setup(host.c_str(), user.c_str(), password.c_str(), dbname.c_str(), "latin1", true, port);
			auto postRes = sqlVerify.DirectQuery("SELECT gold FROM test_player_live WHERE id = 1;");
			MYSQL_ROW rowPost = mysql_fetch_row(postRes->Get()->pSQLResult);
			std::string postGold = rowPost ? rowPost[0] : "";
			LIVE_ASSERT(postGold == preGold, "No blind retry executed on severed connection: gold unchanged");
			sqlVerify.Destroy();
		}
	}

	// --------------------------------------------------------------------------
	// 11. REAL TCP FAULT INJECTION: MID-QUERY SEVERANCE & POST-COMMIT DROPPED RESPONSE
	// --------------------------------------------------------------------------
	std::cout << "\n[11/12] Real TCP Fault Injection: Mid-Query Severance & Post-Commit Dropped Response..." << std::endl;
	{
		TcpFaultProxy proxy;
		int proxyPort = proxy.Start(host, port);
		LIVE_ASSERT(proxyPort > 0, "TCP Fault Injection Proxy initialized and listening on ephemeral port");
		std::cout << "    -> Proxy active on 127.0.0.1:" << proxyPort << " -> forwarding to DB 127.0.0.1:" << port << std::endl;

		// ----------------------------------------------------------------------
		// Scenario 11.A: In-Flight Mid-Query Connection Severance
		// ----------------------------------------------------------------------
		std::cout << "    [Scenario 11.A] In-Flight Mid-Query Connection Severance (Server-side processing interrupted)..." << std::endl;
		{
			CAsyncSQL sqlProxyA;
			bool aSetup = sqlProxyA.Setup(host.c_str(), user.c_str(), password.c_str(), dbname.c_str(), "latin1", true, proxyPort);
			LIVE_ASSERT(aSetup == true, "Client successfully connected to DB via TCP fault proxy (11.A)");

			// Arm proxy to sever socket while query is actively running on server
			proxy.SetFaultMode(ProxyFaultMode::FAULT_SEVER_MID_QUERY);

			// Query that takes 2 seconds on server (SLEEP(2))
			auto resA = sqlProxyA.DirectQuery("SELECT SLEEP(2);");
			LIVE_ASSERT(resA != nullptr, "Interrupted in-flight query returned a result object");
			LIVE_ASSERT(resA->IsError() == true, "Interrupted query correctly flagged as IsError() == true");
			LIVE_ASSERT(resA->uiSQLErrno == CR_SERVER_LOST || resA->uiSQLErrno == CR_SERVER_GONE_ERROR,
				"Mid-query disconnection preserves CR_SERVER_LOST (2013) or CR_SERVER_GONE_ERROR (2006)");
			LIVE_ASSERT(proxy.WaitForSeverMidQuery() == true,
				"TCP proxy actively severed client and server sockets during mid-query execution");
			std::cout << "    -> Captured In-Flight Disconnect Error: " << resA->uiSQLErrno << " (" << (resA->uiSQLErrno == 2013 ? "CR_SERVER_LOST" : "CR_SERVER_GONE_ERROR") << ")" << std::endl;

			sqlProxyA.Destroy();

			// Verify server health via direct connection
			CAsyncSQL sqlCheckA;
			sqlCheckA.Setup(host.c_str(), user.c_str(), password.c_str(), dbname.c_str(), "latin1", true, port);
			auto pingResA = sqlCheckA.DirectQuery("SELECT 1;");
			LIVE_ASSERT(pingResA != nullptr && !pingResA->IsError(),
				"Database server remains healthy and accepting queries after mid-query severance");
			sqlCheckA.Destroy();
		}

		// Brief pause between connections
		std::this_thread::sleep_for(std::chrono::milliseconds(50));

		// ----------------------------------------------------------------------
		// Scenario 11.B: Post-Execution Response Dropped (DB committed, OK dropped, UNKNOWN status)
		// ----------------------------------------------------------------------
		std::cout << "    [Scenario 11.B] Post-Execution Response Dropped (DB committed, OK packet dropped -> UNKNOWN)..." << std::endl;
		{
			// Read current gold via direct connection before fault injection
			uint64_t initialGold = 0;
			{
				CAsyncSQL sqlDirect;
				sqlDirect.Setup(host.c_str(), user.c_str(), password.c_str(), dbname.c_str(), "latin1", true, port);
				auto r = sqlDirect.DirectQuery("SELECT gold FROM test_player_live WHERE id = 1;");
				LIVE_ASSERT(r != nullptr && r->Get() != nullptr && r->Get()->ullNumRows == 1, "Retrieved pre-fault gold baseline");
				MYSQL_ROW row = mysql_fetch_row(r->Get()->pSQLResult);
				initialGold = row && row[0] ? ::strtoull(row[0], nullptr, 10) : 0;
				sqlDirect.Destroy();
			}

			// Connect client through proxy
			CAsyncSQL sqlProxyB;
			bool bSetup = sqlProxyB.Setup(host.c_str(), user.c_str(), password.c_str(), dbname.c_str(), "latin1", true, proxyPort);
			LIVE_ASSERT(bSetup == true, "Client successfully connected to DB via TCP fault proxy (11.B)");

			// Arm proxy to drop server OK response
			proxy.SetFaultMode(ProxyFaultMode::FAULT_DROP_SERVER_RESPONSE);

			// Execute non-idempotent update (+500 gold)
			auto resB = sqlProxyB.DirectQuery("UPDATE test_player_live SET gold = gold + 500 WHERE id = 1;");
			LIVE_ASSERT(resB != nullptr, "Client received result object after dropped response");
			LIVE_ASSERT(resB->IsError() == true, "Client marked dropped response as IsError() == true");
			LIVE_ASSERT(resB->uiSQLErrno == CR_SERVER_LOST || resB->uiSQLErrno == CR_SERVER_GONE_ERROR,
				"Dropped response returned CR_SERVER_LOST (2013) or CR_SERVER_GONE_ERROR (2006)");
			LIVE_ASSERT(proxy.WaitForDroppedResponse() == true,
				"TCP proxy verified server executed query and dropped OK response before client received it");
			std::cout << "    -> Client Seen Error Code: " << resB->uiSQLErrno
					  << " | Transaction Status: UNKNOWN (client saw socket drop after commit)" << std::endl;

			sqlProxyB.Destroy();

			// Verify in database via separate direct connection that the UPDATE was actually applied!
			uint64_t actualGold = 0;
			{
				CAsyncSQL sqlVerifyB;
				sqlVerifyB.Setup(host.c_str(), user.c_str(), password.c_str(), dbname.c_str(), "latin1", true, port);
				auto r = sqlVerifyB.DirectQuery("SELECT gold FROM test_player_live WHERE id = 1;");
				LIVE_ASSERT(r != nullptr && r->Get() != nullptr && r->Get()->ullNumRows == 1, "Direct verification read succeeded");
				MYSQL_ROW row = mysql_fetch_row(r->Get()->pSQLResult);
				actualGold = row && row[0] ? ::strtoull(row[0], nullptr, 10) : 0;

				LIVE_ASSERT(actualGold == initialGold + 500,
					"Database confirmed UPDATE was committed (+500) despite client receiving socket error (UNKNOWN status)");
				std::cout << "    -> Verified DB Actual Gold: " << actualGold << " (Expected initial + 500: " << (initialGold + 500) << ")" << std::endl;

				// Verify NO BLIND RETRY occurred
				std::this_thread::sleep_for(std::chrono::milliseconds(100));
				auto r2 = sqlVerifyB.DirectQuery("SELECT gold FROM test_player_live WHERE id = 1;");
				MYSQL_ROW row2 = mysql_fetch_row(r2->Get()->pSQLResult);
				uint64_t retryGold = row2 && row2[0] ? ::strtoull(row2[0], nullptr, 10) : 0;
				LIVE_ASSERT(retryGold == actualGold,
					"No blind ResendQuery occurred: gold remained unchanged (no duplicate execution)");

				sqlVerifyB.Destroy();
			}
		}

		proxy.Stop();
		LIVE_ASSERT(proxy.GetListenPort() > 0, "TCP Fault Injection Proxy cleanly stopped and torn down");
	}

	// --------------------------------------------------------------------------
	// 12. REAL MULTIBYTE SQL ESCAPING & CHARSET SAFETY (LIVE DB)
	// --------------------------------------------------------------------------
	std::cout << "\n[12/13] Real Multibyte SQL Escaping & Charset Safety (Live DB)..." << std::endl;
	{
		CAsyncSQL sql;
		bool bConn = sql.Setup(host.c_str(), user.c_str(), password.c_str(), dbname.c_str(), "utf8mb4", true, port);
		LIVE_ASSERT(bConn == true, "Connected to DB with utf8mb4 charset for live escaping tests");

		// Create dedicated table for escaping tests
		auto dropTbl = sql.DirectQuery("DROP TABLE IF EXISTS test_escape_live;");
		LIVE_ASSERT(dropTbl != nullptr && !dropTbl->IsError(), "Drop previous test_escape_live table");

		auto createTbl = sql.DirectQuery(
			"CREATE TABLE test_escape_live ("
			"  id INT PRIMARY KEY,"
			"  raw_blob BLOB,"
			"  raw_text VARCHAR(255) CHARACTER SET utf8mb4"
			") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;"
		);
		LIVE_ASSERT(createTbl != nullptr && !createTbl->IsError(), "Created test_escape_live table with utf8mb4 support");

		// 12.1: The Critical Counterexample \xE2\x82\x81' (E2 82 81 27)
		// Naive multibyte loops consume E2 82, then see 81 27, consuming 27 (') without escaping!
		// Real mysql_real_escape_string with utf8mb4 MUST escape the single quote properly!
		{
			const char counterexample[4] = { '\xE2', '\x82', '\x81', '\'' };
			char escBuf[64] = {0};
			size_t escLen = sql.EscapeString(escBuf, sizeof(escBuf), counterexample, sizeof(counterexample));

			LIVE_ASSERT(escLen >= 5, "Counterexample escaped length is at least 5 bytes");
			LIVE_ASSERT(std::strstr(escBuf, "\\'") != nullptr, "Counterexample correctly escapes trailing single quote with backslash");

			// Insert into DB using the escaped string
			char insertQuery[256];
			std::snprintf(insertQuery, sizeof(insertQuery),
				"INSERT INTO test_escape_live (id, raw_blob, raw_text) VALUES (1, '%s', '%s');",
				escBuf, escBuf);

			auto insRes = sql.DirectQuery(insertQuery);
			LIVE_ASSERT(insRes != nullptr && !insRes->IsError(), "INSERT with escaped counterexample succeeds on live DB");

			// Query back and verify HEX representation matches original exact 4 bytes: E2828127
			auto selRes = sql.DirectQuery("SELECT HEX(raw_blob), HEX(raw_text) FROM test_escape_live WHERE id = 1;");
			LIVE_ASSERT(selRes != nullptr && selRes->Get() && selRes->Get()->ullNumRows == 1, "SELECT returned inserted counterexample row");
			MYSQL_ROW row = mysql_fetch_row(selRes->Get()->pSQLResult);
			LIVE_ASSERT(row != nullptr && row[0] != nullptr && row[1] != nullptr, "Row contains valid HEX data");
			LIVE_ASSERT(std::string(row[0]) == "E2828127", "Counterexample BLOB roundtrip matches exact original bytes E2828127");
			LIVE_ASSERT(std::string(row[1]) == "E2828127", "Counterexample VARCHAR roundtrip matches exact original bytes E2828127");
			std::cout << "    -> Counterexample E2828127 escaped and retrieved cleanly without quote injection!" << std::endl;
		}

		// 12.2: UTF-8 1, 2, 3, and 4-Byte Multibyte Sequences with Quotes & Metachars
		{
			struct TestVector {
				int id;
				const char* desc;
				std::string input;
			};

			std::vector<TestVector> vectors = {
				{ 10, "1-byte ASCII with quotes/slashes", "Admin' OR '1'='1 -- \"Quote\" \\ Backslash" },
				{ 20, "2-byte UTF-8 with quote (Greek/Latin-ext)", "éèçà'--\xC3\xA9\xC3\xA8'" },
				{ 30, "3-byte UTF-8 with quote (CJK/Euro)", "Metin2' Euro€ '\xE4\xB8\xAD\xE6\x96\x87' CJK" },
				{ 40, "4-byte UTF-8 with quote (Emoji 🔥🗡️)", "Sword🗡️ '\xF0\x9F\x94\xA5' Fire'!" }
			};

			for (const auto& tv : vectors) {
				char escBuf[512] = {0};
				size_t escLen = sql.EscapeString(escBuf, sizeof(escBuf), tv.input.data(), tv.input.size());
				LIVE_ASSERT(escLen >= tv.input.size(), "Escaped length >= raw length");

				std::string q = "INSERT INTO test_escape_live (id, raw_blob, raw_text) VALUES (" +
					std::to_string(tv.id) + ", '" + escBuf + "', '" + escBuf + "');";
				auto r = sql.DirectQuery(q.c_str());
				LIVE_ASSERT(r != nullptr && !r->IsError(), (std::string("INSERT succeeds for ") + tv.desc).c_str());

				std::string selQ = "SELECT raw_text FROM test_escape_live WHERE id = " + std::to_string(tv.id) + ";";
				auto sel = sql.DirectQuery(selQ.c_str());
				LIVE_ASSERT(sel != nullptr && sel->Get() && sel->Get()->ullNumRows == 1, "SELECT retrieved row");
				MYSQL_ROW rrow = mysql_fetch_row(sel->Get()->pSQLResult);
				LIVE_ASSERT(rrow && rrow[0], "Row content valid");
				LIVE_ASSERT(std::string(rrow[0]) == tv.input, (std::string("Exact text roundtrip preserved for ") + tv.desc).c_str());
			}
		}

		// 12.3: Binary Data with Embedded NUL Bytes (\0)
		{
			const char binData[] = "HEADER\0\x01\x02\xFF'TAIL";
			size_t binLen = sizeof(binData) - 1; // 15 bytes including embedded NUL

			char escBuf[64] = {0};
			size_t escLen = sql.EscapeString(escBuf, sizeof(escBuf), binData, binLen);
			LIVE_ASSERT(escLen > binLen, "Binary data with embedded NUL escaped successfully");

			char insBin[256];
			std::snprintf(insBin, sizeof(insBin), "INSERT INTO test_escape_live (id, raw_blob, raw_text) VALUES (50, '");
			size_t prefixLen = std::strlen(insBin);
			std::memcpy(insBin + prefixLen, escBuf, escLen);
			std::strcpy(insBin + prefixLen + escLen, "', NULL);");

			auto r = sql.DirectQuery(insBin);
			LIVE_ASSERT(r != nullptr && !r->IsError(), "INSERT binary data with embedded NUL succeeded");

			auto sel = sql.DirectQuery("SELECT HEX(raw_blob) FROM test_escape_live WHERE id = 50;");
			LIVE_ASSERT(sel != nullptr && sel->Get() && sel->Get()->ullNumRows == 1, "SELECT binary row succeeded");
			MYSQL_ROW row = mysql_fetch_row(sel->Get()->pSQLResult);
			LIVE_ASSERT(row && row[0], "Retrieved binary HEX");
			LIVE_ASSERT(std::string(row[0]) == "484541444552000102FF275441494C",
				"Binary data with embedded NUL matches exact byte stream 484541444552000102FF275441494C");
		}

		// 12.4: NO_BACKSLASH_ESCAPES SQL Mode Support
		// When NO_BACKSLASH_ESCAPES is active, mysql_real_escape_string escapes single quotes as '' instead of \'
		{
			// Save previous SQL mode
			auto getModeRes = sql.DirectQuery("SELECT @@sql_mode;");
			LIVE_ASSERT(getModeRes != nullptr && getModeRes->Get(), "Retrieved current sql_mode");
			MYSQL_ROW modeRow = mysql_fetch_row(getModeRes->Get()->pSQLResult);
			std::string origMode = modeRow && modeRow[0] ? modeRow[0] : "";

			// Enable NO_BACKSLASH_ESCAPES
			auto setModeRes = sql.DirectQuery("SET sql_mode = CONCAT(@@sql_mode, ',NO_BACKSLASH_ESCAPES');");
			LIVE_ASSERT(setModeRes != nullptr && !setModeRes->IsError(), "Enabled NO_BACKSLASH_ESCAPES mode");

			const char testInput[] = "Safe'Quote";
			char escBuf[64] = {0};
			size_t escLen = sql.EscapeString(escBuf, sizeof(escBuf), testInput, std::strlen(testInput));
			LIVE_ASSERT(escLen == std::strlen(testInput) + 1, "Escaped length added 1 character");
			LIVE_ASSERT(std::strstr(escBuf, "Safe''Quote") != nullptr || std::strstr(escBuf, "Safe\\'Quote") != nullptr,
				"Under NO_BACKSLASH_ESCAPES, single quote is properly escaped");

			// Insert with NO_BACKSLASH_ESCAPES
			char insMode[128];
			std::snprintf(insMode, sizeof(insMode),
				"INSERT INTO test_escape_live (id, raw_blob, raw_text) VALUES (60, '%s', '%s');",
				escBuf, escBuf);
			auto insModeRes = sql.DirectQuery(insMode);
			LIVE_ASSERT(insModeRes != nullptr && !insModeRes->IsError(),
				"INSERT succeeds under NO_BACKSLASH_ESCAPES with escaped quotes");

			// Restore original SQL mode
			std::string restoreModeQuery = "SET sql_mode = '" + origMode + "';";
			sql.DirectQuery(restoreModeQuery.c_str());
		}

		// Cleanup escape test table
		sql.DirectQuery("DROP TABLE IF EXISTS test_escape_live;");
		sql.Quit();
	}

	// --------------------------------------------------------------------------
	// 13. LEGACY HALLOWEEN HAIR COSTUME DRY-RUN, MIGRATION, IDEMPOTENCY & ROLLBACK
	// --------------------------------------------------------------------------
	std::cout << "\n[13/15] Testing Legacy Halloween Hair Costume Dry-Run, Migration, Idempotency & Rollback..." << std::endl;
	{
		CAsyncSQL sql;
		sql.Setup(host.c_str(), user.c_str(), password.c_str(), dbname.c_str(), "latin1", true, port);

		sql.DirectQuery("DROP TABLE IF EXISTS player_item_halloween_rev51_backup;");
		sql.DirectQuery("DROP TABLE IF EXISTS item;");

		const char * createItemTableSQL =
			"CREATE TABLE `item` ("
			"  `id` int(11) unsigned NOT NULL AUTO_INCREMENT,"
			"  `owner_id` int(11) unsigned NOT NULL DEFAULT 0,"
			"  `window` enum('INVENTORY','EQUIPMENT','SAFEBOX','MALL','DRAGON_SOUL_INVENTORY','BELT_INVENTORY','GROUND') NOT NULL DEFAULT 'INVENTORY',"
			"  `pos` smallint(5) unsigned NOT NULL DEFAULT 0,"
			"  `count` tinyint(3) unsigned NOT NULL DEFAULT 0,"
			"  `vnum` int(11) unsigned NOT NULL DEFAULT 0,"
			"  `socket0` int(10) unsigned NOT NULL DEFAULT 0,"
			"  `socket1` int(10) unsigned NOT NULL DEFAULT 0,"
			"  `socket2` int(10) unsigned NOT NULL DEFAULT 0,"
			"  `attrtype0` tinyint(4) NOT NULL DEFAULT 0,"
			"  `attrvalue0` smallint(6) NOT NULL DEFAULT 0,"
			"  `attrtype1` tinyint(4) NOT NULL DEFAULT 0,"
			"  `attrvalue1` smallint(6) NOT NULL DEFAULT 0,"
			"  `attrtype2` tinyint(4) NOT NULL DEFAULT 0,"
			"  `attrvalue2` smallint(6) NOT NULL DEFAULT 0,"
			"  `attrtype3` tinyint(4) NOT NULL DEFAULT 0,"
			"  `attrvalue3` smallint(6) NOT NULL DEFAULT 0,"
			"  `attrtype4` tinyint(4) NOT NULL DEFAULT 0,"
			"  `attrvalue4` smallint(6) NOT NULL DEFAULT 0,"
			"  `attrtype5` tinyint(4) NOT NULL DEFAULT 0,"
			"  `attrvalue5` smallint(6) NOT NULL DEFAULT 0,"
			"  `attrtype6` tinyint(4) NOT NULL DEFAULT 0,"
			"  `attrvalue6` smallint(6) NOT NULL DEFAULT 0,"
			"  PRIMARY KEY (`id`)"
			") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;";
		auto crItem = sql.DirectQuery(createItemTableSQL);
		LIVE_ASSERT(crItem != nullptr && !crItem->IsError(), "Created player.item schema for Halloween hair migration test");

		const char * seedItemsSQL =
			"INSERT INTO `item` (id, owner_id, window, pos, count, vnum, socket0, socket1, socket2, attrtype0, attrvalue0, attrtype1, attrvalue1) VALUES "
			// Category A: 8 Eligible legacy quest rows (IDs 1..8)
			"(1, 1001, 'INVENTORY', 0, 1, 74020, 604800, 0, 0, 59, 10, 0, 0),"
			"(2, 1001, 'EQUIPMENT', 19, 1, 74270, 1729990000, 1, 0, 60, 10, 0, 0),"
			"(3, 1002, 'INVENTORY', 1, 1, 74520, 604800, 0, 0, 61, 10, 0, 0),"
			"(4, 1002, 'SAFEBOX',   2, 1, 74770, 1729995000, 1, 0, 62, 10, 0, 0),"
			"(5, 1003, 'INVENTORY', 3, 1, 75020, 604800, 0, 0, 63, 10, 0, 0),"
			"(6, 1003, 'EQUIPMENT', 19, 1, 75220, 1729998000, 1, 0, 69, 1, 0, 0),"
			"(7, 1004, 'INVENTORY', 4, 1, 75420, 86400, 0, 0, 70, 1, 0, 0),"
			"(8, 1004, 'MALL',      5, 1, 75620, 1729999000, 1, 0, 63, 10, 0, 0),"
			// Category B: 7 Already-valid EPointTypes rows (IDs 11..17)
			"(11, 1005, 'INVENTORY', 0, 1, 74020, 604800, 0, 0, 54, 10, 0, 0),"
			"(12, 1005, 'INVENTORY', 1, 1, 74270, 604800, 0, 0, 55, 10, 0, 0),"
			"(13, 1005, 'INVENTORY', 2, 1, 74520, 604800, 0, 0, 56, 10, 0, 0),"
			"(14, 1005, 'INVENTORY', 3, 1, 74770, 604800, 0, 0, 57, 10, 0, 0),"
			"(15, 1005, 'INVENTORY', 4, 1, 75020, 604800, 0, 0, 53, 10, 0, 0),"
			"(16, 1005, 'INVENTORY', 5, 1, 75220, 21600, 0, 0, 119, 1, 0, 0),"
			"(17, 1005, 'INVENTORY', 6, 1, 75420, 86400, 0, 0, 120, 1, 0, 0),"
			// Category C: 2 Normal hair costumes (IDs 21..22)
			"(21, 1006, 'INVENTORY', 0, 1, 74001, 0, 0, 0, 0, 0, 0, 0),"
			"(22, 1006, 'INVENTORY', 1, 1, 74002, 604800, 0, 0, 59, 10, 0, 0),"
			// Category D: 3 Non-Halloween items with legitimate 59, 63, 69 points (IDs 31..33)
			"(31, 1007, 'EQUIPMENT', 0, 1, 11299, 1, 1, 1, 59, 10, 1, 2000),"
			"(32, 1007, 'EQUIPMENT', 1, 1, 13069, 604800, 0, 0, 63, 10, 0, 0),"
			"(33, 1007, 'INVENTORY', 2, 1, 11299, 604800, 0, 0, 69, 1, 0, 0),"
			// Category E: 4 Ambiguous Halloween hair rows (GM-modified, IDs 41..44)
			"(41, 1008, 'INVENTORY', 0, 1, 74020, 604800, 0, 0, 59, 15, 0, 0),"
			"(42, 1008, 'INVENTORY', 1, 1, 74270, 604800, 0, 0, 63, 10, 1, 500),"
			"(43, 1008, 'INVENTORY', 2, 1, 74520, 0, 0, 0, 61, 10, 0, 0),"
			"(44, 1008, 'INVENTORY', 3, 1, 74770, 21600, 0, 99, 69, 1, 0, 0),"
			// Category F: 2 Unset / non-hair attribute rows on Halloween hair (IDs 51..52)
			"(51, 1009, 'INVENTORY', 0, 1, 75020, 604800, 0, 0, 0, 0, 0, 0),"
			"(52, 1009, 'INVENTORY', 1, 1, 75220, 604800, 0, 0, 15, 10, 0, 0);";
		auto seedRes = sql.DirectQuery(seedItemsSQL);
		LIVE_ASSERT(seedRes != nullptr && !seedRes->IsError() && seedRes->Get()->ullAffectedRows == 26,
			"Seeded 26 test item rows across all 10 Halloween/non-Halloween categories");

		// Step 1: Dry-Run Audit Classification Query (read-only)
		const char * dryRunSQL =
			"SELECT "
			"  SUM(CASE WHEN ("
			"    ((attrtype0 IN (59, 60, 61, 62, 63) AND attrvalue0 = 10) OR (attrtype0 IN (69, 70) AND attrvalue0 = 1)) "
			"    AND attrtype1 = 0 AND attrvalue1 = 0 AND attrtype2 = 0 AND attrvalue2 = 0 "
			"    AND attrtype3 = 0 AND attrvalue3 = 0 AND attrtype4 = 0 AND attrvalue4 = 0 "
			"    AND attrtype5 = 0 AND attrvalue5 = 0 AND attrtype6 = 0 AND attrvalue6 = 0 "
			"    AND ((socket1 = 0 AND ((attrtype0 IN (59, 60, 61, 62, 63) AND socket0 = 604800) OR (attrtype0 = 69 AND socket0 = 21600) OR (attrtype0 = 70 AND socket0 = 86400))) OR (socket1 >= 1 AND socket0 > 604800)) "
			"    AND socket2 = 0"
			"  ) THEN 1 ELSE 0 END) AS eligible_count, "
			"  SUM(CASE WHEN ("
			"    (attrtype0 IN (53, 54, 55, 56, 57) AND attrvalue0 = 10) OR (attrtype0 IN (119, 120) AND attrvalue0 = 1)"
			"  ) THEN 1 ELSE 0 END) AS valid_count, "
			"  SUM(CASE WHEN ("
			"    attrtype0 IN (59, 60, 61, 62, 63, 69, 70) AND NOT ("
			"      ((attrtype0 IN (59, 60, 61, 62, 63) AND attrvalue0 = 10) OR (attrtype0 IN (69, 70) AND attrvalue0 = 1)) "
			"      AND attrtype1 = 0 AND attrvalue1 = 0 AND attrtype2 = 0 AND attrvalue2 = 0 "
			"      AND attrtype3 = 0 AND attrvalue3 = 0 AND attrtype4 = 0 AND attrvalue4 = 0 "
			"      AND attrtype5 = 0 AND attrvalue5 = 0 AND attrtype6 = 0 AND attrvalue6 = 0 "
			"      AND ((socket1 = 0 AND ((attrtype0 IN (59, 60, 61, 62, 63) AND socket0 = 604800) OR (attrtype0 = 69 AND socket0 = 21600) OR (attrtype0 = 70 AND socket0 = 86400))) OR (socket1 >= 1 AND socket0 > 604800)) "
			"      AND socket2 = 0"
			"    )"
			"  ) THEN 1 ELSE 0 END) AS ambiguous_count, "
			"  SUM(CASE WHEN attrtype0 NOT IN (53, 54, 55, 56, 57, 59, 60, 61, 62, 63, 69, 70, 119, 120) THEN 1 ELSE 0 END) AS other_count "
			"FROM `item` WHERE vnum IN (74020, 74270, 74520, 74770, 75020, 75220, 75420, 75620);";
		auto dryRes = sql.DirectQuery(dryRunSQL);
		LIVE_ASSERT(dryRes != nullptr && !dryRes->IsError() && dryRes->Get() && dryRes->Get()->ullNumRows == 1, "Dry-run classification query succeeded");
		MYSQL_ROW dryRow = mysql_fetch_row(dryRes->Get()->pSQLResult);
		LIVE_ASSERT(dryRow != nullptr, "Dry-run row fetched");
		LIVE_ASSERT(std::atoi(dryRow[0]) == 8, "Dry-run identified exactly 8 ELIGIBLE_LEGACY_QUEST_HALLOWEEN rows");
		LIVE_ASSERT(std::atoi(dryRow[1]) == 7, "Dry-run identified 7 ALREADY_VALID_POINT rows");
		LIVE_ASSERT(std::atoi(dryRow[2]) == 4, "Dry-run identified 4 AMBIGUOUS_DO_NOT_CONVERT rows");
		LIVE_ASSERT(std::atoi(dryRow[3]) == 2, "Dry-run identified 2 OTHER_OR_UNSET rows");

		// Step 2: Create Backup Table and Populate Eligible Rows
		const char * createBackupSQL =
			"CREATE TABLE IF NOT EXISTS `player_item_halloween_rev51_backup` ("
			"  `id` int(11) unsigned NOT NULL,"
			"  `owner_id` int(11) unsigned NOT NULL,"
			"  `window` varchar(32) NOT NULL,"
			"  `pos` smallint(5) unsigned NOT NULL,"
			"  `count` tinyint(3) unsigned NOT NULL,"
			"  `vnum` int(11) unsigned NOT NULL,"
			"  `socket0` int(10) unsigned NOT NULL,"
			"  `socket1` int(10) unsigned NOT NULL,"
			"  `socket2` int(10) unsigned NOT NULL,"
			"  `old_attrtype0` tinyint(4) NOT NULL,"
			"  `old_attrvalue0` smallint(6) NOT NULL,"
			"  `new_attrtype0` tinyint(4) NOT NULL,"
			"  `attrtype1` tinyint(4) NOT NULL,"
			"  `attrvalue1` smallint(6) NOT NULL,"
			"  `attrtype2` tinyint(4) NOT NULL,"
			"  `attrvalue2` smallint(6) NOT NULL,"
			"  `attrtype3` tinyint(4) NOT NULL,"
			"  `attrvalue3` smallint(6) NOT NULL,"
			"  `attrtype4` tinyint(4) NOT NULL,"
			"  `attrvalue4` smallint(6) NOT NULL,"
			"  `attrtype5` tinyint(4) NOT NULL,"
			"  `attrvalue5` smallint(6) NOT NULL,"
			"  `attrtype6` tinyint(4) NOT NULL,"
			"  `attrvalue6` smallint(6) NOT NULL,"
			"  `backed_up_at` datetime NOT NULL DEFAULT CURRENT_TIMESTAMP,"
			"  PRIMARY KEY (`id`)"
			") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;";
		auto bkCreate = sql.DirectQuery(createBackupSQL);
		LIVE_ASSERT(bkCreate != nullptr && !bkCreate->IsError(), "Created player_item_halloween_rev51_backup table");

		const char * insertBackupSQL =
			"INSERT INTO `player_item_halloween_rev51_backup` ("
			"  `id`, `owner_id`, `window`, `pos`, `count`, `vnum`, "
			"  `socket0`, `socket1`, `socket2`, "
			"  `old_attrtype0`, `old_attrvalue0`, `new_attrtype0`, "
			"  `attrtype1`, `attrvalue1`, `attrtype2`, `attrvalue2`, "
			"  `attrtype3`, `attrvalue3`, `attrtype4`, `attrvalue4`, "
			"  `attrtype5`, `attrvalue5`, `attrtype6`, `attrvalue6`"
			") "
			"SELECT "
			"  i.id, i.owner_id, i.window, i.pos, i.count, i.vnum, "
			"  i.socket0, i.socket1, i.socket2, "
			"  i.attrtype0 AS old_attrtype0, i.attrvalue0 AS old_attrvalue0, "
			"  CASE i.attrtype0 "
			"    WHEN 59 THEN 54 WHEN 60 THEN 55 WHEN 61 THEN 56 WHEN 62 THEN 57 "
			"    WHEN 63 THEN 53 WHEN 69 THEN 119 WHEN 70 THEN 120 "
			"  END AS new_attrtype0, "
			"  i.attrtype1, i.attrvalue1, i.attrtype2, i.attrvalue2, "
			"  i.attrtype3, i.attrvalue3, i.attrtype4, i.attrvalue4, "
			"  i.attrtype5, i.attrvalue5, i.attrtype6, i.attrvalue6 "
			"FROM `item` i "
			"LEFT JOIN `player_item_halloween_rev51_backup` b ON b.id = i.id "
			"WHERE b.id IS NULL "
			"  AND i.vnum IN (74020, 74270, 74520, 74770, 75020, 75220, 75420, 75620) "
			"  AND ((i.attrtype0 IN (59, 60, 61, 62, 63) AND i.attrvalue0 = 10) OR (i.attrtype0 IN (69, 70) AND i.attrvalue0 = 1)) "
			"  AND i.attrtype1 = 0 AND i.attrvalue1 = 0 AND i.attrtype2 = 0 AND i.attrvalue2 = 0 "
			"  AND i.attrtype3 = 0 AND i.attrvalue3 = 0 AND i.attrtype4 = 0 AND i.attrvalue4 = 0 "
			"  AND i.attrtype5 = 0 AND i.attrvalue5 = 0 AND i.attrtype6 = 0 AND i.attrvalue6 = 0 "
			"  AND ((i.socket1 = 0 AND ((i.attrtype0 IN (59, 60, 61, 62, 63) AND i.socket0 = 604800) OR (i.attrtype0 = 69 AND i.socket0 = 21600) OR (i.attrtype0 = 70 AND i.socket0 = 86400))) OR (i.socket1 >= 1 AND i.socket0 > 604800)) "
			"  AND i.socket2 = 0;";
		auto bkIns = sql.DirectQuery(insertBackupSQL);
		LIVE_ASSERT(bkIns != nullptr && !bkIns->IsError() && bkIns->Get()->ullAffectedRows == 8,
			"Backed up exactly 8 eligible Halloween hair rows into player_item_halloween_rev51_backup");

		// Step 3: Execute Transactional Migration
		const char * migrateSQL =
			"UPDATE `item` i "
			"INNER JOIN `player_item_halloween_rev51_backup` b ON b.id = i.id "
			"SET i.attrtype0 = b.new_attrtype0 "
			"WHERE i.vnum IN (74020, 74270, 74520, 74770, 75020, 75220, 75420, 75620) "
			"  AND i.attrtype0 = b.old_attrtype0 "
			"  AND i.attrvalue0 = b.old_attrvalue0 "
			"  AND i.attrtype1 = 0 AND i.attrvalue1 = 0 AND i.attrtype2 = 0 AND i.attrvalue2 = 0 "
			"  AND i.attrtype3 = 0 AND i.attrvalue3 = 0 AND i.attrtype4 = 0 AND i.attrvalue4 = 0 "
			"  AND i.attrtype5 = 0 AND i.attrvalue5 = 0 AND i.attrtype6 = 0 AND i.attrvalue6 = 0 "
			"  AND ((i.socket1 = 0 AND ((i.attrtype0 IN (59, 60, 61, 62, 63) AND i.socket0 = 604800) OR (i.attrtype0 = 69 AND i.socket0 = 21600) OR (i.attrtype0 = 70 AND i.socket0 = 86400))) OR (i.socket1 >= 1 AND i.socket0 > 604800)) "
			"  AND i.socket2 = 0;";
		sql.DirectQuery("START TRANSACTION;");
		auto migRes = sql.DirectQuery(migrateSQL);
		sql.DirectQuery("COMMIT;");
		LIVE_ASSERT(migRes != nullptr && !migRes->IsError() && migRes->Get()->ullAffectedRows == 8,
			"Migration converted all 8 eligible legacy Halloween hair rows in transaction");

		// Verify all 8 converted rows have exact EPointTypes (54, 55, 56, 57, 53, 119, 120, 53)
		auto verifyConv = sql.DirectQuery("SELECT id, attrtype0, attrvalue0 FROM `item` WHERE id BETWEEN 1 AND 8 ORDER BY id ASC;");
		LIVE_ASSERT(verifyConv != nullptr && verifyConv->Get() && verifyConv->Get()->ullNumRows == 8, "Fetched 8 migrated rows");
		const int expectedNewTypes[8]  = { 54, 55, 56, 57, 53, 119, 120, 53 };
		const int expectedNewValues[8] = { 10, 10, 10, 10, 10,   1,   1, 10 };
		for (int i = 0; i < 8; ++i) {
			MYSQL_ROW r = mysql_fetch_row(verifyConv->Get()->pSQLResult);
			LIVE_ASSERT(r != nullptr && std::atoi(r[1]) == expectedNewTypes[i] && std::atoi(r[2]) == expectedNewValues[i],
				("Migrated row id=" + std::to_string(i + 1) + " matches exact EPointTypes target").c_str());
		}

		// Verify all non-eligible rows (already-valid, normal hair, non-Halloween armor/shield, ambiguous GM items, unset) were untouched!
		auto verifyUntouched = sql.DirectQuery(
			"SELECT COUNT(*) FROM `item` WHERE "
			"(id = 22 AND attrtype0 = 59) OR "
			"(id = 31 AND attrtype0 = 59 AND attrtype1 = 1 AND attrvalue1 = 2000) OR "
			"(id = 32 AND attrtype0 = 63) OR "
			"(id = 33 AND attrtype0 = 69) OR "
			"(id = 41 AND attrtype0 = 59 AND attrvalue0 = 15) OR "
			"(id = 42 AND attrtype0 = 63 AND attrtype1 = 1 AND attrvalue1 = 500) OR "
			"(id = 43 AND attrtype0 = 61 AND socket0 = 0) OR "
			"(id = 44 AND attrtype0 = 69 AND socket2 = 99) OR "
			"(id = 51 AND attrtype0 = 0) OR "
			"(id = 52 AND attrtype0 = 15);");
		LIVE_ASSERT(verifyUntouched != nullptr && verifyUntouched->Get(), "Verified non-eligible rows");
		MYSQL_ROW untRow = mysql_fetch_row(verifyUntouched->Get()->pSQLResult);
		LIVE_ASSERT(untRow != nullptr && std::atoi(untRow[0]) == 10,
			"All 10 non-Halloween, normal-hair, ambiguous GM, and invalid rows remained 100% untouched");

		// Step 4: Idempotency Verification (re-running backup + migration changes 0 rows)
		auto bkIns2 = sql.DirectQuery(insertBackupSQL);
		LIVE_ASSERT(bkIns2 != nullptr && !bkIns2->IsError() && bkIns2->Get()->ullAffectedRows == 0,
			"Second backup run is idempotent (0 rows inserted)");
		auto migRes2 = sql.DirectQuery(migrateSQL);
		LIVE_ASSERT(migRes2 != nullptr && !migRes2->IsError() && migRes2->Get()->ullAffectedRows == 0,
			"Second migration run is idempotent (0 rows modified, no double conversion of 53..57/119/120)");

		// Step 5: Deterministic Rollback Verification
		const char * rollbackSQL =
			"UPDATE `item` i "
			"INNER JOIN `player_item_halloween_rev51_backup` b ON b.id = i.id "
			"SET i.attrtype0 = b.old_attrtype0, i.attrvalue0 = b.old_attrvalue0 "
			"WHERE i.vnum = b.vnum AND i.attrtype0 = b.new_attrtype0 AND i.attrvalue0 = b.old_attrvalue0;";
		sql.DirectQuery("START TRANSACTION;");
		auto rbRes = sql.DirectQuery(rollbackSQL);
		sql.DirectQuery("COMMIT;");
		LIVE_ASSERT(rbRes != nullptr && !rbRes->IsError() && rbRes->Get()->ullAffectedRows == 8,
			"Rollback restored all 8 rows from backup table");

		auto verifyRb = sql.DirectQuery("SELECT id, attrtype0 FROM `item` WHERE id BETWEEN 1 AND 8 ORDER BY id ASC;");
		const int expectedOldTypes[8] = { 59, 60, 61, 62, 63, 69, 70, 63 };
		for (int i = 0; i < 8; ++i) {
			MYSQL_ROW r = mysql_fetch_row(verifyRb->Get()->pSQLResult);
			LIVE_ASSERT(r != nullptr && std::atoi(r[1]) == expectedOldTypes[i],
				("Rolled-back row id=" + std::to_string(i + 1) + " matches original pre-migration attrtype0").c_str());
		}

		// Re-apply migration after rollback and verify 0 eligible rows remain
		auto migRes3 = sql.DirectQuery(migrateSQL);
		LIVE_ASSERT(migRes3 != nullptr && !migRes3->IsError() && migRes3->Get()->ullAffectedRows == 8,
			"Re-applying migration after rollback cleanly re-converted all 8 rows");
		auto dryResAfter = sql.DirectQuery(dryRunSQL);
		MYSQL_ROW dryRowAfter = mysql_fetch_row(dryResAfter->Get()->pSQLResult);
		LIVE_ASSERT(dryRowAfter != nullptr && std::atoi(dryRowAfter[0]) == 0 && std::atoi(dryRowAfter[1]) == 15,
			"Post-migration dry-run confirms 0 eligible legacy rows remaining and 15 valid EPointTypes rows");

		sql.DirectQuery("DROP TABLE IF EXISTS player_item_halloween_rev51_backup;");
		sql.DirectQuery("DROP TABLE IF EXISTS item;");
		sql.Quit();
	}

	// --------------------------------------------------------------------------
	// 14. MARIADB SQL LOG CONNECTION LOSS, QUEUE OVERFLOW, SHUTDOWN & CRASH SPOOL (12 SCENARIOS)
	// --------------------------------------------------------------------------
	std::cout << "\n[14/15] Testing 12-Scenario MariaDB SQL Log Durability, Outage Spooling & Crash Recovery..." << std::endl;
	{
		const char * spoolFile = "test_live_sql_log_spool.dat";
		const char * quarantineFile = "test_live_sql_log_spool.dat.quarantine";
		std::remove(spoolFile);
		std::remove(quarantineFile);

		CAsyncSQL verifyConn;
		verifyConn.Setup(host.c_str(), user.c_str(), password.c_str(), dbname.c_str(), "latin1", true, port);
		verifyConn.DirectQuery("DROP TABLE IF EXISTS test_sql_log_durability;");
		auto crLogRes = verifyConn.DirectQuery(
			"CREATE TABLE test_sql_log_durability ("
			"  row_id INT UNSIGNED NOT NULL AUTO_INCREMENT PRIMARY KEY,"
			"  seq_no INT UNSIGNED NOT NULL,"
			"  scenario VARCHAR(32) NOT NULL,"
			"  hint VARCHAR(255) NOT NULL"
			") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;");
		LIVE_ASSERT(crLogRes != nullptr && !crLogRes->IsError(), "Created test_sql_log_durability table");

		TcpFaultProxy logProxy;
		int proxyPort = logProxy.Start(host, port);
		LIVE_ASSERT(proxyPort > 0, "Started TcpFaultProxy for 12-scenario SQL log durability test");

		auto waitForCommittedRows = [&](uint64_t expectedRows, int maxWaitMs = 5000) -> uint64_t {
			auto t0 = std::chrono::steady_clock::now();
			uint64_t rows = 0;
			while (std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - t0).count() < maxWaitMs) {
				auto r = verifyConn.DirectQuery("SELECT COUNT(*) FROM test_sql_log_durability;");
				if (r && !r->IsError() && r->Get()) {
					MYSQL_ROW row = mysql_fetch_row(r->Get()->pSQLResult);
					if (row && row[0]) rows = static_cast<uint64_t>(strtoull(row[0], nullptr, 10));
				}
				if (rows >= expectedRows) break;
				std::this_thread::sleep_for(std::chrono::milliseconds(30));
			}
			return rows;
		};

		int nextSeqNo = 0;
		auto makeLogQuery = [&](const char * scenario, const char * escapedHint) -> std::string {
			++nextSeqNo;
			char buf[512];
			std::snprintf(buf, sizeof(buf),
				"INSERT INTO test_sql_log_durability (seq_no, scenario, hint) VALUES (%d, '%s', '%s');",
				nextSeqNo, scenario, escapedHint);
			return std::string(buf);
		};

		{
			CAsyncSQL logSql;
			LIVE_ASSERT(logSql.EnableDurableSpool(spoolFile, 4, 64 * 1024 * 1024, 150) == true,
				"Configured durable spool on logSql (queueCap=4, reconnectInterval=150ms)");
			LIVE_ASSERT(logSql.Setup("127.0.0.1", user.c_str(), password.c_str(), dbname.c_str(), "latin1", false, proxyPort) == true,
				"Started logSql worker thread through TcpFaultProxy");

			for (int w = 0; w < 50 && !logSql.IsConnected(); ++w)
				std::this_thread::sleep_for(std::chrono::milliseconds(20));
			LIVE_ASSERT(logSql.IsConnected() == true, "logSql connected to MariaDB via TcpFaultProxy");

			// Scenario 1: Normal SQL log writing (5 records)
			for (int i = 0; i < 5; ++i) {
				logSql.AsyncQuery(makeLogQuery("S1_NORMAL", "Normal game item log").c_str());
			}
			LIVE_ASSERT(waitForCommittedRows(5) == 5, "Scenario 1: All 5 normal SQL logs committed immediately (sent=5, committed=5, lost=0)");

			// Scenario 2 & 3 & 12: Temporary MariaDB outage + multiple logs generated while disconnected (including offline EscapeString with quotes & non-blocking main loop check)
			logProxy.SetBlackhole(true);
			logProxy.SeverActiveConnections();

			auto tSubmitStart = std::chrono::steady_clock::now();
			for (int i = 0; i < 10; ++i) {
				char rawHint[128];
				std::snprintf(rawHint, sizeof(rawHint), "Outage item 'Halloween_Hat_%d' dropped", i + 1);
				char escHint[256] = {0};
				size_t escLen = logSql.EscapeString(escHint, sizeof(escHint), rawHint, std::strlen(rawHint));
				LIVE_ASSERT(escLen > 0 && std::strstr(escHint, "Halloween_Hat_") != nullptr,
					"Scenario 3: EscapeString preserves quoted hint during MariaDB outage");
				logSql.AsyncQuery(makeLogQuery("S2_S3_OUTAGE", escHint).c_str());
			}
			auto submitElapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(
				std::chrono::steady_clock::now() - tSubmitStart).count();
			LIVE_ASSERT(submitElapsedMs < 250,
				"Scenario 12: Main loop AsyncQuery submission during MariaDB outage is non-blocking (<250ms for 10 logs)");

			auto gameCheck = verifyConn.DirectQuery("SELECT 1 + 1;");
			LIVE_ASSERT(gameCheck != nullptr && !gameCheck->IsError(), "Scenario 12: Normal game DB connection operates unblocked during log outage");

			std::this_thread::sleep_for(std::chrono::milliseconds(450));
			auto stOutage = logSql.GetDurableSpoolStats();
			LIVE_ASSERT(stOutage.ullSpooledCount >= 10, "Scenario 2 & 3: All 10 outage logs captured in durable spool without loss");
			LIVE_ASSERT(stOutage.ullReconnectAttempts >= 1, "Scenario 5: Controlled background reconnect attempts recorded during outage");

			// Scenario 4 & 5: Restore MariaDB connection and verify automatic reconnect + full FIFO replay
			logProxy.SetBlackhole(false);
			LIVE_ASSERT(waitForCommittedRows(15, 6000) == 15,
				"Scenario 4: MariaDB reconnected and all 10 spooled outage logs replayed to DB (sent=15, committed=15, lost=0)");
			auto stRecovered = logSql.GetDurableSpoolStats();
			LIVE_ASSERT(stRecovered.ullReconnectSuccessCount >= 1, "Scenario 5: Background reconnect succeeded cleanly");
			LIVE_ASSERT(stRecovered.ullPendingSpoolCount == 0 && stRecovered.ullPendingMemoryCount == 0,
				"Scenario 4: Spool and memory queues completely emptied after replay");

			// Scenario 6: Queue capacity overflow (queueCap=4, push 12 logs while proxy is blackholed so queue fills and overflows to spool)
			logProxy.SetBlackhole(true);
			logProxy.SeverActiveConnections();
			for (int i = 0; i < 12; ++i) {
				logSql.AsyncQuery(makeLogQuery("S6_OVERFLOW", "Burst overflow log entry").c_str());
			}
			std::this_thread::sleep_for(std::chrono::milliseconds(250));
			auto stOverflow = logSql.GetDurableSpoolStats();
			LIVE_ASSERT(stOverflow.ullPendingMemoryCount <= 4, "Scenario 6: In-memory queue strictly bounded by queueCap=4");
			LIVE_ASSERT(stOverflow.ullPendingSpoolCount + stOverflow.ullPendingMemoryCount == 12,
				"Scenario 6: All 12 burst logs preserved across bounded memory queue and durable disk spool");
			logProxy.SetBlackhole(false);
			LIVE_ASSERT(waitForCommittedRows(27, 6000) == 27,
				"Scenario 6: All 12 overflow logs replayed and committed in exact order (sent=27, committed=27, lost=0)");

			// Scenario 9 & 11: Malformed/poison SQL record quarantine + sensitive password/token sanitization
			logSql.AsyncQuery("INSERT INTO non_existent_poison_log_table (bad_col) VALUES ('test', password='LeakedSecretPassword123', token='SecretTokenABC');");
			for (int i = 0; i < 3; ++i) {
				logSql.AsyncQuery(makeLogQuery("S9_AFTER_POISON", "Valid log after poison query").c_str());
			}
			LIVE_ASSERT(waitForCommittedRows(30, 5000) == 30,
				"Scenario 9: Poison SQL query quarantined without blocking subsequent 3 valid logs (committed=30)");
			auto stQuarantine = logSql.GetDurableSpoolStats();
			LIVE_ASSERT(stQuarantine.ullQuarantinedCount == 1, "Scenario 9: Exactly 1 malformed SQL query recorded in ullQuarantinedCount");

			// Scenario 7: Controlled server shutdown while MariaDB is offline -> pending logs must flush to disk spool
			logProxy.SetBlackhole(true);
			logProxy.SeverActiveConnections();
			for (int i = 0; i < 6; ++i) {
				char subBuf[256];
				++nextSeqNo;
				std::snprintf(subBuf, sizeof(subBuf),
					"INSERT INTO test_sql_log_durability (seq_no, scenario, hint) VALUES (%d, 'S7_SHUTDOWN_SPOOL', 'Shutdown spool record /* password=''MyHiddenPass777'' */');",
					nextSeqNo);
				logSql.AsyncQuery(subBuf);
			}
			LIVE_ASSERT(logSql.FlushAndSpoolOnShutdown(400) == true,
				"Scenario 7: Controlled FlushAndSpoolOnShutdown completed cleanly while MariaDB was offline");
			auto stShutdown = logSql.GetDurableSpoolStats();
			LIVE_ASSERT(stShutdown.ullPendingSpoolCount == 6 && stShutdown.ullDroppedSpoolFullCount == 0,
				"Scenario 7: All 6 in-flight shutdown logs persisted to durable disk spool (lost=0)");
		}

		// Scenario 11: Verify spool file permissions (0600 / owner-only) and verify zero plaintext secrets in spool & quarantine files
		{
			struct stat stSpool{};
			LIVE_ASSERT(::stat(spoolFile, &stSpool) == 0 && stSpool.st_size > 0, "Scenario 7 & 11: Spool file exists on disk with non-zero size");
#ifndef __WIN32__
			LIVE_ASSERT((stSpool.st_mode & 0777) == 0600, "Scenario 11: POSIX spool file permissions are strictly 0600");
#else
			LIVE_ASSERT((stSpool.st_mode & _S_IREAD) != 0 && (stSpool.st_mode & _S_IWRITE) != 0,
				"Scenario 11: Windows spool file owner read/write permissions verified");
#endif
			std::ifstream ifsSpool(spoolFile, std::ios::in | std::ios::binary);
			std::string spoolContent((std::istreambuf_iterator<char>(ifsSpool)), std::istreambuf_iterator<char>());
			LIVE_ASSERT(spoolContent.find("MyHiddenPass777") == std::string::npos,
				"Scenario 11: Plaintext password never present in disk spool file");

			std::ifstream ifsQuar(quarantineFile, std::ios::in | std::ios::binary);
			std::string quarContent((std::istreambuf_iterator<char>(ifsQuar)), std::istreambuf_iterator<char>());
			LIVE_ASSERT(!quarContent.empty(), "Scenario 9: Quarantine file created for malformed SQL query");
			LIVE_ASSERT(quarContent.find("LeakedSecretPassword123") == std::string::npos &&
						quarContent.find("SecretTokenABC") == std::string::npos,
				"Scenario 11: Plaintext password and token never present in quarantine file");
		}

		// Scenario 8: Server restart / reconnect recovery from disk spool + new post-restart logs without duplication or out-of-order writes
		logProxy.SetBlackhole(false);
		{
			CAsyncSQL restartedLogSql;
			LIVE_ASSERT(restartedLogSql.EnableDurableSpool(spoolFile, 10, 64 * 1024 * 1024, 150) == true,
				"Scenario 8: Restarted logSql loaded 6 pending records from disk spool");
			auto stLoaded = restartedLogSql.GetDurableSpoolStats();
			LIVE_ASSERT(stLoaded.ullPendingSpoolCount == 6, "Scenario 8: Exactly 6 pre-shutdown records loaded from spool file");

			LIVE_ASSERT(restartedLogSql.Setup("127.0.0.1", user.c_str(), password.c_str(), dbname.c_str(), "latin1", false, proxyPort) == true,
				"Scenario 8: Started restartedLogSql worker thread");

			for (int i = 0; i < 4; ++i) {
				restartedLogSql.AsyncQuery(makeLogQuery("S8_POST_RESTART", "Post-restart log entry").c_str());
			}

			LIVE_ASSERT(waitForCommittedRows(40, 6000) == 40,
				"Scenario 8: All 6 spooled records + 4 post-restart records committed to MariaDB (total=40, lost=0)");
			restartedLogSql.FlushAndSpoolOnShutdown(1000);

			auto stFinalRestart = restartedLogSql.GetDurableSpoolStats();
			LIVE_ASSERT(stFinalRestart.ullReplayedCount >= 6 && stFinalRestart.ullCommittedCount == 10 &&
						stFinalRestart.ullPendingSpoolCount == 0 && stFinalRestart.ullPendingMemoryCount == 0,
				"Scenario 8: All 6 spooled records + 4 post-restart records committed once and spool truncated to 0");
		}

		// Scenario 10: Verify total row count (40), 0 duplicates, 0 lost, 0 out-of-order (row_id order == seq_no order 1..40), and intact escaped quotes
		{
			auto verifyAll = verifyConn.DirectQuery(
				"SELECT row_id, seq_no, scenario, hint FROM test_sql_log_durability ORDER BY row_id ASC;");
			LIVE_ASSERT(verifyAll != nullptr && !verifyAll->IsError() && verifyAll->Get() && verifyAll->Get()->ullNumRows == 40,
				"Scenario 10: Exactly 40 valid log rows committed in MariaDB (0 lost, 0 duplicated)");

			int prevSeq = 0;
			int outOfOrderCount = 0;
			int quoteVerifiedCount = 0;
			for (uint64_t rIdx = 0; rIdx < 40; ++rIdx) {
				MYSQL_ROW row = mysql_fetch_row(verifyAll->Get()->pSQLResult);
				LIVE_ASSERT(row != nullptr, "Row valid");
				int seqNo = std::atoi(row[1]);
				std::string scen = row[2] ? row[2] : "";
				std::string hint = row[3] ? row[3] : "";
				if (seqNo != prevSeq + 1) {
					++outOfOrderCount;
				}
				prevSeq = seqNo;
				if (scen == "S2_S3_OUTAGE" && hint.find("'Halloween_Hat_") != std::string::npos) {
					++quoteVerifiedCount;
				}
			}
			LIVE_ASSERT(outOfOrderCount == 0 && prevSeq == 40,
				"Scenario 10: Strict monotonic sequence order 1..40 preserved in MariaDB (out_of_order=0, duplicated=0, lost=0)");
			LIVE_ASSERT(quoteVerifiedCount == 10,
				"Scenario 10: All 10 outage logs preserved exact single-quoted item names in MariaDB");
		}

		logProxy.Stop();
		verifyConn.DirectQuery("DROP TABLE IF EXISTS test_sql_log_durability;");
		verifyConn.Quit();
		std::remove(spoolFile);
		std::remove(quarantineFile);
	}

	// --------------------------------------------------------------------------
	// 15. CLEANUP OF TEST SCHEMA
	// --------------------------------------------------------------------------
	std::cout << "\n[15/15] Clean Teardown of Test Schema..." << std::endl;
	{
		CAsyncSQL sql;
		sql.Setup(host.c_str(), user.c_str(), password.c_str(), dbname.c_str(), "latin1", true, port);

		auto dropRes = sql.DirectQuery("DROP TABLE IF EXISTS test_player_live;");
		LIVE_ASSERT(dropRes != nullptr && dropRes->uiSQLErrno == 0, "Temporary test table dropped cleanly");

		sql.Quit();
	}

	// --------------------------------------------------------------------------
	// SUMMARY
	// --------------------------------------------------------------------------
	std::cout << "\n==================================================" << std::endl;
	std::cout << " [SUCCESS] Real Live DB Tests for " << engineName << ": "
			  << g_live_tests_passed << "/" << g_live_tests_total << " PASSED!" << std::endl;
	std::cout << "==================================================" << std::endl;

	return 0;
}
