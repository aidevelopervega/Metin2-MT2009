#include "../include/auth_security.h"
#include "../include/argon2.h"

#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <unordered_map>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <bcrypt.h>
#else
#include <fcntl.h>
#include <unistd.h>
#endif

namespace AuthSecurity
{
namespace
{
	// Self-contained RFC 3174 SHA-1 for legacy MySQL 4.1 (*SHA1(SHA1(password))) verification
	struct SSha1Ctx
	{
		uint32_t state[5];
		uint64_t count;
		uint8_t  buffer[64];
	};

	inline uint32_t Sha1Rol(uint32_t value, unsigned int bits) noexcept
	{
		return (value << bits) | (value >> (32U - bits));
	}

	void Sha1Transform(uint32_t state[5], const uint8_t buffer[64]) noexcept
	{
		uint32_t w[80];
		for (int i = 0; i < 16; ++i)
		{
			w[i] = (static_cast<uint32_t>(buffer[i * 4 + 0]) << 24) |
			       (static_cast<uint32_t>(buffer[i * 4 + 1]) << 16) |
			       (static_cast<uint32_t>(buffer[i * 4 + 2]) << 8)  |
			       (static_cast<uint32_t>(buffer[i * 4 + 3]));
		}
		for (int i = 16; i < 80; ++i)
		{
			w[i] = Sha1Rol(w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16], 1);
		}

		uint32_t a = state[0];
		uint32_t b = state[1];
		uint32_t c = state[2];
		uint32_t d = state[3];
		uint32_t e = state[4];

		for (int i = 0; i < 80; ++i)
		{
			uint32_t f = 0;
			uint32_t k = 0;
			if (i < 20)
			{
				f = (b & c) | ((~b) & d);
				k = 0x5A827999U;
			}
			else if (i < 40)
			{
				f = b ^ c ^ d;
				k = 0x6ED9EBA1U;
			}
			else if (i < 60)
			{
				f = (b & c) | (b & d) | (c & d);
				k = 0x8F1BBCDCU;
			}
			else
			{
				f = b ^ c ^ d;
				k = 0xCA62C1D6U;
			}
			uint32_t temp = Sha1Rol(a, 5) + f + e + k + w[i];
			e = d;
			d = c;
			c = Sha1Rol(b, 30);
			b = a;
			a = temp;
		}

		state[0] += a;
		state[1] += b;
		state[2] += c;
		state[3] += d;
		state[4] += e;
		SecureZeroMemoryBuffer(w, sizeof(w));
	}

	void Sha1Compute(const uint8_t* data, size_t len, uint8_t digest[20]) noexcept
	{
		SSha1Ctx ctx{};
		ctx.state[0] = 0x67452301U;
		ctx.state[1] = 0xEFCDAB89U;
		ctx.state[2] = 0x98BADCFEU;
		ctx.state[3] = 0x10325476U;
		ctx.state[4] = 0xC3D2E1F0U;
		ctx.count    = static_cast<uint64_t>(len) * 8ULL;

		size_t offset = 0;
		while (len - offset >= 64)
		{
			Sha1Transform(ctx.state, data + offset);
			offset += 64;
		}

		size_t rem = len - offset;
		std::memset(ctx.buffer, 0, sizeof(ctx.buffer));
		if (rem > 0)
		{
			std::memcpy(ctx.buffer, data + offset, rem);
		}
		ctx.buffer[rem] = 0x80U;

		if (rem >= 56)
		{
			Sha1Transform(ctx.state, ctx.buffer);
			std::memset(ctx.buffer, 0, sizeof(ctx.buffer));
		}

		for (int i = 0; i < 8; ++i)
		{
			ctx.buffer[63 - i] = static_cast<uint8_t>((ctx.count >> (i * 8)) & 0xFFU);
		}
		Sha1Transform(ctx.state, ctx.buffer);

		for (int i = 0; i < 5; ++i)
		{
			digest[i * 4 + 0] = static_cast<uint8_t>((ctx.state[i] >> 24) & 0xFFU);
			digest[i * 4 + 1] = static_cast<uint8_t>((ctx.state[i] >> 16) & 0xFFU);
			digest[i * 4 + 2] = static_cast<uint8_t>((ctx.state[i] >> 8) & 0xFFU);
			digest[i * 4 + 3] = static_cast<uint8_t>(ctx.state[i] & 0xFFU);
		}
		SecureZeroMemoryBuffer(&ctx, sizeof(ctx));
	}

	inline bool IsHexChar(char c) noexcept
	{
		return (c >= '0' && c <= '9') ||
		       (c >= 'a' && c <= 'f') ||
		       (c >= 'A' && c <= 'F');
	}

	inline uint8_t NormalizeHexAscii(char c) noexcept
	{
		if (c >= 'a' && c <= 'f')
			return static_cast<uint8_t>(c - 'a' + 'A');
		return static_cast<uint8_t>(c);
	}

	inline bool IsPhcBase64Char(char c) noexcept
	{
		return (c >= 'A' && c <= 'Z') ||
		       (c >= 'a' && c <= 'z') ||
		       (c >= '0' && c <= '9') ||
		       c == '+' || c == '/';
	}

	bool ParseUint32Strict(std::string_view sv, uint32_t* out_val) noexcept
	{
		if (sv.empty() || sv.size() > 10)
			return false;
		if (sv.size() > 1 && sv[0] == '0')
			return false;
		uint64_t acc = 0;
		for (char c : sv)
		{
			if (c < '0' || c > '9')
				return false;
			acc = acc * 10ULL + static_cast<uint64_t>(c - '0');
			if (acc > 0xFFFFFFFFULL)
				return false;
		}
		*out_val = static_cast<uint32_t>(acc);
		return true;
	}

	std::atomic<uint32_t> g_inFlightAuthSlots{0};
	std::mutex            g_budgetMutex;
	uint64_t              g_budgetWindowStartMs = 0;
	uint32_t              g_budgetCountInWindow = 0;

	std::once_flag        g_dummyHashOnce;
	char                  g_dummyArgon2Hash[128] = {0};

	void EnsureDummyHashInitialized() noexcept
	{
		std::call_once(g_dummyHashOnce, []() {
			const uint8_t fixedSalt[kDefaultSaltBytes] = {
				0x4d, 0x54, 0x32, 0x30, 0x30, 0x39, 0x44, 0x75,
				0x6d, 0x6d, 0x79, 0x53, 0x61, 0x6c, 0x74, 0x21
			};
			const char dummyPwd[] = "__mt2009_dummy_anti_enum_pwd__";
			argon2id_hash_encoded(
				kDefaultTimeCost,
				kDefaultMemoryCostKiB,
				kDefaultParallelism,
				dummyPwd,
				sizeof(dummyPwd) - 1,
				fixedSalt,
				sizeof(fixedSalt),
				kDefaultTagBytes,
				g_dummyArgon2Hash,
				sizeof(g_dummyArgon2Hash));
		});
	}
} // anonymous namespace

uint64_t GetMonotonicMs() noexcept
{
	const auto now = std::chrono::steady_clock::now().time_since_epoch();
	return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(now).count());
}

uint64_t GetMonotonicUsec() noexcept
{
	const auto now = std::chrono::steady_clock::now().time_since_epoch();
	return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::microseconds>(now).count());
}

void SecureZeroMemoryBuffer(void* ptr, size_t len) noexcept
{
	if (!ptr || len == 0)
		return;
#ifdef _WIN32
	SecureZeroMemory(ptr, len);
#else
	volatile uint8_t* p = static_cast<volatile uint8_t*>(ptr);
	while (len--)
	{
		*p++ = 0;
	}
	__asm__ __volatile__("" : : "r"(ptr) : "memory");
#endif
}

bool GenerateCSPRNGBytes(void* buf, size_t len) noexcept
{
	if (!buf || len == 0)
		return false;
#ifdef _WIN32
	NTSTATUS st = BCryptGenRandom(
		nullptr,
		static_cast<PUCHAR>(buf),
		static_cast<ULONG>(len),
		BCRYPT_USE_SYSTEM_PREFERRED_RNG);
	return st == 0;
#elif defined(__FreeBSD__)
	arc4random_buf(buf, len);
	return true;
#else
	int fd = ::open("/dev/urandom", O_RDONLY);
	if (fd < 0)
		return false;
	uint8_t* out = static_cast<uint8_t*>(buf);
	size_t total = 0;
	while (total < len)
	{
		ssize_t r = ::read(fd, out + total, len - total);
		if (r <= 0)
		{
			::close(fd);
			return false;
		}
		total += static_cast<size_t>(r);
	}
	::close(fd);
	return true;
#endif
}

uint32_t GenerateSecureLoginKey() noexcept
{
	uint32_t key = 0;
	for (int attempt = 0; attempt < 8; ++attempt)
	{
		if (GenerateCSPRNGBytes(&key, sizeof(key)) && key != 0)
			return key;
	}
	// Fallback non-zero mixing if OS RNG were ever unavailable
	uint64_t t = GetMonotonicUsec();
	uint32_t fallback = static_cast<uint32_t>((t ^ (t >> 32)) * 2654435761ULL);
	return fallback != 0 ? fallback : 1U;
}

bool ConstantTimeEquals(const void* a, const void* b, size_t len) noexcept
{
	if (!a || !b)
		return false;
	const volatile uint8_t* pa = static_cast<const volatile uint8_t*>(a);
	const volatile uint8_t* pb = static_cast<const volatile uint8_t*>(b);
	uint8_t diff = 0;
	for (size_t i = 0; i < len; ++i)
	{
		diff |= static_cast<uint8_t>(pa[i] ^ pb[i]);
	}
	return diff == 0;
}

bool ConstantTimeHexEquals(std::string_view a, std::string_view b) noexcept
{
	if (a.size() != b.size() || a.empty())
		return false;
	uint8_t diff = 0;
	for (size_t i = 0; i < a.size(); ++i)
	{
		diff |= static_cast<uint8_t>(NormalizeHexAscii(a[i]) ^ NormalizeHexAscii(b[i]));
	}
	return diff == 0;
}

std::string ComputeLegacyMySQL41Hash(std::string_view password)
{
	if (password.empty())
		return {};

	uint8_t stage1[20];
	uint8_t stage2[20];
	Sha1Compute(reinterpret_cast<const uint8_t*>(password.data()), password.size(), stage1);
	Sha1Compute(stage1, sizeof(stage1), stage2);
	SecureZeroMemoryBuffer(stage1, sizeof(stage1));

	char out[42];
	out[0] = '*';
	static constexpr char kHexDigits[] = "0123456789ABCDEF";
	for (size_t i = 0; i < 20; ++i)
	{
		out[1 + i * 2]     = kHexDigits[(stage2[i] >> 4) & 0x0FU];
		out[1 + i * 2 + 1] = kHexDigits[stage2[i] & 0x0FU];
	}
	out[41] = '\0';
	SecureZeroMemoryBuffer(stage2, sizeof(stage2));
	return std::string(out, 41);
}

EHashFormat IdentifyPasswordHashFormat(std::string_view stored_hash, SArgon2Params* out_params) noexcept
{
	if (out_params)
		*out_params = SArgon2Params{};

	if (stored_hash.empty() || stored_hash.size() > kMaxStoredHashLength)
		return EHashFormat::Invalid;

	// 1. Check Legacy MySQL 4.1 format: '*' followed by 40 hex chars
	if (stored_hash.size() == 41 && stored_hash[0] == '*')
	{
		for (size_t i = 1; i < 41; ++i)
		{
			if (!IsHexChar(stored_hash[i]))
				return EHashFormat::Invalid;
		}
		return EHashFormat::LegacyMySQL41;
	}

	// 2. Check PHC Argon2id format: $argon2id$v=19$m=<m>,t=<t>,p=<p>$<salt_b64>$<tag_b64>
	static constexpr std::string_view kPrefix = "$argon2id$v=19$";
	if (stored_hash.size() < kPrefix.size() || stored_hash.substr(0, kPrefix.size()) != kPrefix)
		return EHashFormat::Invalid;

	std::string_view rest = stored_hash.substr(kPrefix.size());
	size_t paramsEnd = rest.find('$');
	if (paramsEnd == std::string_view::npos)
		return EHashFormat::Invalid;

	std::string_view paramsPart = rest.substr(0, paramsEnd);
	std::string_view saltAndTag = rest.substr(paramsEnd + 1);

	size_t saltEnd = saltAndTag.find('$');
	if (saltEnd == std::string_view::npos)
		return EHashFormat::Invalid;

	std::string_view saltB64 = saltAndTag.substr(0, saltEnd);
	std::string_view tagB64  = saltAndTag.substr(saltEnd + 1);

	// Ensure no additional '$' segments
	if (tagB64.find('$') != std::string_view::npos)
		return EHashFormat::Invalid;

	// Parse m=<m>,t=<t>,p=<p>
	size_t comma1 = paramsPart.find(',');
	if (comma1 == std::string_view::npos)
		return EHashFormat::Invalid;
	size_t comma2 = paramsPart.find(',', comma1 + 1);
	if (comma2 == std::string_view::npos || paramsPart.find(',', comma2 + 1) != std::string_view::npos)
		return EHashFormat::Invalid;

	std::string_view mSeg = paramsPart.substr(0, comma1);
	std::string_view tSeg = paramsPart.substr(comma1 + 1, comma2 - comma1 - 1);
	std::string_view pSeg = paramsPart.substr(comma2 + 1);

	if (mSeg.size() <= 2 || mSeg.substr(0, 2) != "m=" ||
	    tSeg.size() <= 2 || tSeg.substr(0, 2) != "t=" ||
	    pSeg.size() <= 2 || pSeg.substr(0, 2) != "p=")
	{
		return EHashFormat::Invalid;
	}

	uint32_t m_cost = 0, t_cost = 0, lanes = 0;
	if (!ParseUint32Strict(mSeg.substr(2), &m_cost) ||
	    !ParseUint32Strict(tSeg.substr(2), &t_cost) ||
	    !ParseUint32Strict(pSeg.substr(2), &lanes))
	{
		return EHashFormat::Invalid;
	}

	// Enforce strict resource bounds
	if (m_cost < kMinAllowedMemoryKiB || m_cost > kMaxAllowedMemoryKiB ||
	    t_cost < kMinAllowedTimeCost  || t_cost > kMaxAllowedTimeCost  ||
	    lanes  < kMinAllowedParallelism || lanes > kMaxAllowedParallelism)
	{
		return EHashFormat::Invalid;
	}

	// Base64 unpadded length bounds:
	// 16..32 raw salt bytes -> 22..43 base64 chars
	// 32..64 raw tag bytes  -> 43..86 base64 chars
	if (saltB64.size() < 22 || saltB64.size() > 43 ||
	    tagB64.size()  < 43 || tagB64.size()  > 86)
	{
		return EHashFormat::Invalid;
	}

	for (char c : saltB64)
	{
		if (!IsPhcBase64Char(c))
			return EHashFormat::Invalid;
	}
	for (char c : tagB64)
	{
		if (!IsPhcBase64Char(c))
			return EHashFormat::Invalid;
	}

	if (out_params)
	{
		out_params->m_cost_kib = m_cost;
		out_params->t_cost     = t_cost;
		out_params->lanes      = lanes;
		out_params->salt_len   = (saltB64.size() * 3) / 4;
		out_params->tag_len    = (tagB64.size() * 3) / 4;
	}
	return EHashFormat::Argon2id;
}

std::string HashPasswordArgon2id(
	std::string_view password,
	uint32_t m_cost_kib,
	uint32_t t_cost,
	uint32_t lanes,
	uint64_t* out_elapsed_usec)
{
	if (out_elapsed_usec)
		*out_elapsed_usec = 0;

	if (password.empty() || password.size() > 128)
		return {};

	if (m_cost_kib < kMinAllowedMemoryKiB || m_cost_kib > kMaxAllowedMemoryKiB ||
	    t_cost < kMinAllowedTimeCost || t_cost > kMaxAllowedTimeCost ||
	    lanes < kMinAllowedParallelism || lanes > kMaxAllowedParallelism)
	{
		return {};
	}

	uint8_t salt[kDefaultSaltBytes];
	if (!GenerateCSPRNGBytes(salt, sizeof(salt)))
	{
		SecureZeroMemoryBuffer(salt, sizeof(salt));
		return {};
	}

	size_t encodedLen = argon2_encodedlen(
		t_cost,
		m_cost_kib,
		lanes,
		static_cast<uint32_t>(sizeof(salt)),
		static_cast<uint32_t>(kDefaultTagBytes),
		Argon2_id);

	if (encodedLen == 0 || encodedLen > kMaxStoredHashLength)
	{
		SecureZeroMemoryBuffer(salt, sizeof(salt));
		return {};
	}

	char encoded[kMaxStoredHashLength + 1] = {0};
	uint64_t startUsec = GetMonotonicUsec();
	int rc = argon2id_hash_encoded(
		t_cost,
		m_cost_kib,
		lanes,
		password.data(),
		password.size(),
		salt,
		sizeof(salt),
		kDefaultTagBytes,
		encoded,
		sizeof(encoded));
	uint64_t elapsedUsec = GetMonotonicUsec() - startUsec;
	SecureZeroMemoryBuffer(salt, sizeof(salt));

	if (out_elapsed_usec)
		*out_elapsed_usec = elapsedUsec;

	if (rc != ARGON2_OK)
		return {};

	return std::string(encoded);
}

SVerifyResult VerifyPassword(std::string_view password, std::string_view stored_hash)
{
	SVerifyResult result{};
	uint64_t startUsec = GetMonotonicUsec();

	if (password.empty() || password.size() > 128)
	{
		result.elapsed_usec = GetMonotonicUsec() - startUsec;
		return result;
	}

	SArgon2Params params{};
	EHashFormat fmt = IdentifyPasswordHashFormat(stored_hash, &params);
	result.format = fmt;

	if (fmt == EHashFormat::Argon2id)
	{
		// NEVER fall back to legacy SHA1 if the stored hash is Argon2id
		char hashBuf[kMaxStoredHashLength + 1];
		std::memcpy(hashBuf, stored_hash.data(), stored_hash.size());
		hashBuf[stored_hash.size()] = '\0';

		int rc = argon2id_verify(hashBuf, password.data(), password.size());
		SecureZeroMemoryBuffer(hashBuf, sizeof(hashBuf));

		result.matched = (rc == ARGON2_OK);
		result.needs_upgrade = result.matched &&
		                       (params.m_cost_kib < kDefaultMemoryCostKiB ||
		                        params.t_cost < kDefaultTimeCost);
		result.elapsed_usec = GetMonotonicUsec() - startUsec;
		return result;
	}

	if (fmt == EHashFormat::LegacyMySQL41)
	{
		std::string candidate = ComputeLegacyMySQL41Hash(password);
		bool matched = ConstantTimeHexEquals(candidate, stored_hash);
		if (!candidate.empty())
		{
			SecureZeroMemoryBuffer(candidate.data(), candidate.size());
		}
		result.matched       = matched;
		result.needs_upgrade = matched;
		result.elapsed_usec  = GetMonotonicUsec() - startUsec;
		return result;
	}

	// EHashFormat::Invalid -> fail closed immediately
	result.elapsed_usec = GetMonotonicUsec() - startUsec;
	return result;
}

void VerifyDummyPassword(std::string_view password) noexcept
{
	EnsureDummyHashInitialized();
	if (g_dummyArgon2Hash[0] == '\0')
		return;
	const char* pwdPtr = password.empty() ? "x" : password.data();
	size_t pwdLen      = password.empty() ? 1 : (password.size() > 64 ? 64 : password.size());
	(void)argon2id_verify(g_dummyArgon2Hash, pwdPtr, pwdLen);
}

bool IsFieldNullTerminated(const char* field, size_t buf_capacity, size_t min_len, size_t max_len) noexcept
{
	if (!field || buf_capacity == 0 || min_len > max_len || max_len >= buf_capacity)
		return false;

	const void* nulPos = std::memchr(field, '\0', buf_capacity);
	if (!nulPos)
		return false;

	size_t len = static_cast<size_t>(static_cast<const char*>(nulPos) - field);
	return len >= min_len && len <= max_len;
}

bool IsValidAccountLogin(std::string_view login) noexcept
{
	if (login.size() < kMinLoginLength || login.size() > kMaxLoginLength)
		return false;

	for (char c : login)
	{
		bool ok = (c >= 'a' && c <= 'z') ||
		          (c >= 'A' && c <= 'Z') ||
		          (c >= '0' && c <= '9') ||
		          c == '_';
		if (!ok)
			return false;
	}
	return true;
}

bool IsValidPlaintextPassword(std::string_view password) noexcept
{
	if (password.size() < kMinPasswordLength || password.size() > kMaxPasswordLength)
		return false;

	for (char c : password)
	{
		const unsigned char uc = static_cast<unsigned char>(c);
		if (uc < 0x20 || uc == 0x7F)
			return false;
	}
	return true;
}

bool IsValidSocialId(std::string_view social_id) noexcept
{
	if (social_id.size() < kMinSocialIdLength || social_id.size() > kMaxSocialIdLength)
		return false;

	for (char c : social_id)
	{
		if (c < '0' || c > '9')
			return false;
	}
	return true;
}

bool PrepareNewAccountCredentials(
	std::string_view login,
	std::string_view password,
	std::string_view social_id,
	SNewAccountCredentials* out_creds,
	std::string* out_error)
{
	if (!out_creds)
	{
		if (out_error)
			*out_error = "out_creds pointer is null";
		return false;
	}

	out_creds->login.clear();
	out_creds->argon2id_hash.clear();
	out_creds->social_id.clear();

	if (!IsValidAccountLogin(login))
	{
		if (out_error)
			*out_error = "invalid login (must be 2..30 ASCII alphanumeric/underscore characters)";
		return false;
	}

	if (!IsValidPlaintextPassword(password))
	{
		if (out_error)
			*out_error = "invalid password length or characters (must be 6..16 printable characters for 32-bit client protocol)";
		return false;
	}

	std::string_view effectiveSocialId = social_id.empty() ? std::string_view(kDefaultSocialId) : social_id;
	if (!IsValidSocialId(effectiveSocialId))
	{
		if (out_error)
			*out_error = "invalid social_id (must be 7..18 ASCII digits)";
		return false;
	}

	std::string phc = HashPasswordArgon2id(password);
	if (phc.empty() || IdentifyPasswordHashFormat(phc) != EHashFormat::Argon2id)
	{
		if (out_error)
			*out_error = "failed to generate Argon2id PHC password hash";
		return false;
	}

	out_creds->login.assign(login.data(), login.size());
	out_creds->argon2id_hash = std::move(phc);
	out_creds->social_id.assign(effectiveSocialId.data(), effectiveSocialId.size());
	return true;
}

bool TryAcquireInFlightAuthSlot() noexcept
{
	uint32_t cur = g_inFlightAuthSlots.load(std::memory_order_relaxed);
	while (cur < kMaxInFlightAuthQueries)
	{
		if (g_inFlightAuthSlots.compare_exchange_weak(
			cur, cur + 1, std::memory_order_acquire, std::memory_order_relaxed))
		{
			return true;
		}
	}
	return false;
}

void ReleaseInFlightAuthSlot() noexcept
{
	uint32_t cur = g_inFlightAuthSlots.load(std::memory_order_relaxed);
	while (cur > 0)
	{
		if (g_inFlightAuthSlots.compare_exchange_weak(
			cur, cur - 1, std::memory_order_release, std::memory_order_relaxed))
		{
			return;
		}
	}
}

uint32_t GetInFlightAuthSlotCount() noexcept
{
	return g_inFlightAuthSlots.load(std::memory_order_relaxed);
}

bool TryAcquireArgon2Budget(uint64_t now_ms) noexcept
{
	if (now_ms == 0)
		now_ms = GetMonotonicMs();

	std::lock_guard<std::mutex> lock(g_budgetMutex);
	if (now_ms < g_budgetWindowStartMs || (now_ms - g_budgetWindowStartMs) >= 1000ULL)
	{
		g_budgetWindowStartMs = now_ms;
		g_budgetCountInWindow = 1;
		return true;
	}

	if (g_budgetCountInWindow >= kMaxArgon2OpsPerSecond)
		return false;

	++g_budgetCountInWindow;
	return true;
}

void ResetArgon2BudgetForTesting() noexcept
{
	std::lock_guard<std::mutex> lock(g_budgetMutex);
	g_budgetWindowStartMs = 0;
	g_budgetCountInWindow = 0;
	g_inFlightAuthSlots.store(0, std::memory_order_relaxed);
}

namespace
{
	struct SRateBucket
	{
		uint32_t failures          = 0;
		uint32_t rapid_attempts    = 0;
		uint64_t first_failure_ms  = 0;
		uint64_t last_attempt_ms   = 0;
		uint64_t backoff_until_ms  = 0;
	};

	std::mutex                                    g_rateMutex;
	std::unordered_map<std::string, SRateBucket>  g_ipBuckets;
	std::unordered_map<std::string, SRateBucket>  g_accountBuckets;

	uint64_t ComputeBackoffDurationMs(uint32_t failures) noexcept
	{
		if (failures < AuthRateLimiter::kFailureThreshold)
			return 0;
		uint32_t over = failures - AuthRateLimiter::kFailureThreshold;
		// 2s, 4s, 8s, 16s, 32s, capped at 60s
		uint64_t backoff = 2000ULL << (over > 5 ? 5 : over);
		if (backoff > AuthRateLimiter::kMaxBackoffMs)
			backoff = AuthRateLimiter::kMaxBackoffMs;
		return backoff;
	}

	void PruneMapIfFull(std::unordered_map<std::string, SRateBucket>& map, uint64_t now_ms)
	{
		if (map.size() < AuthRateLimiter::kMaxTrackedEntries)
			return;

		for (auto it = map.begin(); it != map.end(); )
		{
			bool expired = (now_ms > it->second.last_attempt_ms) &&
			               ((now_ms - it->second.last_attempt_ms) > AuthRateLimiter::kFailureWindowMs) &&
			               (now_ms >= it->second.backoff_until_ms);
			if (expired)
				it = map.erase(it);
			else
				++it;
		}

		if (map.size() >= AuthRateLimiter::kMaxTrackedEntries)
		{
			map.erase(map.begin());
		}
	}
} // anonymous namespace

AuthRateLimiter& AuthRateLimiter::Instance()
{
	static AuthRateLimiter s_instance;
	return s_instance;
}

SRateLimitStatus AuthRateLimiter::CheckAllowed(
	std::string_view ip,
	std::string_view normalized_login,
	uint64_t now_ms)
{
	if (now_ms == 0)
		now_ms = GetMonotonicMs();

	std::lock_guard<std::mutex> lock(g_rateMutex);
	SRateLimitStatus status{};

	if (!ip.empty())
	{
		PruneMapIfFull(g_ipBuckets, now_ms);
		SRateBucket& ipB = g_ipBuckets[std::string(ip)];

		// Reset failure window if stale
		if (ipB.failures > 0 && now_ms > ipB.first_failure_ms &&
		    (now_ms - ipB.first_failure_ms) > kFailureWindowMs &&
		    now_ms >= ipB.backoff_until_ms)
		{
			ipB.failures = 0;
			ipB.first_failure_ms = 0;
			ipB.backoff_until_ms = 0;
		}

		if (now_ms < ipB.backoff_until_ms)
		{
			status.decision       = ERateLimitDecision::IpBackoffActive;
			status.retry_after_ms = ipB.backoff_until_ms - now_ms;
			status.ip_failures    = ipB.failures;
			return status;
		}

		if (ipB.last_attempt_ms > 0 && now_ms >= ipB.last_attempt_ms &&
		    (now_ms - ipB.last_attempt_ms) < kMinIpIntervalMs)
		{
			++ipB.rapid_attempts;
			if (ipB.rapid_attempts >= kIpBurstAllowance)
			{
				status.decision       = ERateLimitDecision::IpBurstThrottled;
				status.retry_after_ms = kMinIpIntervalMs - (now_ms - ipB.last_attempt_ms);
				status.ip_failures    = ipB.failures;
				return status;
			}
		}
		else
		{
			ipB.rapid_attempts = 0;
		}

		ipB.last_attempt_ms = now_ms;
		status.ip_failures  = ipB.failures;
	}

	if (!normalized_login.empty())
	{
		PruneMapIfFull(g_accountBuckets, now_ms);
		SRateBucket& acctB = g_accountBuckets[std::string(normalized_login)];

		if (acctB.failures > 0 && now_ms > acctB.first_failure_ms &&
		    (now_ms - acctB.first_failure_ms) > kFailureWindowMs &&
		    now_ms >= acctB.backoff_until_ms)
		{
			acctB.failures = 0;
			acctB.first_failure_ms = 0;
			acctB.backoff_until_ms = 0;
		}

		if (now_ms < acctB.backoff_until_ms)
		{
			status.decision       = ERateLimitDecision::AccountBackoffActive;
			status.retry_after_ms = acctB.backoff_until_ms - now_ms;
			status.acct_failures  = acctB.failures;
			return status;
		}

		acctB.last_attempt_ms = now_ms;
		status.acct_failures  = acctB.failures;
	}

	status.decision = ERateLimitDecision::Allowed;
	return status;
}

void AuthRateLimiter::RecordFailure(
	std::string_view ip,
	std::string_view normalized_login,
	uint64_t now_ms)
{
	if (now_ms == 0)
		now_ms = GetMonotonicMs();

	std::lock_guard<std::mutex> lock(g_rateMutex);

	if (!ip.empty())
	{
		PruneMapIfFull(g_ipBuckets, now_ms);
		SRateBucket& ipB = g_ipBuckets[std::string(ip)];
		if (ipB.failures == 0 || (now_ms > ipB.first_failure_ms && (now_ms - ipB.first_failure_ms) > kFailureWindowMs))
		{
			ipB.failures = 1;
			ipB.first_failure_ms = now_ms;
		}
		else
		{
			++ipB.failures;
		}
		ipB.last_attempt_ms = now_ms;
		uint64_t backoff = ComputeBackoffDurationMs(ipB.failures);
		if (backoff > 0)
			ipB.backoff_until_ms = now_ms + backoff;
	}

	if (!normalized_login.empty())
	{
		PruneMapIfFull(g_accountBuckets, now_ms);
		SRateBucket& acctB = g_accountBuckets[std::string(normalized_login)];
		if (acctB.failures == 0 || (now_ms > acctB.first_failure_ms && (now_ms - acctB.first_failure_ms) > kFailureWindowMs))
		{
			acctB.failures = 1;
			acctB.first_failure_ms = now_ms;
		}
		else
		{
			++acctB.failures;
		}
		acctB.last_attempt_ms = now_ms;
		uint64_t backoff = ComputeBackoffDurationMs(acctB.failures);
		if (backoff > 0)
			acctB.backoff_until_ms = now_ms + backoff;
	}
}

void AuthRateLimiter::RecordSuccess(
	std::string_view ip,
	std::string_view normalized_login,
	uint64_t now_ms)
{
	if (now_ms == 0)
		now_ms = GetMonotonicMs();

	std::lock_guard<std::mutex> lock(g_rateMutex);

	if (!normalized_login.empty())
	{
		g_accountBuckets.erase(std::string(normalized_login));
	}
	if (!ip.empty())
	{
		auto it = g_ipBuckets.find(std::string(ip));
		if (it != g_ipBuckets.end())
		{
			it->second.failures         = 0;
			it->second.first_failure_ms = 0;
			it->second.backoff_until_ms = 0;
			it->second.last_attempt_ms  = now_ms;
		}
	}
}

void AuthRateLimiter::ResetAll()
{
	std::lock_guard<std::mutex> lock(g_rateMutex);
	g_ipBuckets.clear();
	g_accountBuckets.clear();
}

} // namespace AuthSecurity
