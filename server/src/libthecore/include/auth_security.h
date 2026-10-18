#ifndef __INC_LIBTHECORE_AUTH_SECURITY_H__
#define __INC_LIBTHECORE_AUTH_SECURITY_H__

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

namespace AuthSecurity
{
	// OWASP-aligned Argon2id baseline parameters (m=19456 KiB / 19 MiB, t=2, p=1)
	inline constexpr uint32_t kDefaultMemoryCostKiB = 19456;
	inline constexpr uint32_t kDefaultTimeCost      = 2;
	inline constexpr uint32_t kDefaultParallelism   = 1;
	inline constexpr size_t   kDefaultSaltBytes     = 16;
	inline constexpr size_t   kDefaultTagBytes      = 32;

	// Strict verification parameter bounds to prevent CPU/RAM exhaustion via crafted DB hashes
	inline constexpr uint32_t kMinAllowedMemoryKiB  = 8192;   // 8 MiB
	inline constexpr uint32_t kMaxAllowedMemoryKiB  = 65536;  // 64 MiB
	inline constexpr uint32_t kMinAllowedTimeCost   = 1;
	inline constexpr uint32_t kMaxAllowedTimeCost   = 8;
	inline constexpr uint32_t kMinAllowedParallelism = 1;
	inline constexpr uint32_t kMaxAllowedParallelism = 4;

	inline constexpr size_t   kMaxPasswordLength    = 16;     // PASSWD_MAX_LEN in 32-bit wire protocol
	inline constexpr size_t   kMaxLoginLength       = 30;     // LOGIN_MAX_LEN in 32-bit wire protocol
	inline constexpr size_t   kMinLoginLength       = 2;
	inline constexpr size_t   kMinSocialIdLength    = 7;
	inline constexpr size_t   kMaxSocialIdLength    = 18;     // SOCIAL_ID_MAX_LEN in 32-bit wire protocol
	inline constexpr size_t   kMaxStoredHashLength  = 255;    // account.password VARCHAR(255)

	// Concurrency & CPU budget limits for AUTH_SERVER
	inline constexpr uint32_t kMaxInFlightAuthQueries = 64;
	inline constexpr uint32_t kMaxArgon2OpsPerSecond  = 40;

	// LoginByKey TTL in milliseconds (60 seconds)
	inline constexpr uint32_t kLoginKeyTTLMs          = 60000;

	enum class EHashFormat : uint8_t
	{
		Invalid = 0,
		LegacyMySQL41 = 1, // 41-char *UPPERHEX40 (*SHA1(SHA1(password)))
		Argon2id = 2       // $argon2id$v=19$m=...,t=...,p=...$<salt>$<tag>
	};

	struct SArgon2Params
	{
		uint32_t m_cost_kib = 0;
		uint32_t t_cost     = 0;
		uint32_t lanes      = 0;
		size_t   salt_len   = 0;
		size_t   tag_len    = 0;
	};

	struct SVerifyResult
	{
		bool        matched       = false;
		bool        needs_upgrade = false;
		EHashFormat format        = EHashFormat::Invalid;
		uint64_t    elapsed_usec  = 0;
	};

	// Monotonic clock in milliseconds and microseconds
	uint64_t GetMonotonicMs() noexcept;
	uint64_t GetMonotonicUsec() noexcept;

	// Guaranteed memory scrubbing that will not be optimized away
	void SecureZeroMemoryBuffer(void* ptr, size_t len) noexcept;

	// Cryptographically secure random bytes and non-zero 32-bit session key
	bool GenerateCSPRNGBytes(void* buf, size_t len) noexcept;
	uint32_t GenerateSecureLoginKey() noexcept;

	// Constant-time comparison helpers
	bool ConstantTimeEquals(const void* a, const void* b, size_t len) noexcept;
	bool ConstantTimeHexEquals(std::string_view a, std::string_view b) noexcept;

	// Legacy MySQL 4.1+ hash (*SHA1(SHA1(password))) for legacy account migration only
	std::string ComputeLegacyMySQL41Hash(std::string_view password);

	// Strictly parse and validate stored password hash format and Argon2id parameters
	EHashFormat IdentifyPasswordHashFormat(std::string_view stored_hash, SArgon2Params* out_params = nullptr) noexcept;

	// Generate PHC-formatted Argon2id hash ($argon2id$v=19$m=...,t=...,p=...$<salt>$<tag>)
	std::string HashPasswordArgon2id(
		std::string_view password,
		uint32_t m_cost_kib = kDefaultMemoryCostKiB,
		uint32_t t_cost     = kDefaultTimeCost,
		uint32_t lanes      = kDefaultParallelism,
		uint64_t* out_elapsed_usec = nullptr);

	// Verify password against stored hash:
	// - Argon2id hashes are verified ONLY with Argon2id (never falls back to LegacyMySQL41).
	// - LegacyMySQL41 hashes are verified in constant time and flagged with needs_upgrade = true.
	// - Malformed/unknown hashes fail closed immediately.
	SVerifyResult VerifyPassword(std::string_view password, std::string_view stored_hash);

	// Perform a real Argon2id verification against a dummy PHC hash to prevent username timing enumeration
	void VerifyDummyPassword(std::string_view password) noexcept;

	// Fixed-buffer NUL-termination and character safety checks for network packets
	bool IsFieldNullTerminated(const char* field, size_t buf_capacity, size_t min_len, size_t max_len) noexcept;
	bool IsValidAccountLogin(std::string_view login) noexcept;
	bool IsValidPlaintextPassword(std::string_view password) noexcept;
	bool IsValidSocialId(std::string_view social_id) noexcept;

	struct SNewAccountCredentials
	{
		std::string login;
		std::string argon2id_hash;
		std::string social_id;
	};

	// Validate new account inputs (login 2..30 chars, password 1..16 chars, social_id 7..18 digits)
	// and generate a fresh Argon2id PHC hash (never SHA-1).
	bool PrepareNewAccountCredentials(
		std::string_view login,
		std::string_view password,
		std::string_view social_id,
		SNewAccountCredentials* out_creds,
		std::string* out_error = nullptr);

	// In-flight auth query slot and CPU budget guards
	bool TryAcquireInFlightAuthSlot() noexcept;
	void ReleaseInFlightAuthSlot() noexcept;
	uint32_t GetInFlightAuthSlotCount() noexcept;
	bool TryAcquireArgon2Budget(uint64_t now_ms = 0) noexcept;
	void ResetArgon2BudgetForTesting() noexcept;

	// Independent IP-based and Account-based rate limiting with temporary exponential backoff
	enum class ERateLimitDecision : uint8_t
	{
		Allowed = 0,
		IpBurstThrottled = 1,
		IpBackoffActive = 2,
		AccountBackoffActive = 3
	};

	struct SRateLimitStatus
	{
		ERateLimitDecision decision      = ERateLimitDecision::Allowed;
		uint64_t           retry_after_ms = 0;
		uint32_t           ip_failures    = 0;
		uint32_t           acct_failures  = 0;
	};

	class AuthRateLimiter
	{
	public:
		static constexpr uint32_t kFailureThreshold      = 5;
		static constexpr uint64_t kFailureWindowMs       = 60000;  // 60s window
		static constexpr uint64_t kMinIpIntervalMs       = 200;    // 200ms min spacing after burst
		static constexpr uint32_t kIpBurstAllowance      = 3;      // allow up to 3 rapid tries before spacing
		static constexpr uint64_t kMaxBackoffMs          = 60000;  // 60s max temporary backoff (never permanent)
		static constexpr size_t   kMaxTrackedEntries     = 8192;

		static AuthRateLimiter& Instance();

		SRateLimitStatus CheckAllowed(std::string_view ip, std::string_view normalized_login, uint64_t now_ms = 0);
		void RecordFailure(std::string_view ip, std::string_view normalized_login, uint64_t now_ms = 0);
		void RecordSuccess(std::string_view ip, std::string_view normalized_login, uint64_t now_ms = 0);
		void ResetAll();
	};
} // namespace AuthSecurity

#endif // __INC_LIBTHECORE_AUTH_SECURITY_H__
