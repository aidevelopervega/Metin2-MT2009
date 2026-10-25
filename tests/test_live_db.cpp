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
// Target database: MariaDB 12.3 LTS (via MariaDB Connector/C / libmariadb).
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
		std::cout << "[SKIP] Real MariaDB 12.3 LTS live integration tests: SKIPPED (RUN_LIVE_DB_TESTS!=1 and no CLI args)" << std::endl;
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
			std::this_thread::sleep_for(std::chrono::milliseconds(100));

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
			std::this_thread::sleep_for(std::chrono::milliseconds(100));

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
			// Category A: 8 Eligible legacy quest rows (IDs 1..8) + 1 Same-fingerprint GM-crafted POINT_RESIST_WARRIOR=59 row (ID 9, protected by human approval gate)
			"(1, 1001, 'INVENTORY', 0, 1, 74020, 604800, 0, 0, 59, 10, 0, 0),"
			"(2, 1001, 'EQUIPMENT', 19, 1, 74270, 1729990000, 1, 0, 60, 10, 0, 0),"
			"(3, 1002, 'INVENTORY', 1, 1, 74520, 604800, 0, 0, 61, 10, 0, 0),"
			"(4, 1002, 'SAFEBOX',   2, 1, 74770, 1729995000, 1, 0, 62, 10, 0, 0),"
			"(5, 1003, 'INVENTORY', 3, 1, 75020, 604800, 0, 0, 63, 10, 0, 0),"
			"(6, 1003, 'EQUIPMENT', 19, 1, 75220, 1729998000, 1, 0, 69, 1, 0, 0),"
			"(7, 1004, 'INVENTORY', 4, 1, 75420, 86400, 0, 0, 70, 1, 0, 0),"
			"(8, 1004, 'MALL',      5, 1, 75620, 1729999000, 1, 0, 63, 10, 0, 0),"
			"(9, 1010, 'INVENTORY', 0, 1, 74020, 604800, 0, 0, 59, 10, 0, 0),"
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
		LIVE_ASSERT(seedRes != nullptr && !seedRes->IsError() && seedRes->Get()->ullAffectedRows == 27,
			"Seeded 27 test item rows across all Halloween/non-Halloween/same-fingerprint-GM categories");

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
			"  ) THEN 1 ELSE 0 END) AS candidate_count, "
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
		LIVE_ASSERT(std::atoi(dryRow[0]) == 9, "Dry-run identified 9 CANDIDATE_REQUIRES_HUMAN_APPROVAL rows (8 quest + 1 same-fingerprint GM item)");
		LIVE_ASSERT(std::atoi(dryRow[1]) == 7, "Dry-run identified 7 ALREADY_VALID_POINT rows");
		LIVE_ASSERT(std::atoi(dryRow[2]) == 4, "Dry-run identified 4 AMBIGUOUS_DO_NOT_CONVERT rows");
		LIVE_ASSERT(std::atoi(dryRow[3]) == 2, "Dry-run identified 2 OTHER_OR_UNSET rows");

		// Step 2: Create Backup Table (with approved_for_migration = 0 default) and Stage Candidate Rows
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
			"  `approved_for_migration` tinyint(1) unsigned NOT NULL DEFAULT 0,"
			"  `backed_up_at` datetime NOT NULL DEFAULT CURRENT_TIMESTAMP,"
			"  PRIMARY KEY (`id`)"
			") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;";
		auto bkCreate = sql.DirectQuery(createBackupSQL);
		LIVE_ASSERT(bkCreate != nullptr && !bkCreate->IsError(), "Created player_item_halloween_rev51_backup table with approved_for_migration=0 default");

		const char * insertBackupSQL =
			"INSERT INTO `player_item_halloween_rev51_backup` ("
			"  `id`, `owner_id`, `window`, `pos`, `count`, `vnum`, "
			"  `socket0`, `socket1`, `socket2`, "
			"  `old_attrtype0`, `old_attrvalue0`, `new_attrtype0`, "
			"  `attrtype1`, `attrvalue1`, `attrtype2`, `attrvalue2`, "
			"  `attrtype3`, `attrvalue3`, `attrtype4`, `attrvalue4`, "
			"  `attrtype5`, `attrvalue5`, `attrtype6`, `attrvalue6`, "
			"  `approved_for_migration`"
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
			"  i.attrtype5, i.attrvalue5, i.attrtype6, i.attrvalue6, "
			"  0 AS approved_for_migration "
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
		LIVE_ASSERT(bkIns != nullptr && !bkIns->IsError() && bkIns->Get()->ullAffectedRows == 9,
			"Staged 9 candidate Halloween hair rows into player_item_halloween_rev51_backup with approved_for_migration=0");

		// Step 3: Verify Unattended Migration Modifies 0 Rows Until Human Approval (`approved_for_migration = 1`)
		const char * migrateSQL =
			"UPDATE `item` i "
			"INNER JOIN `player_item_halloween_rev51_backup` b ON b.id = i.id "
			"SET i.attrtype0 = b.new_attrtype0 "
			"WHERE b.approved_for_migration = 1 "
			"  AND i.vnum IN (74020, 74270, 74520, 74770, 75020, 75220, 75420, 75620) "
			"  AND i.attrtype0 = b.old_attrtype0 "
			"  AND i.attrvalue0 = b.old_attrvalue0 "
			"  AND i.attrtype1 = 0 AND i.attrvalue1 = 0 AND i.attrtype2 = 0 AND i.attrvalue2 = 0 "
			"  AND i.attrtype3 = 0 AND i.attrvalue3 = 0 AND i.attrtype4 = 0 AND i.attrvalue4 = 0 "
			"  AND i.attrtype5 = 0 AND i.attrvalue5 = 0 AND i.attrtype6 = 0 AND i.attrvalue6 = 0 "
			"  AND ((i.socket1 = 0 AND ((i.attrtype0 IN (59, 60, 61, 62, 63) AND i.socket0 = 604800) OR (i.attrtype0 = 69 AND i.socket0 = 21600) OR (i.attrtype0 = 70 AND i.socket0 = 86400))) OR (i.socket1 >= 1 AND i.socket0 > 604800)) "
			"  AND i.socket2 = 0;";
		auto migUnapproved = sql.DirectQuery(migrateSQL);
		LIVE_ASSERT(migUnapproved != nullptr && !migUnapproved->IsError() && migUnapproved->Get()->ullAffectedRows == 0,
			"Unattended execution before human approval modifies 0 rows in player.item");

		// Human operator reviews candidate table against GM logs: approves quest-generated IDs 1..8 and leaves GM-crafted same-fingerprint ID 9 unapproved (0)
		auto approveRes = sql.DirectQuery("UPDATE `player_item_halloween_rev51_backup` SET `approved_for_migration` = 1 WHERE `id` BETWEEN 1 AND 8;");
		LIVE_ASSERT(approveRes != nullptr && !approveRes->IsError() && approveRes->Get()->ullAffectedRows == 8,
			"Human operator approved verified quest-generated item IDs 1..8 (excluding GM-crafted ID 9)");

		sql.DirectQuery("START TRANSACTION;");
		auto migRes = sql.DirectQuery(migrateSQL);
		sql.DirectQuery("COMMIT;");
		LIVE_ASSERT(migRes != nullptr && !migRes->IsError() && migRes->Get()->ullAffectedRows == 8,
			"Migration converted ONLY the 8 human-approved legacy Halloween hair rows in transaction");

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

		// Verify all 11 non-approved rows (including ID 9 same-fingerprint GM-crafted item, normal hair, non-Halloween armor/shield, ambiguous GM items, unset) were untouched!
		auto verifyUntouched = sql.DirectQuery(
			"SELECT COUNT(*) FROM `item` WHERE "
			"(id = 9 AND attrtype0 = 59 AND attrvalue0 = 10) OR "
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
		LIVE_ASSERT(verifyUntouched != nullptr && verifyUntouched->Get(), "Verified non-approved & non-eligible rows");
		MYSQL_ROW untRow = mysql_fetch_row(verifyUntouched->Get()->pSQLResult);
		LIVE_ASSERT(untRow != nullptr && std::atoi(untRow[0]) == 11,
			"All 11 non-approved (including same-fingerprint GM ID 9), non-Halloween, normal-hair, ambiguous GM, and invalid rows remained 100% untouched");

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
			"WHERE b.approved_for_migration = 1 AND i.vnum = b.vnum AND i.attrtype0 = b.new_attrtype0 AND i.attrvalue0 = b.old_attrvalue0;";
		sql.DirectQuery("START TRANSACTION;");
		auto rbRes = sql.DirectQuery(rollbackSQL);
		sql.DirectQuery("COMMIT;");
		LIVE_ASSERT(rbRes != nullptr && !rbRes->IsError() && rbRes->Get()->ullAffectedRows == 8,
			"Rollback restored all 8 approved rows from backup table");

		auto verifyRb = sql.DirectQuery("SELECT id, attrtype0 FROM `item` WHERE id BETWEEN 1 AND 8 ORDER BY id ASC;");
		const int expectedOldTypes[8] = { 59, 60, 61, 62, 63, 69, 70, 63 };
		for (int i = 0; i < 8; ++i) {
			MYSQL_ROW r = mysql_fetch_row(verifyRb->Get()->pSQLResult);
			LIVE_ASSERT(r != nullptr && std::atoi(r[1]) == expectedOldTypes[i],
				("Rolled-back row id=" + std::to_string(i + 1) + " matches original pre-migration attrtype0").c_str());
		}

		// Re-apply migration after rollback and verify only the 1 unapproved GM row (id=9) remains in candidate state and 15 valid EPointTypes rows exist
		auto migRes3 = sql.DirectQuery(migrateSQL);
		LIVE_ASSERT(migRes3 != nullptr && !migRes3->IsError() && migRes3->Get()->ullAffectedRows == 8,
			"Re-applying migration after rollback cleanly re-converted all 8 approved rows");
		auto dryResAfter = sql.DirectQuery(dryRunSQL);
		MYSQL_ROW dryRowAfter = mysql_fetch_row(dryResAfter->Get()->pSQLResult);
		LIVE_ASSERT(dryRowAfter != nullptr && std::atoi(dryRowAfter[0]) == 1 && std::atoi(dryRowAfter[1]) == 15,
			"Post-migration dry-run confirms 1 unapproved GM row (id=9) preserved untouched and 15 valid EPointTypes rows");

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

		// Scenario 12: Live MariaDB replay of SanitizeQueryForLog (backslashes, UTF-8, NO_BACKSLASH_ESCAPES) & active multi-instance spool lock isolation
		{
			const char * slot1File = "test_live_sql_log_spool.1.dat";
			std::remove(spoolFile);
			std::remove(slot1File);
			std::remove("test_live_sql_log_spool.dat.lock");
			std::remove("test_live_sql_log_spool.1.dat.lock");

			logProxy.SetBlackhole(true);
			logProxy.SeverActiveConnections();

			CAsyncSQL instA;
			CAsyncSQL instB;
			LIVE_ASSERT(instA.EnableDurableSpool(spoolFile, 10, 64 * 1024 * 1024, 150) == true,
				"Scenario 12: Instance A acquired primary spool lock in working directory");
			LIVE_ASSERT(instB.EnableDurableSpool(spoolFile, 10, 64 * 1024 * 1024, 150) == true,
				"Scenario 12: Instance B automatically isolated to secondary spool slot in same working directory");
			LIVE_ASSERT(instA.GetSpoolFilePath() == spoolFile && instB.GetSpoolFilePath() == slot1File,
				"Scenario 12: Concurrent instances in same directory isolated to .dat and .1.dat");

			// Instance A: standard backslash-escape mode with path backslashes, escaped quotes, UTF-8, and redacted password
			instA.AsyncQuery(
				"INSERT INTO test_sql_log_durability (seq_no, scenario, hint) VALUES "
				"(41, 'S12_INST_A_STD', 'Path=C:\\\\mt2\\\\logs\\\\ item=\\'Sword\\' UTF8_Şğüıöç password=\\'LiveSecret999\\' ok');");

			// Instance B: NO_BACKSLASH_ESCAPES mode where trailing backslash before quote is literal and inner quotes are doubled
			instB.SetNoBackslashEscapes(true);
			instB.AsyncQuery(
				"INSERT INTO test_sql_log_durability (seq_no, scenario, hint) VALUES "
				"(42, 'S12_INST_B_NOBS', 'Dir=C:\\ token=''LiveToken888'' UTF8_İÖÜ');");

			// instA shuts down and restarts as replayA while instB remains active holding slot 1 lock
			LIVE_ASSERT(instA.FlushAndSpoolOnShutdown(300) == true,
				"Scenario 12: Instance A flushed to slot 0 disk spool on shutdown");

			logProxy.SetBlackhole(false);

			CAsyncSQL replayA;
			LIVE_ASSERT(replayA.EnableDurableSpool(spoolFile, 10, 64 * 1024 * 1024, 150) == true,
				"Scenario 12: Replay instance A loaded slot 0 spool file while instB holds slot 1 lock");
			LIVE_ASSERT(replayA.GetDurableSpoolStats().ullAdoptedOrphanCount == 0,
				"Scenario 12: Active locked slot 1 is preserved for instB and not hijacked by replayA");

			LIVE_ASSERT(replayA.Setup(host.c_str(), user.c_str(), password.c_str(), dbname.c_str(), "utf8mb4", true, port) == true,
				"Scenario 12: Replay instance A connected to live MariaDB");
			LIVE_ASSERT(instB.Setup(host.c_str(), user.c_str(), password.c_str(), dbname.c_str(), "utf8mb4", true, port) == true,
				"Scenario 12: Instance B connected to live MariaDB");
			instB.DirectQuery("SET SESSION sql_mode = CONCAT(@@SESSION.sql_mode, ',NO_BACKSLASH_ESCAPES');");
			instB.SetNoBackslashEscapes(true);

			LIVE_ASSERT(replayA.ReplayDurableSpoolNow() == true && replayA.GetDurableSpoolStats().ullQuarantinedCount == 0,
				"Scenario 12: Instance A replayed sanitized query into MariaDB with 0 SQL syntax errors");
			LIVE_ASSERT(instB.ReplayDurableSpoolNow() == true && instB.GetDurableSpoolStats().ullQuarantinedCount == 0,
				"Scenario 12: Instance B replayed NO_BACKSLASH_ESCAPES sanitized query into MariaDB with 0 SQL syntax errors");

			replayA.FlushAndSpoolOnShutdown(200);
			instB.FlushAndSpoolOnShutdown(200);

			auto verifyS12 = verifyConn.DirectQuery(
				"SELECT seq_no, scenario, hint FROM test_sql_log_durability WHERE seq_no IN (41, 42) ORDER BY seq_no ASC;");
			LIVE_ASSERT(verifyS12 != nullptr && !verifyS12->IsError() && verifyS12->Get() && verifyS12->Get()->ullNumRows == 2,
				"Scenario 12: Both isolated spool records (seq 41 and 42) committed to MariaDB");

			MYSQL_ROW r41 = mysql_fetch_row(verifyS12->Get()->pSQLResult);
			MYSQL_ROW r42 = mysql_fetch_row(verifyS12->Get()->pSQLResult);
			std::string hint41 = (r41 && r41[2]) ? r41[2] : "";
			std::string hint42 = (r42 && r42[2]) ? r42[2] : "";
			LIVE_ASSERT(hint41.find("password='***'") != std::string::npos &&
						hint41.find("LiveSecret999") == std::string::npos &&
						hint41.find("item='Sword'") != std::string::npos,
				"Scenario 12: Replayed row 41 preserved valid SQL syntax, backslashes, quotes, and redacted password='***'");
			LIVE_ASSERT(hint42.find("Dir=C:\\") != std::string::npos &&
						hint42.find("token='***'") != std::string::npos &&
						hint42.find("LiveToken888") == std::string::npos,
				"Scenario 12: Replayed row 42 preserved valid SQL syntax and redacted token under NO_BACKSLASH_ESCAPES");

			std::remove(slot1File);
			std::remove("test_live_sql_log_spool.dat.lock");
			std::remove("test_live_sql_log_spool.1.dat.lock");
		}

		// Scenario 13: Multi-slot crash/shutdown (.dat, .1.dat, .2.dat) followed by SINGLE-instance restart & live MariaDB replay
		{
			const char * slot1File = "test_live_sql_log_spool.1.dat";
			const char * slot2File = "test_live_sql_log_spool.2.dat";
			std::remove(spoolFile);
			std::remove(slot1File);
			std::remove(slot2File);
			std::remove("test_live_sql_log_spool.dat.lock");
			std::remove("test_live_sql_log_spool.1.dat.lock");
			std::remove("test_live_sql_log_spool.2.dat.lock");

			{
				CAsyncSQL p0;
				CAsyncSQL p1;
				CAsyncSQL p2;
				LIVE_ASSERT(p0.EnableDurableSpool(spoolFile, 10, 64 * 1024 * 1024, 150) == true && p0.GetSpoolLockSlot() == 0,
					"Scenario 13: Process 0 acquired slot 0 (.dat)");
				LIVE_ASSERT(p1.EnableDurableSpool(spoolFile, 10, 64 * 1024 * 1024, 150) == true && p1.GetSpoolLockSlot() == 1,
					"Scenario 13: Process 1 acquired slot 1 (.1.dat)");
				LIVE_ASSERT(p2.EnableDurableSpool(spoolFile, 10, 64 * 1024 * 1024, 150) == true && p2.GetSpoolLockSlot() == 2,
					"Scenario 13: Process 2 acquired slot 2 (.2.dat)");

				p0.AsyncQuery("INSERT INTO test_sql_log_durability (seq_no, scenario, hint) VALUES (43, 'S13_SLOT0', 'Slot0_Rec1');");
				p0.AsyncQuery("INSERT INTO test_sql_log_durability (seq_no, scenario, hint) VALUES (44, 'S13_SLOT0', 'Slot0_Rec2');");
				p1.AsyncQuery("INSERT INTO test_sql_log_durability (seq_no, scenario, hint) VALUES (45, 'S13_SLOT1', 'Slot1_Rec1');");
				p1.AsyncQuery("INSERT INTO test_sql_log_durability (seq_no, scenario, hint) VALUES (46, 'S13_SLOT1', 'Slot1_Rec2');");
				p2.AsyncQuery("INSERT INTO test_sql_log_durability (seq_no, scenario, hint) VALUES (47, 'S13_SLOT2', 'Slot2_Rec1');");

				LIVE_ASSERT(p0.FlushAndSpoolOnShutdown(300) == true &&
							p1.FlushAndSpoolOnShutdown(300) == true &&
							p2.FlushAndSpoolOnShutdown(300) == true,
					"Scenario 13: All 3 processes flushed offline spool files (.dat, .1.dat, .2.dat) and exited");
			}

			// Now ONLY ONE instance restarts in the working directory
			CAsyncSQL singleSurvivor;
			LIVE_ASSERT(singleSurvivor.EnableDurableSpool(spoolFile, 10, 64 * 1024 * 1024, 150) == true,
				"Scenario 13: Single survivor process restarted on slot 0");
			auto stAdopted = singleSurvivor.GetDurableSpoolStats();
			LIVE_ASSERT(stAdopted.ullAdoptedOrphanCount == 3 && stAdopted.ullPendingSpoolCount == 5,
				"Scenario 13: Single survivor automatically adopted 3 orphaned records from .1.dat and .2.dat (total pending=5)");

			LIVE_ASSERT(singleSurvivor.Setup(host.c_str(), user.c_str(), password.c_str(), dbname.c_str(), "utf8mb4", true, port) == true,
				"Scenario 13: Single survivor connected to live MariaDB");
			LIVE_ASSERT(singleSurvivor.ReplayDurableSpoolNow() == true,
				"Scenario 13: Single survivor replayed all 5 records (slot 0 + orphan slots 1 & 2) into MariaDB");
			singleSurvivor.FlushAndSpoolOnShutdown(200);

			auto verifyS13 = verifyConn.DirectQuery(
				"SELECT seq_no, scenario, hint FROM test_sql_log_durability WHERE seq_no BETWEEN 43 AND 47 ORDER BY row_id ASC;");
			LIVE_ASSERT(verifyS13 != nullptr && !verifyS13->IsError() && verifyS13->Get() && verifyS13->Get()->ullNumRows == 5,
				"Scenario 13: All 5 rows (seq 43..47) across slot 0, slot 1, and slot 2 committed to MariaDB with 0 loss and 0 duplicates");

			int expectedSeq = 43;
			bool orderMatch = true;
			for (uint64_t i = 0; i < 5; ++i) {
				MYSQL_ROW r = mysql_fetch_row(verifyS13->Get()->pSQLResult);
				if (!r || std::atoi(r[0]) != expectedSeq++) {
					orderMatch = false;
				}
			}
			LIVE_ASSERT(orderMatch == true,
				"Scenario 13: Replayed rows from slot 0 (.dat), slot 1 (.1.dat), and slot 2 (.2.dat) preserved exact sequence order 43..47");

			std::remove(slot1File);
			std::remove(slot2File);
			std::remove("test_live_sql_log_spool.dat.lock");
			std::remove("test_live_sql_log_spool.1.dat.lock");
			std::remove("test_live_sql_log_spool.2.dat.lock");
		}

		logProxy.Stop();
		verifyConn.DirectQuery("DROP TABLE IF EXISTS test_sql_log_durability;");
		verifyConn.Quit();
		std::remove(spoolFile);
		std::remove(quarantineFile);
	}

	// --------------------------------------------------------------------------
	// 15. MARIADB 12.3 LTS LOOPBACK BIND, LEAST-PRIVILEGE & SERVER AUDIT SUITE
	// --------------------------------------------------------------------------
	std::cout << "\n[15/16] Testing MariaDB 12.3 LTS Loopback Bind, Least-Privilege Runtime/Log Accounts & Server Audit..." << std::endl;
	{
		CAsyncSQL adminSql;
		LIVE_ASSERT(adminSql.Setup(host.c_str(), user.c_str(), password.c_str(), dbname.c_str(), "utf8mb4", true, port) == true,
			"Security Suite: Connected to MariaDB 12.3 LTS for security posture inspection");

		// 1. Verify bind_address == 127.0.0.1 and local_infile == 0
		auto bindMsg = adminSql.DirectQuery("SELECT @@GLOBAL.bind_address, @@GLOBAL.local_infile;");
		LIVE_ASSERT(bindMsg != nullptr && !bindMsg->IsError() && bindMsg->Get() && bindMsg->Get()->uiNumRows == 1,
			"Security Suite: Queried @@GLOBAL.bind_address and @@GLOBAL.local_infile");
		if (bindMsg && !bindMsg->IsError() && bindMsg->Get() && bindMsg->Get()->uiNumRows == 1) {
			MYSQL_ROW bRow = mysql_fetch_row(bindMsg->Get()->pSQLResult);
			std::string bindAddr = (bRow && bRow[0]) ? bRow[0] : "";
			int localInfile = (bRow && bRow[1]) ? std::atoi(bRow[1]) : -1;
			LIVE_ASSERT(bindAddr == "127.0.0.1",
				"Security Suite [B2/B5.10]: @@GLOBAL.bind_address is strictly locked to 127.0.0.1 (not 0.0.0.0 or *)");
			LIVE_ASSERT(localInfile == 0,
				"Security Suite [B2]: @@GLOBAL.local_infile is disabled (0)");
		}

		// 2. Verify unauthorized user / wrong password connection is rejected (ER_ACCESS_DENIED_ERROR = 1045)
		{
			CAsyncSQL unauthSql;
			bool unauthOk = unauthSql.Setup(host.c_str(), "ransom_attacker_unauth", "WrongPass!999", dbname.c_str(), "utf8mb4", true, port);
			LIVE_ASSERT(unauthOk == false,
				"Security Suite [B5.2]: Unauthorized database user connection is strictly rejected (ER_ACCESS_DENIED_ERROR)");
			unauthSql.Quit();
		}

		// 3. Verify least-privilege runtime account (mt2server) and dedicated log account (mt2log) if configured
		const char * envRuntimeUser = std::getenv("MT2_RUNTIME_USER");
		const char * envRuntimePass = std::getenv("MT2_RUNTIME_PASSWORD");
		const char * envLogUser = std::getenv("MT2_LOG_DB_USER");
		const char * envLogPass = std::getenv("MT2_LOG_DB_PASS");

		if (envRuntimeUser && *envRuntimeUser && envRuntimePass && *envRuntimePass) {
			CAsyncSQL rtSql;
			LIVE_ASSERT(rtSql.Setup(host.c_str(), envRuntimeUser, envRuntimePass, dbname.c_str(), "utf8mb4", true, port) == true,
				"Security Suite [B5.1]: Least-privilege runtime account (mt2server) connected via 127.0.0.1");

			// B5.3 & B5.4: Verify mt2server has NO global admin privileges (no SUPER, FILE, CREATE USER, GRANT OPTION, SHUTDOWN, PROCESS, RELOAD)
			auto privMsg = rtSql.DirectQuery(
				"SELECT COUNT(*) FROM information_schema.USER_PRIVILEGES "
				"WHERE PRIVILEGE_TYPE IN ('SUPER','FILE','CREATE USER','SHUTDOWN','PROCESS','RELOAD','ALL PRIVILEGES') "
				"OR IS_GRANTABLE = 'YES';");
			LIVE_ASSERT(privMsg != nullptr && !privMsg->IsError() && privMsg->Get() && privMsg->Get()->uiNumRows == 1,
				"Security Suite [B5.3]: Queried information_schema.USER_PRIVILEGES for runtime account");
			if (privMsg && !privMsg->IsError() && privMsg->Get() && privMsg->Get()->uiNumRows == 1) {
				MYSQL_ROW pRow = mysql_fetch_row(privMsg->Get()->pSQLResult);
				int adminPrivCount = (pRow && pRow[0]) ? std::atoi(pRow[0]) : -1;
				LIVE_ASSERT(adminPrivCount == 0,
					"Security Suite [B5.3/B5.4]: Runtime account possesses 0 global admin/super/file/create-user/grant privileges");
			}

			// B5.4: Attempt DCL / admin commands as runtime account -> must be denied
			auto dclCreateUser = rtSql.DirectQuery("CREATE USER 'ransom_backdoor'@'%' IDENTIFIED BY 'Pwned!123';");
			LIVE_ASSERT(dclCreateUser != nullptr && dclCreateUser->IsError() && dclCreateUser->uiSQLErrno != 0,
				"Security Suite [B5.4]: Runtime account cannot execute CREATE USER (rejected by MariaDB)");

			auto dclGrant = rtSql.DirectQuery("GRANT ALL PRIVILEGES ON *.* TO 'mt2server'@'127.0.0.1';");
			LIVE_ASSERT(dclGrant != nullptr && dclGrant->IsError() && dclGrant->uiSQLErrno != 0,
				"Security Suite [B5.4]: Runtime account cannot escalate privileges via GRANT");

			// B5.5: Attempt destructive DDL (CREATE DATABASE, DROP DATABASE, DROP TABLE, ALTER TABLE) as runtime account -> must be denied
			auto ddlCreateDb = rtSql.DirectQuery("CREATE DATABASE z_ransom_note_test;");
			LIVE_ASSERT(ddlCreateDb != nullptr && ddlCreateDb->IsError() && (ddlCreateDb->uiSQLErrno == 1044 || ddlCreateDb->uiSQLErrno == 1227),
				"Security Suite [B5.5]: Runtime account cannot execute CREATE DATABASE (errno 1044)");

			auto ddlDropDb = rtSql.DirectQuery("DROP DATABASE mt2_testdb;");
			LIVE_ASSERT(ddlDropDb != nullptr && ddlDropDb->IsError() && (ddlDropDb->uiSQLErrno == 1044 || ddlDropDb->uiSQLErrno == 1142),
				"Security Suite [B5.5]: Runtime account cannot execute DROP DATABASE (errno 1044)");

			auto ddlDropTbl = rtSql.DirectQuery("DROP TABLE test_player_live;");
			LIVE_ASSERT(ddlDropTbl != nullptr && ddlDropTbl->IsError() && ddlDropTbl->uiSQLErrno == 1142,
				"Security Suite [B5.5]: Runtime account cannot execute DROP TABLE (errno 1142)");

			auto ddlAlterTbl = rtSql.DirectQuery("ALTER TABLE test_player_live ADD COLUMN ransom_col INT DEFAULT 0;");
			LIVE_ASSERT(ddlAlterTbl != nullptr && ddlAlterTbl->IsError() && ddlAlterTbl->uiSQLErrno == 1142,
				"Security Suite [B5.5]: Runtime account cannot execute ALTER TABLE (errno 1142)");

			// B5.6: Verify legitimate GAME/DB runtime queries (SELECT, INSERT, UPDATE, REPLACE, DELETE, SHOW CREATE TABLE) succeed
			auto dmlSelect = rtSql.DirectQuery("SELECT id, name, gold FROM test_player_live LIMIT 1;");
			LIVE_ASSERT(dmlSelect != nullptr && !dmlSelect->IsError(),
				"Security Suite [B5.6]: Runtime account SELECT on player table succeeds");

			auto dmlShowCreate = rtSql.DirectQuery("SHOW CREATE TABLE test_player_live;");
			LIVE_ASSERT(dmlShowCreate != nullptr && !dmlShowCreate->IsError(),
				"Security Suite [B5.6]: Runtime account SHOW CREATE TABLE (used by PlayerHB::Initialize) succeeds");

			auto dmlReplace = rtSql.DirectQuery(
				"REPLACE INTO test_player_live (id, name, gold, created_date) "
				"VALUES (99901, 'RtLeastPriv', 10000, '2026-10-24 12:00:00');");
			LIVE_ASSERT(dmlReplace != nullptr && !dmlReplace->IsError(),
				"Security Suite [B5.6]: Runtime account REPLACE INTO (INSERT+DELETE) succeeds");

			auto dmlUpdate = rtSql.DirectQuery("UPDATE test_player_live SET gold = 56000 WHERE id = 99901;");
			LIVE_ASSERT(dmlUpdate != nullptr && !dmlUpdate->IsError(),
				"Security Suite [B5.6]: Runtime account UPDATE succeeds");

			auto dmlDelete = rtSql.DirectQuery("DELETE FROM test_player_live WHERE id = 99901;");
			LIVE_ASSERT(dmlDelete != nullptr && !dmlDelete->IsError(),
				"Security Suite [B5.6]: Runtime account DELETE succeeds");

			// Section 1 Narrowed Privileges: Verify mt2server has 0 access to log.* and SELECT-only on world.*
			adminSql.DirectQuery("CREATE TABLE IF NOT EXISTS world.test_world_proto (vnum INT PRIMARY KEY, name VARCHAR(32));");
			adminSql.DirectQuery("REPLACE INTO world.test_world_proto VALUES (10, 'Sword+0');");
			adminSql.DirectQuery("CREATE TABLE IF NOT EXISTS log.test_log_isolation (id INT PRIMARY KEY, msg VARCHAR(64));");
			adminSql.DirectQuery("REPLACE INTO log.test_log_isolation VALUES (1, 'boot');");

			auto rtSelectLog = rtSql.DirectQuery("SELECT * FROM log.test_log_isolation LIMIT 1;");
			LIVE_ASSERT(rtSelectLog != nullptr && rtSelectLog->IsError() && (rtSelectLog->uiSQLErrno == 1142 || rtSelectLog->uiSQLErrno == 1044),
				"Security Suite [Sec1]: Runtime account (mt2server) has 0 SELECT access to log.* (errno 1142/1044)");

			auto rtDeleteLog = rtSql.DirectQuery("DELETE FROM log.test_log_isolation WHERE id = 1;");
			LIVE_ASSERT(rtDeleteLog != nullptr && rtDeleteLog->IsError() && (rtDeleteLog->uiSQLErrno == 1142 || rtDeleteLog->uiSQLErrno == 1044),
				"Security Suite [Sec1]: Runtime account (mt2server) cannot DELETE/erase forensic records in log.* (errno 1142/1044)");

			auto rtSelectWorld = rtSql.DirectQuery("SELECT vnum, name FROM world.test_world_proto WHERE vnum = 10;");
			LIVE_ASSERT(rtSelectWorld != nullptr && !rtSelectWorld->IsError() && rtSelectWorld->Get() && rtSelectWorld->Get()->uiNumRows == 1,
				"Security Suite [Sec1]: Runtime account (mt2server) SELECT on world.* succeeds (PROTO_FROM_DB=1)");

			auto rtUpdateWorld = rtSql.DirectQuery("UPDATE world.test_world_proto SET name = 'Hacked' WHERE vnum = 10;");
			LIVE_ASSERT(rtUpdateWorld != nullptr && rtUpdateWorld->IsError() && rtUpdateWorld->uiSQLErrno == 1142,
				"Security Suite [Sec1]: Runtime account (mt2server) is strictly denied UPDATE on world.* (errno 1142)");

			auto rtDeleteWorld = rtSql.DirectQuery("DELETE FROM world.test_world_proto WHERE vnum = 10;");
			LIVE_ASSERT(rtDeleteWorld != nullptr && rtDeleteWorld->IsError() && rtDeleteWorld->uiSQLErrno == 1142,
				"Security Suite [Sec1]: Runtime account (mt2server) is strictly denied DELETE on world.* (errno 1142)");

			adminSql.DirectQuery("DROP TABLE IF EXISTS world.test_world_proto;");
			adminSql.DirectQuery("DROP TABLE IF EXISTS log.test_log_isolation;");

			rtSql.Quit();
		}

		if (envLogUser && *envLogUser && envLogPass && *envLogPass) {
			CAsyncSQL logRoleSql;
			LIVE_ASSERT(logRoleSql.Setup(host.c_str(), envLogUser, envLogPass, "log", "utf8mb4", true, port) == true,
				"Security Suite [B3]: Dedicated SQL Log account (mt2log) connected to 'log' database");

			// Verify mt2log CANNOT read or modify player/test tables in mt2_testdb or account
			auto logReadPlayer = logRoleSql.DirectQuery("SELECT * FROM mt2_testdb.test_player_live LIMIT 1;");
			LIVE_ASSERT(logReadPlayer != nullptr && logReadPlayer->IsError() && (logReadPlayer->uiSQLErrno == 1142 || logReadPlayer->uiSQLErrno == 1044),
				"Security Suite [B3]: Dedicated SQL Log account (mt2log) is strictly denied SELECT access to non-log schemas (errno 1142/1044)");

			auto logDropTable = logRoleSql.DirectQuery("DROP TABLE mt2_testdb.test_player_live;");
			LIVE_ASSERT(logDropTable != nullptr && logDropTable->IsError() && (logDropTable->uiSQLErrno == 1142 || logDropTable->uiSQLErrno == 1044),
				"Security Suite [B3]: Dedicated SQL Log account (mt2log) is strictly denied DDL across schemas");

			logRoleSql.Quit();
		}

		const char * envBackupUser = std::getenv("MT2_BACKUP_DB_USER");
		const char * envBackupPass = std::getenv("MT2_BACKUP_DB_PASS");
		if (envBackupUser && *envBackupUser && envBackupPass && *envBackupPass) {
			CAsyncSQL bkSql;
			LIVE_ASSERT(bkSql.Setup(host.c_str(), envBackupUser, envBackupPass, dbname.c_str(), "utf8mb4", true, port) == true,
				"Security Suite [Sec4]: Dedicated read-only backup account (mt2backup) connected via 127.0.0.1");

			auto bkSelect = bkSql.DirectQuery("SELECT id, name, gold FROM test_player_live LIMIT 1;");
			LIVE_ASSERT(bkSelect != nullptr && !bkSelect->IsError(),
				"Security Suite [Sec4]: Read-only backup account (mt2backup) SELECT succeeds");

			auto bkUpdate = bkSql.DirectQuery("UPDATE test_player_live SET gold = 0;");
			LIVE_ASSERT(bkUpdate != nullptr && bkUpdate->IsError() && bkUpdate->uiSQLErrno == 1142,
				"Security Suite [Sec4]: Read-only backup account (mt2backup) is strictly denied UPDATE (errno 1142)");

			auto bkDelete = bkSql.DirectQuery("DELETE FROM test_player_live;");
			LIVE_ASSERT(bkDelete != nullptr && bkDelete->IsError() && bkDelete->uiSQLErrno == 1142,
				"Security Suite [Sec4]: Read-only backup account (mt2backup) is strictly denied DELETE (errno 1142)");

			auto bkDrop = bkSql.DirectQuery("DROP TABLE test_player_live;");
			LIVE_ASSERT(bkDrop != nullptr && bkDrop->IsError() && bkDrop->uiSQLErrno == 1142,
				"Security Suite [Sec4]: Read-only backup account (mt2backup) is strictly denied DROP TABLE (errno 1142)");

			bkSql.Quit();
		}

		// B4, B5.7 & Sec2/Sec3: Check server_audit plugin status, QUERY_DML_NO_SELECT scope, and 100MBx90 retention
		auto auditVarMsg = adminSql.DirectQuery("SHOW GLOBAL VARIABLES LIKE 'server_audit_logging';");
		if (auditVarMsg && !auditVarMsg->IsError() && auditVarMsg->Get() && auditVarMsg->Get()->uiNumRows == 1) {
			MYSQL_ROW aRow = mysql_fetch_row(auditVarMsg->Get()->pSQLResult);
			std::string auditLogging = (aRow && aRow[1]) ? aRow[1] : "";
			LIVE_ASSERT(auditLogging == "ON",
				"Security Suite [B4/B5.7]: MariaDB server_audit plugin is active with server_audit_logging=ON");

			auto auditEventsMsg = adminSql.DirectQuery(
				"SELECT @@GLOBAL.server_audit_events, @@GLOBAL.server_audit_file_rotate_size, @@GLOBAL.server_audit_file_rotations;");
			if (auditEventsMsg && !auditEventsMsg->IsError() && auditEventsMsg->Get() && auditEventsMsg->Get()->uiNumRows == 1) {
				MYSQL_ROW eRow = mysql_fetch_row(auditEventsMsg->Get()->pSQLResult);
				std::string evs = (eRow && eRow[0]) ? eRow[0] : "";
				long long rotSize = (eRow && eRow[1]) ? std::strtoll(eRow[1], nullptr, 10) : 0;
				int rotCount = (eRow && eRow[2]) ? std::atoi(eRow[2]) : 0;
				LIVE_ASSERT(evs.find("CONNECT") != std::string::npos &&
							evs.find("QUERY_DDL") != std::string::npos &&
							evs.find("QUERY_DCL") != std::string::npos &&
							evs.find("QUERY_DML_NO_SELECT") != std::string::npos,
					"Security Suite [Sec2]: server_audit_events tracks CONNECT, QUERY_DDL, QUERY_DCL, and QUERY_DML_NO_SELECT");
				LIVE_ASSERT(rotSize >= 104857600LL && rotCount >= 90,
					"Security Suite [Sec3]: server_audit rotation configured for >= 100 MB x 90 files (~9 GB retention)");
			}

			// Verify FORCE_PLUS_PERMANENT prevents unloading server_audit at runtime
			auto uninstallAudit = adminSql.DirectQuery("UNINSTALL PLUGIN server_audit;");
			LIVE_ASSERT(uninstallAudit != nullptr && uninstallAudit->IsError() && uninstallAudit->uiSQLErrno != 0,
				"Security Suite [B4]: server_audit is protected by FORCE_PLUS_PERMANENT and cannot be uninstalled at runtime");
		}

		adminSql.Quit();
	}

	// --------------------------------------------------------------------------
	// 16. IN-GAME SQL BUG FIXES VERIFICATION (IKARUS OFFLINE SHOP, ITEMSHOP, MONARCH)
	// --------------------------------------------------------------------------
	std::cout << "\n[16/17] Verifying In-Game SQL Fixes (ikarusshop_log, itemshop + Durable Spool 0 Quarantine, monarch)..." << std::endl;
	{
		CAsyncSQL logSql;
		if (logSql.Setup(host.c_str(), user.c_str(), password.c_str(), "log", "utf8mb4", true, port)) {
			bool bBootstrappedLog = false;
			auto preLogCheck = logSql.DirectQuery(
				"SELECT COUNT(*) FROM information_schema.TABLES "
				"WHERE TABLE_SCHEMA='log' AND TABLE_NAME IN ('ikarusshop_log','itemshop','itemshop_log');");
			if (preLogCheck && !preLogCheck->IsError() && preLogCheck->Get() && preLogCheck->Get()->uiNumRows == 1) {
				MYSQL_ROW pRow = mysql_fetch_row(preLogCheck->Get()->pSQLResult);
				int existingCnt = (pRow && pRow[0]) ? std::atoi(pRow[0]) : 0;
				if (existingCnt == 0) {
					bBootstrappedLog = true;
					logSql.DirectQuery(
						"CREATE TABLE IF NOT EXISTS `ikarusshop_log` ("
						"  `id` int(11) unsigned NOT NULL AUTO_INCREMENT,"
						"  `who` int(11) unsigned NOT NULL DEFAULT 0,"
						"  `itemid` int(11) unsigned NOT NULL DEFAULT 0,"
						"  `what` varchar(64) NOT NULL DEFAULT '',"
						"  `shop_owner` int(11) unsigned NOT NULL DEFAULT 0,"
						"  `extra` mediumtext NOT NULL,"
						"  `vnum` int(11) unsigned NOT NULL DEFAULT 0,"
						"  `count` int(11) unsigned NOT NULL DEFAULT 0,"
						"  `yang` bigint(20) NOT NULL DEFAULT 0,"
						"  `cheque` int(11) NOT NULL DEFAULT 0,"
						"  `time` datetime NOT NULL DEFAULT current_timestamp(),"
						"  PRIMARY KEY (`id`),"
						"  KEY `idx_ikashop_who` (`who`),"
						"  KEY `idx_ikashop_owner` (`shop_owner`),"
						"  KEY `idx_ikashop_itemid` (`itemid`),"
						"  KEY `idx_ikashop_time` (`time`)"
						") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci;");
					logSql.DirectQuery(
						"CREATE TABLE IF NOT EXISTS `itemshop` ("
						"  `id` int(11) unsigned NOT NULL AUTO_INCREMENT,"
						"  `pid` int(11) unsigned NOT NULL DEFAULT 0,"
						"  `account_id` int(11) unsigned NOT NULL DEFAULT 0,"
						"  `item_index` int(11) NOT NULL DEFAULT 0,"
						"  `vnum` int(11) unsigned NOT NULL DEFAULT 0,"
						"  `count` int(11) NOT NULL DEFAULT 0,"
						"  `price` bigint(20) NOT NULL DEFAULT 0,"
						"  `currency` tinyint(4) NOT NULL DEFAULT 0,"
						"  `item_id` int(11) unsigned DEFAULT NULL,"
						"  `date` datetime NOT NULL DEFAULT current_timestamp(),"
						"  `money_before_buy` int(11) unsigned NOT NULL DEFAULT 0,"
						"  PRIMARY KEY (`id`),"
						"  KEY `idx_itemshop_pid` (`pid`),"
						"  KEY `idx_itemshop_account` (`account_id`),"
						"  KEY `idx_itemshop_date` (`date`)"
						") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci;");
					logSql.DirectQuery(
						"CREATE TABLE IF NOT EXISTS `itemshop_log` ("
						"  `account_id` int(11) DEFAULT NULL,"
						"  `player_id` int(11) DEFAULT NULL,"
						"  `player_name` char(50) DEFAULT NULL,"
						"  `vnum` int(11) DEFAULT NULL,"
						"  `count` int(11) DEFAULT NULL,"
						"  `price` int(11) DEFAULT NULL,"
						"  `ip` varchar(50) DEFAULT NULL,"
						"  `date` datetime DEFAULT NULL"
						") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci;");
				}
			}

			// Verify ikarusshop_log and itemshop exist in `log` schema with ENGINE=InnoDB
			auto tblCheck = logSql.DirectQuery(
				"SELECT TABLE_NAME, ENGINE FROM information_schema.TABLES "
				"WHERE TABLE_SCHEMA='log' AND TABLE_NAME IN ('ikarusshop_log','itemshop','itemshop_log') "
				"ORDER BY TABLE_NAME;");
			LIVE_ASSERT(tblCheck != nullptr && !tblCheck->IsError() && tblCheck->Get() && tblCheck->Get()->uiNumRows == 3,
				"In-Game SQL [Bug 1 & 2]: log.ikarusshop_log, log.itemshop, and legacy log.itemshop_log all exist");
			if (tblCheck && tblCheck->Get()) {
				MYSQL_ROW tRow;
				while ((tRow = mysql_fetch_row(tblCheck->Get()->pSQLResult)) != nullptr) {
					std::string tName = tRow[0] ? tRow[0] : "";
					std::string tEng = tRow[1] ? tRow[1] : "";
					LIVE_ASSERT(tEng == "InnoDB",
						("In-Game SQL [Bug 1 & 2]: log." + tName + " uses ENGINE=InnoDB").c_str());
				}
			}

			// Bug 1: Test ikarusshop_log inserts with single quotes, backslashes, UTF-8, and 2500-char extra
			logSql.DirectQuery("DELETE FROM ikarusshop_log WHERE who IN (991001, 991002);");

			std::string rawExtra = "Pazar: 'Dolunay Kılıcı+9' \\ Özel Not: ŞĞÜÖÇİ — ";
			rawExtra.append(2450, 'X');
			std::vector<char> escExtra(rawExtra.size() * 2 + 1, '\0');
			size_t escLen = logSql.EscapeString(escExtra.data(), escExtra.size(), rawExtra.c_str(), rawExtra.size());
			LIVE_ASSERT(escLen > rawExtra.size(),
				"In-Game SQL [Bug 1]: CAsyncSQL::EscapeString escaped quotes/backslashes/UTF-8 in 2500+ byte extra");

			std::string qCreate = "INSERT INTO `ikarusshop_log` (`who`, `itemid`, `what`, `shop_owner`, `extra`, `vnum`, `count`, `yang`, `cheque`) "
								  "VALUES(991001, 0, 'CREATE_SHOP', 991001, '" + std::string(escExtra.data()) + "', 0, 0, 0, 0);";
			auto rCreate = logSql.DirectQuery(qCreate.c_str());
			LIVE_ASSERT(rCreate != nullptr && !rCreate->IsError() && rCreate->uiSQLErrno == 0,
				"In-Game SQL [Bug 1]: ikarusshop_log CREATE_SHOP with quotes, backslashes, UTF-8 & 2500+ byte extra succeeded");

			auto rAdd = logSql.DirectQuery(
				"INSERT INTO `ikarusshop_log` (`who`, `itemid`, `what`, `shop_owner`, `extra`, `vnum`, `count`, `yang`, `cheque`) "
				"VALUES(991001, 550001, 'ADD_ITEM', 991001, 'pos:0', 299, 1, 150000000, 10);");
			LIVE_ASSERT(rAdd != nullptr && !rAdd->IsError() && rAdd->uiSQLErrno == 0,
				"In-Game SQL [Bug 1]: ikarusshop_log ADD_ITEM succeeded");

			auto rBuy = logSql.DirectQuery(
				"INSERT INTO `ikarusshop_log` (`who`, `itemid`, `what`, `shop_owner`, `extra`, `vnum`, `count`, `yang`, `cheque`) "
				"VALUES(991002, 550001, 'BUY_ITEM', 991001, 'buyer:991002', 299, 1, 150000000, 10);");
			LIVE_ASSERT(rBuy != nullptr && !rBuy->IsError() && rBuy->uiSQLErrno == 0,
				"In-Game SQL [Bug 1]: ikarusshop_log BUY_ITEM succeeded");

			auto rVerifyIka = logSql.DirectQuery(
				"SELECT extra FROM ikarusshop_log WHERE who=991001 AND what='CREATE_SHOP' LIMIT 1;");
			LIVE_ASSERT(rVerifyIka != nullptr && !rVerifyIka->IsError() && rVerifyIka->Get() && rVerifyIka->Get()->uiNumRows == 1,
				"In-Game SQL [Bug 1]: ikarusshop_log CREATE_SHOP row fetched back");
			if (rVerifyIka && rVerifyIka->Get() && rVerifyIka->Get()->uiNumRows == 1) {
				MYSQL_ROW iRow = mysql_fetch_row(rVerifyIka->Get()->pSQLResult);
				std::string fetchedExtra = (iRow && iRow[0]) ? iRow[0] : "";
				LIVE_ASSERT(fetchedExtra == rawExtra,
					"In-Game SQL [Bug 1]: ikarusshop_log extra round-tripped byte-for-byte intact (quotes, backslashes, UTF-8, 2500+ chars)");
			}
			logSql.DirectQuery("DELETE FROM ikarusshop_log WHERE who IN (991001, 991002);");

			// Bug 2: Test itemshop 10-column insert (normal item ID, VIP/award NULL item_id, and large 64-bit price > INT32_MAX via %lld) + verify 0 quarantine in durable spool
			logSql.DirectQuery("DELETE FROM itemshop WHERE pid = 991001;");
			const std::string itemshopSpoolPath = "./mt2_test_itemshop_spool_" + std::to_string(port) + ".jsonl";
			std::remove(itemshopSpoolPath.c_str());
			std::remove((itemshopSpoolPath + ".quarantine").c_str());

			const uint32_t largeUnitPrice = 300000000U;
			const int largeQty = 10;
			const long long llLargePrice = static_cast<long long>(largeUnitPrice) * static_cast<long long>(largeQty); // 3,000,000,000 > INT32_MAX (2,147,483,647)
			char szLargePriceQuery[512];
			std::snprintf(szLargePriceQuery, sizeof(szLargePriceQuery),
				"INSERT INTO itemshop (pid, account_id, item_index, vnum, count, price, currency, item_id, date, money_before_buy) "
				"VALUES (%u, %u, %d, %u, %d, %lld, %d, %s, NOW(), %u)",
				991001U, 990501U, 18, 80017U, largeQty, llLargePrice, 1, "8800999", 4000000000U);

			{
				CAsyncSQL asyncLogSql;
				LIVE_ASSERT(asyncLogSql.EnableDurableSpool(itemshopSpoolPath.c_str()) == true,
					"In-Game SQL [Bug 2]: Enabled durable spool for itemshop log verification");
				LIVE_ASSERT(asyncLogSql.Setup(host.c_str(), user.c_str(), password.c_str(), "log", "utf8mb4", false, port) == true,
					"In-Game SQL [Bug 2]: Started threaded CAsyncSQL worker for log.itemshop");

				asyncLogSql.AsyncQuery(
					"INSERT INTO itemshop (pid, account_id, item_index, vnum, count, price, currency, item_id, date, money_before_buy) "
					"VALUES (991001, 990501, 12, 71084, 5, 250, 1, 8800123, NOW(), 1000)");
				asyncLogSql.AsyncQuery(
					"INSERT INTO itemshop (pid, account_id, item_index, vnum, count, price, currency, item_id, date, money_before_buy) "
					"VALUES (991001, 990501, 15, 70021, 1, 500, 2, null, NOW(), 750)");
				asyncLogSql.AsyncQuery(szLargePriceQuery);

				for (int w = 0; w < 60; ++w) {
					if (asyncLogSql.CountQueryFinished() >= 3 && asyncLogSql.CountQuery() == 0)
						break;
					std::this_thread::sleep_for(std::chrono::milliseconds(50));
				}
				std::this_thread::sleep_for(std::chrono::milliseconds(100));

				auto spoolStats = asyncLogSql.GetDurableSpoolStats();
				LIVE_ASSERT(spoolStats.ullQuarantinedCount == 0 && spoolStats.ullCommittedCount >= 3,
					"In-Game SQL [Bug 2]: CItemShopManager::BuyItem 10-column itemshop log (including >INT32_MAX 64-bit price) committed cleanly with 0 quarantined statements");
				asyncLogSql.Quit();
			}

			auto rVerifyShop = logSql.DirectQuery(
				"SELECT COUNT(*), SUM(price), MAX(price), SUM(CASE WHEN item_id IS NULL THEN 1 ELSE 0 END) "
				"FROM itemshop WHERE pid = 991001;");
			LIVE_ASSERT(rVerifyShop != nullptr && !rVerifyShop->IsError() && rVerifyShop->Get() && rVerifyShop->Get()->uiNumRows == 1,
				"In-Game SQL [Bug 2]: SELECT from log.itemshop succeeded");
			if (rVerifyShop && rVerifyShop->Get() && rVerifyShop->Get()->uiNumRows == 1) {
				MYSQL_ROW sRow = mysql_fetch_row(rVerifyShop->Get()->pSQLResult);
				int cnt = (sRow && sRow[0]) ? std::atoi(sRow[0]) : 0;
				long long sumPrice = (sRow && sRow[1]) ? std::strtoll(sRow[1], nullptr, 10) : 0;
				long long maxPrice = (sRow && sRow[2]) ? std::strtoll(sRow[2], nullptr, 10) : 0;
				int nullItemIds = (sRow && sRow[3]) ? std::atoi(sRow[3]) : 0;
				LIVE_ASSERT(cnt == 3 && sumPrice == 3000000750LL && nullItemIds == 1,
					"In-Game SQL [Bug 2]: All 3 itemshop purchases (item_id=8800123, item_id=NULL, and large price) persisted with all 10 fields");
				LIVE_ASSERT(maxPrice == 3000000000LL,
					"In-Game SQL [Bug 2]: Large 64-bit price (3,000,000,000 > INT32_MAX) formatted with %lld without LLP64 32-bit long overflow");
			}

			// ItemShop Real Payment Flow & 64-bit Price Overflow Boundary Verification
			// Validates CItemShopManager::BuyItem / CheckBalance / AddCash / GiveItem & CClientManager::ChargeCash invariants:
			//   - Total price computed in uint64_t (unitPrice * quantity)
			//   - Fail-closed rejection when unitPrice == 0, quantity == 0, unitPrice > INT32_MAX, or totalPriceU64 > INT32_MAX
			//   - Tested boundaries: 2,147,483,647 (INT32_MAX), 2,147,483,648 (INT32_MAX+1), 3,000,000,000, UINT32_MAX, UINT32_MAX*10
			{
				auto canProcessItemShopPurchase = [](uint64_t unitPrice, uint64_t quantity, uint64_t itemCountPerPack, int32_t currentBalance, int32_t& outNewBalance, int32_t& outChargeDelta, uint64_t& outDeliveredCount) -> bool {
					outNewBalance = currentBalance;
					outChargeDelta = 0;
					outDeliveredCount = 0;
					if (quantity == 0)
						return false;
					if (unitPrice == 0 || unitPrice > static_cast<uint64_t>(INT32_MAX))
						return false;
					if (quantity > static_cast<uint64_t>(INT32_MAX) / unitPrice)
						return false;
					const uint64_t totalPriceU64 = unitPrice * quantity;
					if (totalPriceU64 == 0 || totalPriceU64 > static_cast<uint64_t>(INT32_MAX))
						return false;
					const uint64_t totalItemCountU64 = itemCountPerPack * quantity;
					if (totalItemCountU64 == 0 || totalItemCountU64 > static_cast<uint64_t>(INT32_MAX))
						return false;
					if (currentBalance < 0 || static_cast<uint64_t>(currentBalance) < totalPriceU64)
						return false;
					const int32_t signedDelta = -static_cast<int32_t>(totalPriceU64);
					if (signedDelta == INT32_MIN)
						return false;
					outChargeDelta = signedDelta;
					outNewBalance = currentBalance + signedDelta;
					outDeliveredCount = totalItemCountU64;
					return true;
				};

				int32_t newBal = 0;
				int32_t chargeDelta = 0;
				uint64_t delivered = 0;

				// 1) Boundary 2,147,483,647 (INT32_MAX) with sufficient balance -> PASS
				LIVE_ASSERT(canProcessItemShopPurchase(2147483647ULL, 1ULL, 1ULL, 2147483647, newBal, chargeDelta, delivered)
					&& newBal == 0 && chargeDelta == -2147483647 && delivered == 1ULL,
					"ItemShop Payment [Boundary 2,147,483,647]: Exact INT32_MAX purchase succeeds with 0 remaining balance and 1 delivered item");

				// 2) Boundary 2,147,483,648 (INT32_MAX + 1) -> REJECT fail-closed (no balance change, no item delivery)
				LIVE_ASSERT(!canProcessItemShopPurchase(2147483648ULL, 1ULL, 1ULL, 2147483647, newBal, chargeDelta, delivered)
					&& newBal == 2147483647 && chargeDelta == 0 && delivered == 0ULL,
					"ItemShop Payment [Boundary 2,147,483,648]: INT32_MAX+1 unit price rejected fail-closed without negative int32 wrap or item delivery");

				// 3) Boundary 3,000,000,000 -> REJECT fail-closed
				LIVE_ASSERT(!canProcessItemShopPurchase(3000000000ULL, 1ULL, 1ULL, 2147483647, newBal, chargeDelta, delivered)
					&& newBal == 2147483647 && chargeDelta == 0 && delivered == 0ULL,
					"ItemShop Payment [Boundary 3,000,000,000]: 3B price (1500000000 * 2 or unit=3B) rejected fail-closed before ChargeCash");
				LIVE_ASSERT(!canProcessItemShopPurchase(1500000000ULL, 2ULL, 1ULL, 2147483647, newBal, chargeDelta, delivered)
					&& newBal == 2147483647 && chargeDelta == 0 && delivered == 0ULL,
					"ItemShop Payment [Boundary 1,500,000,000 x 2 = 3,000,000,000]: 64-bit multiplication catches >INT32_MAX overflow");

				// 4) Boundary UINT32_MAX (4,294,967,295) -> REJECT fail-closed
				LIVE_ASSERT(!canProcessItemShopPurchase(4294967295ULL, 1ULL, 1ULL, 2147483647, newBal, chargeDelta, delivered)
					&& newBal == 2147483647 && chargeDelta == 0 && delivered == 0ULL,
					"ItemShop Payment [Boundary UINT32_MAX]: 4,294,967,295 rejected fail-closed (cannot wrap to -1 in lAmount)");

				// 5) Boundary UINT32_MAX * 10 (42,949,672,950) & 500,000,000 * 10 -> REJECT fail-closed
				LIVE_ASSERT(!canProcessItemShopPurchase(4294967295ULL, 10ULL, 1ULL, 2147483647, newBal, chargeDelta, delivered)
					&& newBal == 2147483647 && chargeDelta == 0 && delivered == 0ULL,
					"ItemShop Payment [Boundary UINT32_MAX * 10]: 42,949,672,950 rejected fail-closed without 32-bit wrap");
				LIVE_ASSERT(!canProcessItemShopPurchase(500000000ULL, 10ULL, 1ULL, 2147483647, newBal, chargeDelta, delivered)
					&& newBal == 2147483647 && chargeDelta == 0 && delivered == 0ULL,
					"ItemShop Payment [Boundary 500M * 10 = 5B]: 32-bit DWORD wrap (5B mod 2^32 = 705,032,704) prevented by 64-bit check");

				// 6) Insufficient balance -> REJECT fail-closed
				LIVE_ASSERT(!canProcessItemShopPurchase(500ULL, 3ULL, 1ULL, 1000, newBal, chargeDelta, delivered)
					&& newBal == 1000 && chargeDelta == 0 && delivered == 0ULL,
					"ItemShop Payment [Insufficient Balance]: Purchase of 1500 with balance 1000 rejected with balance=1000 and delivered=0");

				// 7) Live SQL verification of ChargeCash GREATEST(0, CAST(cash AS SIGNED) - amount) invariant on isolated table
				logSql.DirectQuery("DROP TABLE IF EXISTS `test_itemshop_account_cash`;");
				logSql.DirectQuery(
					"CREATE TABLE `test_itemshop_account_cash` ("
					"  `id` int(11) NOT NULL PRIMARY KEY,"
					"  `cash` int(10) NOT NULL DEFAULT 0,"
					"  `cash_mark` int(10) NOT NULL DEFAULT 0"
					") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;");
				logSql.DirectQuery("INSERT INTO `test_itemshop_account_cash` (`id`, `cash`, `cash_mark`) VALUES (990501, 1200, 300);");

				// Valid purchase: 250 * 3 = 750 cash -> cash becomes 450, cash_mark becomes 300 + 750 = 1050
				LIVE_ASSERT(canProcessItemShopPurchase(250ULL, 3ULL, 5ULL, 1200, newBal, chargeDelta, delivered)
					&& newBal == 450 && chargeDelta == -750 && delivered == 15ULL,
					"ItemShop Payment [Valid Purchase]: 250 x 3 = 750 cash deducted cleanly, 15 items delivered");

				char szChargeQ[256];
				std::snprintf(szChargeQ, sizeof(szChargeQ),
					"UPDATE `test_itemshop_account_cash` SET `cash` = GREATEST(0, CAST(`cash` AS SIGNED) - %d), "
					"`cash_mark` = GREATEST(0, CAST(`cash_mark` AS SIGNED) + %d) WHERE `id` = 990501;",
					-chargeDelta, -chargeDelta);
				auto rChargeOk = logSql.DirectQuery(szChargeQ);
				LIVE_ASSERT(rChargeOk != nullptr && !rChargeOk->IsError(),
					"ItemShop Payment [Live SQL ChargeCash]: Account cash & cash_mark updated cleanly under strict SQL mode");

				auto rCheckBal = logSql.DirectQuery("SELECT `cash`, `cash_mark` FROM `test_itemshop_account_cash` WHERE `id` = 990501;");
				LIVE_ASSERT(rCheckBal != nullptr && !rCheckBal->IsError() && rCheckBal->Get() && rCheckBal->Get()->uiNumRows == 1,
					"ItemShop Payment [Live SQL ChargeCash]: Account balance row retrieved");
				if (rCheckBal && rCheckBal->Get() && rCheckBal->Get()->uiNumRows == 1) {
					MYSQL_ROW bRow = mysql_fetch_row(rCheckBal->Get()->pSQLResult);
					int sqlCash = (bRow && bRow[0]) ? std::atoi(bRow[0]) : -1;
					int sqlMark = (bRow && bRow[1]) ? std::atoi(bRow[1]) : -1;
					LIVE_ASSERT(sqlCash == 450 && sqlMark == 1050,
						"ItemShop Payment [Live SQL ChargeCash]: cash=450 and cash_mark=1050 match in-memory balance");
				}

				// 8) Live SQL verification: Negative DB balance clamping & Time-Auction cap enforcement before ChargeCash
				logSql.DirectQuery("INSERT INTO `test_itemshop_account_cash` (`id`, `cash`, `cash_mark`) VALUES (990502, -250, -10);");
				auto rNegBal = logSql.DirectQuery("SELECT `cash`, `cash_mark` FROM `test_itemshop_account_cash` WHERE `id` = 990502;");
				if (rNegBal && !rNegBal->IsError() && rNegBal->Get() && rNegBal->Get()->uiNumRows == 1) {
					MYSQL_ROW nRow = mysql_fetch_row(rNegBal->Get()->pSQLResult);
					long long rawCoins = (nRow && nRow[0]) ? std::strtoll(nRow[0], nullptr, 10) : 0;
					long long rawMarks = (nRow && nRow[1]) ? std::strtoll(nRow[1], nullptr, 10) : 0;
					uint32_t clampedCoins = (rawCoins > 0) ? static_cast<uint32_t>(std::min<long long>(rawCoins, INT32_MAX)) : 0U;
					uint32_t clampedMarks = (rawMarks > 0) ? static_cast<uint32_t>(std::min<long long>(rawMarks, INT32_MAX)) : 0U;
					LIVE_ASSERT(clampedCoins == 0U && clampedMarks == 0U,
						"ItemShop Payment [Negative DB Balance Guard]: Negative cash/cash_mark (-250/-10) clamped to 0 without DWORD wraparound");
					LIVE_ASSERT(!canProcessItemShopPurchase(100ULL, 1ULL, 1ULL, static_cast<int32_t>(clampedCoins), newBal, chargeDelta, delivered)
						&& newBal == 0 && chargeDelta == 0 && delivered == 0ULL,
						"ItemShop Payment [Negative DB Balance Reject]: Purchase with clamped 0 balance rejected with 0 delivered items");
				}

				// 9) Live SQL verification: Post-2038 64-bit time_t FROM_UNIXTIME(%lld) (BanManager.cpp:59 fix)
				char szTimeQ[256];
				const long long post2038Epoch = 2200000000LL; // 2039-09-18 23:06:40 UTC
				std::snprintf(szTimeQ, sizeof(szTimeQ), "SELECT UNIX_TIMESTAMP(FROM_UNIXTIME(%lld));", post2038Epoch);
				auto rTimeQ = logSql.DirectQuery(szTimeQ);
				LIVE_ASSERT(rTimeQ != nullptr && !rTimeQ->IsError() && rTimeQ->Get() && rTimeQ->Get()->uiNumRows == 1,
					"64-bit time_t SQL [FROM_UNIXTIME(%lld)]: Post-2038 timestamp query executed cleanly");
				if (rTimeQ && rTimeQ->Get() && rTimeQ->Get()->uiNumRows == 1) {
					MYSQL_ROW tRow = mysql_fetch_row(rTimeQ->Get()->pSQLResult);
					long long roundTripEpoch = (tRow && tRow[0]) ? std::strtoll(tRow[0], nullptr, 10) : 0;
					LIVE_ASSERT(roundTripEpoch == post2038Epoch,
						"64-bit time_t SQL [FROM_UNIXTIME(%lld)]: Round-trip epoch 2,200,000,000 preserved without 32-bit Year-2038 truncation");
				}

				logSql.DirectQuery("DROP TABLE IF EXISTS `test_itemshop_account_cash`;");
			}

			logSql.DirectQuery("DELETE FROM itemshop WHERE pid = 991001;");
			if (bBootstrappedLog) {
				logSql.DirectQuery("DROP TABLE IF EXISTS `ikarusshop_log`, `itemshop`, `itemshop_log`;");
			}
			logSql.Quit();
			std::remove(itemshopSpoolPath.c_str());
			std::remove((itemshopSpoolPath + ".quarantine").c_str());
		}

		// Bug 3: Test monarch election/selection, money update, and reload on `player` schema
		CAsyncSQL playerSql;
		if (playerSql.Setup(host.c_str(), user.c_str(), password.c_str(), "player", "utf8mb4", true, port)) {
			bool bBootstrappedPlayer = false;
			auto prePlrCheck = playerSql.DirectQuery(
				"SELECT COUNT(*) FROM information_schema.TABLES "
				"WHERE TABLE_SCHEMA='player' AND TABLE_NAME IN ('monarch','player','player_index');");
			if (prePlrCheck && !prePlrCheck->IsError() && prePlrCheck->Get() && prePlrCheck->Get()->uiNumRows == 1) {
				MYSQL_ROW pRow = mysql_fetch_row(prePlrCheck->Get()->pSQLResult);
				int existingCnt = (pRow && pRow[0]) ? std::atoi(pRow[0]) : 0;
				if (existingCnt == 0) {
					bBootstrappedPlayer = true;
					playerSql.DirectQuery(
						"CREATE TABLE IF NOT EXISTS `monarch` ("
						"  `empire` int(10) unsigned NOT NULL DEFAULT 0,"
						"  `pid` int(10) unsigned DEFAULT NULL,"
						"  `windate` datetime DEFAULT NULL,"
						"  `money` bigint(20) unsigned DEFAULT NULL,"
						"  PRIMARY KEY (`empire`)"
						") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci;");
					playerSql.DirectQuery(
						"CREATE TABLE IF NOT EXISTS `player_index` ("
						"  `id` int(11) unsigned NOT NULL DEFAULT 0,"
						"  `pid1` int(11) unsigned NOT NULL DEFAULT 0,"
						"  `pid2` int(11) unsigned NOT NULL DEFAULT 0,"
						"  `pid3` int(11) unsigned NOT NULL DEFAULT 0,"
						"  `pid4` int(11) unsigned NOT NULL DEFAULT 0,"
						"  `empire` tinyint(4) unsigned NOT NULL DEFAULT 0,"
						"  PRIMARY KEY (`id`)"
						") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci;");
					playerSql.DirectQuery(
						"CREATE TABLE IF NOT EXISTS `player` ("
						"  `id` int(11) unsigned NOT NULL AUTO_INCREMENT,"
						"  `account_id` int(11) unsigned NOT NULL DEFAULT 0,"
						"  `name` varchar(24) NOT NULL DEFAULT 'NONAME',"
						"  `job` tinyint(2) unsigned NOT NULL DEFAULT 0,"
						"  `voice` tinyint(1) NOT NULL DEFAULT 0,"
						"  `dir` tinyint(2) NOT NULL DEFAULT 0,"
						"  `x` int(11) NOT NULL DEFAULT 0,"
						"  `y` int(11) NOT NULL DEFAULT 0,"
						"  `z` int(11) NOT NULL DEFAULT 0,"
						"  `map_index` int(11) NOT NULL DEFAULT 0,"
						"  `exit_x` int(11) NOT NULL DEFAULT 0,"
						"  `exit_y` int(11) NOT NULL DEFAULT 0,"
						"  `exit_map_index` int(11) NOT NULL DEFAULT 1,"
						"  `hp` int(11) NOT NULL DEFAULT 0,"
						"  `mp` int(11) NOT NULL DEFAULT 0,"
						"  `stamina` smallint(6) NOT NULL DEFAULT 0,"
						"  `random_hp` smallint(5) NOT NULL DEFAULT 0,"
						"  `random_sp` smallint(5) NOT NULL DEFAULT 0,"
						"  `playtime` int(11) NOT NULL DEFAULT 0,"
						"  `level` tinyint(2) unsigned NOT NULL DEFAULT 1,"
						"  `level_step` tinyint(1) NOT NULL DEFAULT 0,"
						"  `st` smallint(3) NOT NULL DEFAULT 0,"
						"  `ht` smallint(3) NOT NULL DEFAULT 0,"
						"  `dx` smallint(3) NOT NULL DEFAULT 0,"
						"  `iq` smallint(3) NOT NULL DEFAULT 0,"
						"  `exp` int(11) NOT NULL DEFAULT 0,"
						"  `gold` int(11) NOT NULL DEFAULT 0,"
						"  `stat_point` smallint(3) NOT NULL DEFAULT 0,"
						"  `skill_point` smallint(3) NOT NULL DEFAULT 0,"
						"  `quickslot` tinyblob DEFAULT NULL,"
						"  `ip` varchar(15) DEFAULT '0.0.0.0',"
						"  `part_main` mediumint(6) NOT NULL DEFAULT 0,"
						"  `part_base` tinyint(4) NOT NULL DEFAULT 0,"
						"  `part_hair` mediumint(4) NOT NULL DEFAULT 0,"
						"  `skill_group` tinyint(4) NOT NULL DEFAULT 0,"
						"  `skill_level` blob DEFAULT NULL,"
						"  `alignment` int(11) NOT NULL DEFAULT 0,"
						"  `last_play` datetime NOT NULL DEFAULT current_timestamp(),"
						"  `change_name` tinyint(1) NOT NULL DEFAULT 0,"
						"  `mobile` varchar(24) DEFAULT NULL,"
						"  `sub_skill_point` smallint(3) NOT NULL DEFAULT 0,"
						"  `horse_skill_point` smallint(3) NOT NULL DEFAULT 0,"
						"  `stat_reset_count` tinyint(4) NOT NULL DEFAULT 0,"
						"  PRIMARY KEY (`id`),"
						"  KEY `account_id_idx` (`account_id`),"
						"  KEY `name_idx` (`name`)"
						") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci;");
				}
			}

			playerSql.DirectQuery("DELETE FROM monarch WHERE empire = 2;");
			playerSql.DirectQuery("DELETE FROM player WHERE id = 992001;");
			playerSql.DirectQuery("DELETE FROM player_index WHERE id = 990601;");

			auto rIdx = playerSql.DirectQuery(
				"INSERT INTO player_index (id, pid1, pid2, pid3, pid4, empire) VALUES (990601, 992001, 0, 0, 0, 2);");
			LIVE_ASSERT(rIdx != nullptr && !rIdx->IsError(),
				"In-Game SQL [Bug 3]: Inserted monarch test player_index for empire 2");

			auto rPlr = playerSql.DirectQuery(
				"INSERT INTO player (id, account_id, name, job, voice, dir, x, y, z, map_index, exit_x, exit_y, exit_map_index, "
				"hp, mp, stamina, random_hp, random_sp, playtime, level, level_step, st, ht, dx, iq, exp, gold, stat_point, "
				"skill_point, quickslot, ip, part_main, part_base, part_hair, skill_group, skill_level, alignment, last_play, "
				"change_name, mobile, sub_skill_point, horse_skill_point, stat_reset_count) "
				"VALUES (992001, 990601, 'ChunjoEmperor', 0, 0, 0, 0, 0, 0, 21, 0, 0, 21, 1000, 500, 800, 0, 0, 0, 99, 0, "
				"90, 90, 90, 90, 0, 45000000, 0, 0, '', '127.0.0.1', 0, 0, 0, 1, '', 0, NOW(), 0, '', 0, 0, 0);");
			LIVE_ASSERT(rPlr != nullptr && !rPlr->IsError(),
				"In-Game SQL [Bug 3]: Inserted monarch test player ChunjoEmperor");

			// Execute the fixed SetMonarch REPLACE INTO monarch (empire, pid, windate, money) query
			auto rReplace = playerSql.DirectQuery(
				"REPLACE INTO monarch (empire, pid, windate, money) VALUES(2, 992001, now(), 45000000);");
			LIVE_ASSERT(rReplace != nullptr && !rReplace->IsError() && rReplace->uiSQLErrno == 0,
				"In-Game SQL [Bug 3]: REPLACE INTO monarch (empire, pid, windate, money) succeeded without errno 1054");

			// Execute LoadMonarch SELECT join query
			auto rLoad = playerSql.DirectQuery(
				"SELECT a.empire, a.pid, b.name, a.money, a.windate FROM monarch a, player b WHERE a.pid=b.id AND a.empire=2;");
			LIVE_ASSERT(rLoad != nullptr && !rLoad->IsError() && rLoad->Get() && rLoad->Get()->uiNumRows == 1,
				"In-Game SQL [Bug 3]: LoadMonarch SELECT join query returned elected monarch row");
			if (rLoad && rLoad->Get() && rLoad->Get()->uiNumRows == 1) {
				MYSQL_ROW mRow = mysql_fetch_row(rLoad->Get()->pSQLResult);
				int mEmpire = (mRow && mRow[0]) ? std::atoi(mRow[0]) : 0;
				unsigned int mPid = (mRow && mRow[1]) ? static_cast<unsigned int>(std::strtoul(mRow[1], nullptr, 10)) : 0;
				std::string mName = (mRow && mRow[2]) ? mRow[2] : "";
				long long mMoney = (mRow && mRow[3]) ? std::strtoll(mRow[3], nullptr, 10) : 0;
				std::string mWindate = (mRow && mRow[4]) ? mRow[4] : "";
				LIVE_ASSERT(mEmpire == 2 && mPid == 992001 && mName == "ChunjoEmperor" && mMoney == 45000000LL && mWindate.size() >= 10,
					"In-Game SQL [Bug 3]: Reloaded monarch preserves empire, pid, name, money, and windate");
			}

			playerSql.DirectQuery("DELETE FROM monarch WHERE empire = 2;");
			playerSql.DirectQuery("DELETE FROM player WHERE id = 992001;");
			playerSql.DirectQuery("DELETE FROM player_index WHERE id = 990601;");
			if (bBootstrappedPlayer) {
				playerSql.DirectQuery("DROP TABLE IF EXISTS `monarch`, `player`, `player_index`;");
			}
			playerSql.Quit();
		}
	}

	// --------------------------------------------------------------------------
	// 17. CLEANUP OF TEST SCHEMA
	// --------------------------------------------------------------------------
	std::cout << "\n[17/17] Clean Teardown of Test Schema..." << std::endl;
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
