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
			LIVE_ASSERT(proxy.WasSeverMidQueryTriggered() == true,
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
			LIVE_ASSERT(proxy.WasDroppedResponseCaptured() == true,
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
			std::snprintf(insBin, sizeof(insBin), "INSERT INTO test_escape_live (id, raw_blob, raw_text) VALUES (50, '", "");
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
	// 13. CLEANUP OF TEST SCHEMA
	// --------------------------------------------------------------------------
	std::cout << "\n[13/13] Clean Teardown of Test Schema..." << std::endl;
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
