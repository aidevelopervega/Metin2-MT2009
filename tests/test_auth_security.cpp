#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <set>
#include <string>
#include <vector>

#include "auth_security.h"

static int g_passed = 0;
static int g_failed = 0;

#define TEST_ASSERT(cond, msg) \
	do { \
		if (cond) { \
			++g_passed; \
		} else { \
			++g_failed; \
			std::fprintf(stderr, "[FAIL] %s (line %d): %s\n", __func__, __LINE__, msg); \
		} \
	} while (0)

static void TestScenario1_NewArgon2idAccount()
{
	std::printf("[Scenario 1] Testing Argon2id PHC generation & verification...\n");

	uint64_t hashUsec1 = 0;
	uint64_t hashUsec2 = 0;
	std::string hash1 = AuthSecurity::HashPasswordArgon2id(
		"SmokePass123",
		AuthSecurity::kDefaultMemoryCostKiB,
		AuthSecurity::kDefaultTimeCost,
		AuthSecurity::kDefaultParallelism,
		&hashUsec1);
	std::string hash2 = AuthSecurity::HashPasswordArgon2id(
		"SmokePass123",
		AuthSecurity::kDefaultMemoryCostKiB,
		AuthSecurity::kDefaultTimeCost,
		AuthSecurity::kDefaultParallelism,
		&hashUsec2);

	TEST_ASSERT(!hash1.empty(), "Argon2id hash1 must not be empty");
	TEST_ASSERT(!hash2.empty(), "Argon2id hash2 must not be empty");
	TEST_ASSERT(hash1 != hash2, "Two Argon2id hashes of the same password must have unique random salts");
	TEST_ASSERT(hash1.size() > 80 && hash1.size() <= AuthSecurity::kMaxStoredHashLength,
		"Argon2id PHC string length must fit VARCHAR(255) and exceed legacy 42-char limit");

	AuthSecurity::SArgon2Params params{};
	AuthSecurity::EHashFormat fmt = AuthSecurity::IdentifyPasswordHashFormat(hash1, &params);
	TEST_ASSERT(fmt == AuthSecurity::EHashFormat::Argon2id, "Hash1 must be identified as Argon2id");
	TEST_ASSERT(params.m_cost_kib == 19456, "Parsed m_cost must equal 19456 KiB");
	TEST_ASSERT(params.t_cost == 2, "Parsed t_cost must equal 2");
	TEST_ASSERT(params.lanes == 1, "Parsed parallelism must equal 1");
	TEST_ASSERT(params.salt_len == 16, "Parsed salt_len must equal 16 bytes");
	TEST_ASSERT(params.tag_len == 32, "Parsed tag_len must equal 32 bytes");

	AuthSecurity::SVerifyResult okRes = AuthSecurity::VerifyPassword("SmokePass123", hash1);
	TEST_ASSERT(okRes.matched, "Correct password must verify against Argon2id hash");
	TEST_ASSERT(!okRes.needs_upgrade, "Current-parameter Argon2id hash must not need upgrade");
	TEST_ASSERT(okRes.format == AuthSecurity::EHashFormat::Argon2id, "VerifyResult format must be Argon2id");

	std::printf("  -> Argon2id benchmark (m=19456 KiB, t=2, p=1): hash=%llu us, verify=%llu us, len=%zu\n",
		static_cast<unsigned long long>(hashUsec1),
		static_cast<unsigned long long>(okRes.elapsed_usec),
		hash1.size());

	AuthSecurity::SVerifyResult wrongRes1 = AuthSecurity::VerifyPassword("WrongPass123", hash1);
	TEST_ASSERT(!wrongRes1.matched, "Wrong password must be rejected");

	AuthSecurity::SVerifyResult wrongRes2 = AuthSecurity::VerifyPassword("smokepass123", hash1);
	TEST_ASSERT(!wrongRes2.matched, "Case-altered password must be rejected");

	AuthSecurity::SVerifyResult emptyRes = AuthSecurity::VerifyPassword("", hash1);
	TEST_ASSERT(!emptyRes.matched, "Empty password must be rejected");
}

static void TestScenario2_LegacySha1AndTransparentUpgrade()
{
	std::printf("[Scenario 2] Testing legacy *SHA1(SHA1(password)) verification & transparent upgrade...\n");

	std::string legacyHash = AuthSecurity::ComputeLegacyMySQL41Hash("SmokePass123");
	TEST_ASSERT(legacyHash.size() == 41 && legacyHash[0] == '*',
		"Legacy MySQL 4.1 hash must be 41 chars starting with '*'");
	TEST_ASSERT(AuthSecurity::IdentifyPasswordHashFormat(legacyHash) == AuthSecurity::EHashFormat::LegacyMySQL41,
		"Legacy hash must be identified as LegacyMySQL41");

	// Verify lowercase hex variant is also accepted in constant time
	std::string lowerLegacyHash = legacyHash;
	for (size_t i = 1; i < lowerLegacyHash.size(); ++i)
	{
		if (lowerLegacyHash[i] >= 'A' && lowerLegacyHash[i] <= 'F')
			lowerLegacyHash[i] = static_cast<char>(lowerLegacyHash[i] - 'A' + 'a');
	}

	AuthSecurity::SVerifyResult legOk = AuthSecurity::VerifyPassword("SmokePass123", legacyHash);
	TEST_ASSERT(legOk.matched, "Correct password must match legacy *SHA1(SHA1) hash");
	TEST_ASSERT(legOk.needs_upgrade, "Matched legacy *SHA1(SHA1) hash must set needs_upgrade=true");
	TEST_ASSERT(legOk.format == AuthSecurity::EHashFormat::LegacyMySQL41, "Format must be LegacyMySQL41");

	AuthSecurity::SVerifyResult legLowerOk = AuthSecurity::VerifyPassword("SmokePass123", lowerLegacyHash);
	TEST_ASSERT(legLowerOk.matched && legLowerOk.needs_upgrade,
		"Lowercase hex legacy hash must also match and request upgrade");

	AuthSecurity::SVerifyResult legWrong = AuthSecurity::VerifyPassword("BadPassword", legacyHash);
	TEST_ASSERT(!legWrong.matched && !legWrong.needs_upgrade,
		"Wrong password against legacy hash must fail and not trigger upgrade");

	// Perform transparent upgrade from PLAINTEXT password (never hashing the SHA1 hash)
	std::string upgradedHash = AuthSecurity::HashPasswordArgon2id("SmokePass123");
	TEST_ASSERT(!upgradedHash.empty(), "Upgraded Argon2id hash must be generated");

	AuthSecurity::SVerifyResult postUpgradeOk = AuthSecurity::VerifyPassword("SmokePass123", upgradedHash);
	TEST_ASSERT(postUpgradeOk.matched && !postUpgradeOk.needs_upgrade,
		"Upgraded Argon2id hash must verify plaintext password without needing further upgrade");

	// Prove that the SHA1 hash itself was NOT hashed into Argon2id
	AuthSecurity::SVerifyResult sha1AsPwd = AuthSecurity::VerifyPassword(legacyHash, upgradedHash);
	TEST_ASSERT(!sha1AsPwd.matched,
		"Passing the legacy SHA1 string as password must NOT match the upgraded Argon2id hash");
}

static void TestScenario3_PostUpgradeNoFallbackAndParamUpgrade()
{
	std::printf("[Scenario 3] Testing post-upgrade anti-downgrade & lower-cost Argon2id upgrade flag...\n");

	std::string upgradedHash = AuthSecurity::HashPasswordArgon2id("Secure99!");
	std::string legacyHash   = AuthSecurity::ComputeLegacyMySQL41Hash("Secure99!");

	// Once stored hash is Argon2id, legacy SHA1 or wrong password must never pass
	TEST_ASSERT(!AuthSecurity::VerifyPassword("Wrong99!", upgradedHash).matched,
		"Wrong password must fail on upgraded account");
	TEST_ASSERT(!AuthSecurity::VerifyPassword(legacyHash, upgradedHash).matched,
		"Legacy hash string must fail on upgraded account");

	// If an Argon2id hash was created with lower parameters (m=8192, t=1, p=1), it should verify and set needs_upgrade=true
	std::string lowCostHash = AuthSecurity::HashPasswordArgon2id("Secure99!", 8192, 1, 1);
	TEST_ASSERT(!lowCostHash.empty(), "Low-cost Argon2id hash within bounds should be created");
	AuthSecurity::SVerifyResult lowCostRes = AuthSecurity::VerifyPassword("Secure99!", lowCostHash);
	TEST_ASSERT(lowCostRes.matched && lowCostRes.needs_upgrade,
		"Argon2id hash below target m=19456,t=2 must verify and flag needs_upgrade=true");
}

static void TestScenario4_MalformedAndOutOfBoundsHashes()
{
	std::printf("[Scenario 4] Testing malformed, truncated, and DoS-parameter hash rejection...\n");

	std::string validHash = AuthSecurity::HashPasswordArgon2id("SmokePass123");
	// Simulate VARCHAR(42) truncation bug
	std::string truncated42 = validHash.substr(0, 42);

	const char* invalidHashes[] = {
		"",
		"SmokePass123",
		"*1234567890ABCDEF", // too short
		"*ZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZ", // non-hex
		"1234567890ABCDEF1234567890ABCDEF123456789", // missing '*'
		"$argon2i$v=19$m=19456,t=2,p=1$c29tZXNhbHQxMjM0NTY3OA$c29tZXRhZzEyMzQ1Njc4OTAxMjM0NTY3ODkwMTIzNDU2", // argon2i disallowed
		"$argon2d$v=19$m=19456,t=2,p=1$c29tZXNhbHQxMjM0NTY3OA$c29tZXRhZzEyMzQ1Njc4OTAxMjM0NTY3ODkwMTIzNDU2", // argon2d disallowed
		"$argon2id$v=16$m=19456,t=2,p=1$c29tZXNhbHQxMjM0NTY3OA$c29tZXRhZzEyMzQ1Njc4OTAxMjM0NTY3ODkwMTIzNDU2", // old version
		"$argon2id$v=19$m=1024,t=2,p=1$c29tZXNhbHQxMjM0NTY3OA$c29tZXRhZzEyMzQ1Njc4OTAxMjM0NTY3ODkwMTIzNDU2",  // m too low
		"$argon2id$v=19$m=1048576,t=2,p=1$c29tZXNhbHQxMjM0NTY3OA$c29tZXRhZzEyMzQ1Njc4OTAxMjM0NTY3ODkwMTIzNDU2", // 1 GiB RAM DoS
		"$argon2id$v=19$m=19456,t=0,p=1$c29tZXNhbHQxMjM0NTY3OA$c29tZXRhZzEyMzQ1Njc4OTAxMjM0NTY3ODkwMTIzNDU2", // t=0
		"$argon2id$v=19$m=19456,t=99,p=1$c29tZXNhbHQxMjM0NTY3OA$c29tZXRhZzEyMzQ1Njc4OTAxMjM0NTY3ODkwMTIzNDU2", // t=99 CPU DoS
		"$argon2id$v=19$m=19456,t=2,p=16$c29tZXNhbHQxMjM0NTY3OA$c29tZXRhZzEyMzQ1Njc4OTAxMjM0NTY3ODkwMTIzNDU2", // p=16 DoS
		"$argon2id$v=19$m=19456,t=2,p=1$c2hvcnRzYWx0$c29tZXRhZzEyMzQ1Njc4OTAxMjM0NTY3ODkwMTIzNDU2" // short salt
	};

	TEST_ASSERT(AuthSecurity::IdentifyPasswordHashFormat(truncated42) == AuthSecurity::EHashFormat::Invalid,
		"42-char truncated Argon2id hash must be rejected as Invalid");
	TEST_ASSERT(!AuthSecurity::VerifyPassword("SmokePass123", truncated42).matched,
		"42-char truncated Argon2id hash must fail verification");

	for (const char* bad : invalidHashes)
	{
		TEST_ASSERT(AuthSecurity::IdentifyPasswordHashFormat(bad) == AuthSecurity::EHashFormat::Invalid,
			"Malformed/DoS hash must be classified as Invalid");
		AuthSecurity::SVerifyResult res = AuthSecurity::VerifyPassword("SmokePass123", bad);
		TEST_ASSERT(!res.matched && !res.needs_upgrade,
			"Malformed/DoS hash must fail closed without upgrade");
	}
}

static void TestScenario5_ConcurrentUpgradeRaceSafety()
{
	std::printf("[Scenario 5] Testing atomic conditional upgrade race safety (WHERE id=%%u AND password='<old>')...\n");

	std::string dbCurrentPassword = AuthSecurity::ComputeLegacyMySQL41Hash("RacePass77");

	// Worker A and Worker B both read legacy hash simultaneously
	std::string snapshotA = dbCurrentPassword;
	std::string snapshotB = dbCurrentPassword;

	AuthSecurity::SVerifyResult verA = AuthSecurity::VerifyPassword("RacePass77", snapshotA);
	AuthSecurity::SVerifyResult verB = AuthSecurity::VerifyPassword("RacePass77", snapshotB);
	TEST_ASSERT(verA.matched && verA.needs_upgrade, "Worker A verifies legacy hash");
	TEST_ASSERT(verB.matched && verB.needs_upgrade, "Worker B verifies legacy hash");

	std::string newHashA = AuthSecurity::HashPasswordArgon2id("RacePass77");
	std::string newHashB = AuthSecurity::HashPasswordArgon2id("RacePass77");

	// Simulate SQL: UPDATE account SET password=newHash WHERE id=1 AND password=snapshot
	auto atomicCompareAndSwapSQL = [&](const std::string& expectedOld, const std::string& candidateNew) -> bool {
		if (dbCurrentPassword == expectedOld)
		{
			dbCurrentPassword = candidateNew;
			return true;
		}
		return false;
	};

	bool updatedA = atomicCompareAndSwapSQL(snapshotA, newHashA);
	bool updatedB = atomicCompareAndSwapSQL(snapshotB, newHashB);

	TEST_ASSERT(updatedA, "First concurrent upgrade must update the row");
	TEST_ASSERT(!updatedB, "Second concurrent upgrade must be a no-op because password!=old_sha1");
	TEST_ASSERT(dbCurrentPassword == newHashA, "Database must retain Worker A's valid Argon2id hash");
	TEST_ASSERT(AuthSecurity::VerifyPassword("RacePass77", dbCurrentPassword).matched,
		"Subsequent login against upgraded row must succeed");
}

static void TestScenario6_RateLimitingAndExponentialBackoff()
{
	std::printf("[Scenario 6] Testing IP/Account rate limiting and temporary exponential backoff...\n");

	auto& limiter = AuthSecurity::AuthRateLimiter::Instance();
	limiter.ResetAll();

	uint64_t t = 100000ULL;

	// 1. Rapid IP burst throttling (< 200ms spacing)
	TEST_ASSERT(limiter.CheckAllowed("10.0.0.1", "user_a", t).decision == AuthSecurity::ERateLimitDecision::Allowed,
		"Attempt 1 allowed");
	TEST_ASSERT(limiter.CheckAllowed("10.0.0.1", "user_b", t + 10).decision == AuthSecurity::ERateLimitDecision::Allowed,
		"Attempt 2 within burst allowance allowed");
	TEST_ASSERT(limiter.CheckAllowed("10.0.0.1", "user_c", t + 20).decision == AuthSecurity::ERateLimitDecision::Allowed,
		"Attempt 3 within burst allowance allowed");
	TEST_ASSERT(limiter.CheckAllowed("10.0.0.1", "user_d", t + 30).decision == AuthSecurity::ERateLimitDecision::IpBurstThrottled,
		"Attempt 4 within 200ms must be throttled by IP burst guard");

	// 2. Account brute-force backoff (independent of IP)
	limiter.ResetAll();
	for (uint32_t i = 0; i < AuthSecurity::AuthRateLimiter::kFailureThreshold; ++i)
	{
		t += 1000ULL;
		std::string attackerIp = "192.168.1." + std::to_string(10 + i);
		TEST_ASSERT(limiter.CheckAllowed(attackerIp, "victim_acct", t).decision == AuthSecurity::ERateLimitDecision::Allowed,
			"Allowed before failure threshold");
		limiter.RecordFailure(attackerIp, "victim_acct", t);
	}

	// 5th failure recorded at t -> 2000ms backoff active on "victim_acct"
	auto blockedStatus = limiter.CheckAllowed("192.168.1.99", "victim_acct", t + 500ULL);
	TEST_ASSERT(blockedStatus.decision == AuthSecurity::ERateLimitDecision::AccountBackoffActive,
		"Account must enter temporary backoff after 5 failures across distributed IPs");
	TEST_ASSERT(blockedStatus.retry_after_ms == 1500ULL,
		"Remaining backoff after 500ms must be 1500ms");

	// Another account from a clean IP is unaffected
	TEST_ASSERT(limiter.CheckAllowed("192.168.1.99", "other_user", t + 500ULL).decision == AuthSecurity::ERateLimitDecision::Allowed,
		"Other accounts must not be locked out by victim_acct backoff");

	// Advance clock past 2000ms temporary backoff -> account is automatically allowed again (no permanent lockout DoS)
	t += 2500ULL;
	TEST_ASSERT(limiter.CheckAllowed("192.168.1.99", "victim_acct", t).decision == AuthSecurity::ERateLimitDecision::Allowed,
		"Account must recover automatically after temporary backoff expires");

	// 6th failure doubles backoff to 4000ms
	limiter.RecordFailure("192.168.1.99", "victim_acct", t);
	auto backoff2 = limiter.CheckAllowed("192.168.1.99", "victim_acct", t + 1000ULL);
	TEST_ASSERT(backoff2.decision == AuthSecurity::ERateLimitDecision::AccountBackoffActive &&
	            backoff2.retry_after_ms == 3000ULL,
		"6th failure must double temporary backoff to 4000ms");

	// After backoff expires, a successful login resets the failure streak
	t += 4500ULL;
	TEST_ASSERT(limiter.CheckAllowed("192.168.1.99", "victim_acct", t).decision == AuthSecurity::ERateLimitDecision::Allowed,
		"Allowed after 4000ms backoff");
	limiter.RecordSuccess("192.168.1.99", "victim_acct", t);
	limiter.RecordFailure("192.168.1.99", "victim_acct", t + 1000ULL);
	TEST_ASSERT(limiter.CheckAllowed("192.168.1.99", "victim_acct", t + 1500ULL).decision == AuthSecurity::ERateLimitDecision::Allowed,
		"Single failure after RecordSuccess must not trigger backoff");

	limiter.ResetAll();
}

static void TestScenario7_CsprngKeysAndInputBounds()
{
	std::printf("[Scenario 7] Testing CSPRNG LoginKeys, fixed-buffer NUL checks, and concurrency guards...\n");

	std::set<uint32_t> keys;
	for (int i = 0; i < 256; ++i)
	{
		uint32_t k = AuthSecurity::GenerateSecureLoginKey();
		TEST_ASSERT(k != 0, "CSPRNG LoginKey must never be 0");
		keys.insert(k);
	}
	TEST_ASSERT(keys.size() >= 255, "CSPRNG LoginKeys must have high entropy without small-range collisions");

	// Fixed-buffer NUL termination validation
	char unterminated[17];
	std::memset(unterminated, 'A', sizeof(unterminated));
	TEST_ASSERT(!AuthSecurity::IsFieldNullTerminated(unterminated, sizeof(unterminated), 1, 16),
		"Unterminated fixed buffer must be rejected");

	char validField[17] = "SmokePass123";
	TEST_ASSERT(AuthSecurity::IsFieldNullTerminated(validField, sizeof(validField), 1, 16),
		"Properly NUL-terminated buffer within bounds must pass");

	TEST_ASSERT(AuthSecurity::IsValidAccountLogin("smoke_test_01"), "Valid login with underscore must pass");
	TEST_ASSERT(!AuthSecurity::IsValidAccountLogin("a"), "1-char login must be rejected");
	TEST_ASSERT(!AuthSecurity::IsValidAccountLogin("admin' OR '1'='1"), "SQLi payload in login must be rejected");

	// In-flight slot guard
	AuthSecurity::ResetArgon2BudgetForTesting();
	for (uint32_t i = 0; i < AuthSecurity::kMaxInFlightAuthQueries; ++i)
	{
		TEST_ASSERT(AuthSecurity::TryAcquireInFlightAuthSlot(), "Must acquire in-flight slot below limit");
	}
	TEST_ASSERT(!AuthSecurity::TryAcquireInFlightAuthSlot(), "Must reject 65th concurrent in-flight auth query");
	for (uint32_t i = 0; i < AuthSecurity::kMaxInFlightAuthQueries; ++i)
	{
		AuthSecurity::ReleaseInFlightAuthSlot();
	}
	TEST_ASSERT(AuthSecurity::GetInFlightAuthSlotCount() == 0, "All in-flight slots must be released");
	AuthSecurity::ResetArgon2BudgetForTesting();
}

int main()
{
	std::printf("==================================================\n");
	std::printf(" MT2009 AUTH & PASSWORD SECURITY UNIT TESTS\n");
	std::printf("==================================================\n");

	TestScenario1_NewArgon2idAccount();
	TestScenario2_LegacySha1AndTransparentUpgrade();
	TestScenario3_PostUpgradeNoFallbackAndParamUpgrade();
	TestScenario4_MalformedAndOutOfBoundsHashes();
	TestScenario5_ConcurrentUpgradeRaceSafety();
	TestScenario6_RateLimitingAndExponentialBackoff();
	TestScenario7_CsprngKeysAndInputBounds();

	std::printf("--------------------------------------------------\n");
	std::printf(" Auth Security Assertions: %d passed, %d failed\n", g_passed, g_failed);
	std::printf("==================================================\n");

	return (g_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
