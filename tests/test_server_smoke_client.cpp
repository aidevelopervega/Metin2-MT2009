#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <cstring>
#include <string>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
typedef SOCKET sock_t;
#define CLOSESOCK closesocket
#else
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
typedef int sock_t;
#define INVALID_SOCKET (-1)
#define CLOSESOCK close
#endif

#pragma pack(push, 1)
struct SmokePacketGCPhase {
	uint8_t header; // 0xFD (HEADER_GC_PHASE)
	uint8_t phase;  // 1 = PHASE_HANDSHAKE, 2 = PHASE_LOGIN, 3 = PHASE_SELECT, 10 = PHASE_AUTH
};

struct SmokePacketHandshake {
	uint8_t  bHeader;     // 0xFF (HEADER_GC_HANDSHAKE / HEADER_CG_HANDSHAKE)
	uint32_t dwHandshake;
	uint32_t dwTime;
	int32_t  lDelta;
};

struct SmokePacketCGLogin2 {
	uint8_t  header;      // 109 (HEADER_CG_LOGIN2)
	char     login[31];   // LOGIN_MAX_LEN (30) + 1
	uint32_t dwLoginKey;
	uint32_t adwClientKey[4];
};

struct SmokePacketCGLogin3 {
	uint8_t  header;      // 111 (HEADER_CG_LOGIN3)
	char     login[31];   // LOGIN_MAX_LEN (30) + 1
	char     passwd[17];  // PASSWD_MAX_LEN (16) + 1
	uint32_t adwClientKey[4];
	char     hwid[256];   // HWID_MAX_LEN (255) + 1
};

struct SmokePacketGCAuthSuccess {
	uint8_t  bHeader;     // 150 (HEADER_GC_AUTH_SUCCESS)
	uint32_t dwLoginKey;
	uint8_t  bResult;
};

struct SmokePacketGCLoginFailure {
	uint8_t  header;      // 7 (HEADER_GC_LOGIN_FAILURE)
	char     szStatus[9]; // ACCOUNT_STATUS_MAX_LEN (8) + 1
};
#pragma pack(pop)

static_assert(sizeof(SmokePacketGCPhase) == 2, "SmokePacketGCPhase must be 2 bytes");
static_assert(sizeof(SmokePacketHandshake) == 13, "SmokePacketHandshake must be 13 bytes");
static_assert(sizeof(SmokePacketCGLogin2) == 52, "SmokePacketCGLogin2 must be 52 bytes");
static_assert(sizeof(SmokePacketCGLogin3) == 321, "SmokePacketCGLogin3 must be 321 bytes");
static_assert(sizeof(SmokePacketGCAuthSuccess) == 6, "SmokePacketGCAuthSuccess must be 6 bytes");
static_assert(sizeof(SmokePacketGCLoginFailure) == 10, "SmokePacketGCLoginFailure must be 10 bytes");

// ==============================================================================
// Metin2 32-Bit Wire TEA Cipher (matches server/src/libthecore/src/tea.c & desc.cpp)
// ==============================================================================
static constexpr uint32_t TEA_DELTA = 0x9E3779B9u;
static constexpr uint32_t TEA_ROUNDS = 32u;

static void tea_code(uint32_t sz, uint32_t sy, const uint32_t* key, uint32_t* dest) {
	uint32_t y = sy, z = sz, sum = 0;
	for (uint32_t r = 0; r < TEA_ROUNDS; ++r) {
		y += (((z << 4) ^ (z >> 5)) + z) ^ (sum + key[sum & 3]);
		sum += TEA_DELTA;
		z += (((y << 4) ^ (y >> 5)) + y) ^ (sum + key[(sum >> 11) & 3]);
	}
	dest[0] = y;
	dest[1] = z;
}

static void tea_decode(uint32_t sz, uint32_t sy, const uint32_t* key, uint32_t* dest) {
	uint32_t y = sy, z = sz, sum = TEA_DELTA * TEA_ROUNDS;
	for (uint32_t r = 0; r < TEA_ROUNDS; ++r) {
		z -= (((y << 4) ^ (y >> 5)) + y) ^ (sum + key[(sum >> 11) & 3]);
		sum -= TEA_DELTA;
		y -= (((z << 4) ^ (z >> 5)) + z) ^ (sum + key[sum & 3]);
	}
	dest[0] = y;
	dest[1] = z;
}

static int smoke_tea_encrypt(uint32_t* dest, const uint32_t* src, const uint32_t* key, int size) {
	int resize = (size % 8 != 0) ? (size + 8 - (size % 8)) : size;
	for (int i = 0; i < (resize >> 3); ++i, dest += 2, src += 2)
		tea_code(*(src + 1), *src, key, dest);
	return resize;
}

static int smoke_tea_decrypt(uint32_t* dest, const uint32_t* src, const uint32_t* key, int size) {
	int resize = (size % 8 != 0) ? (size + 8 - (size % 8)) : size;
	for (int i = 0; i < (resize >> 3); ++i, dest += 2, src += 2)
		tea_decode(*(src + 1), *src, key, dest);
	return resize;
}

static const uint8_t* get_key_20050304_myevan() {
	static bool bGenerated = false;
	static uint32_t s_adwKey[1938];
	if (!bGenerated) {
		bGenerated = true;
		uint32_t seed = 1491971513u;
		for (uint32_t i = 0; i < static_cast<uint8_t>(seed); ++i) {
			seed ^= 2148941891u;
			seed += 3592385981u;
			s_adwKey[i] = seed;
		}
	}
	return reinterpret_cast<const uint8_t*>(s_adwKey);
}

static void get_initial_tea_key(uint32_t out_key[4]) {
	std::memcpy(out_key, "9382ujfn8237gydb", 16);
}

static void compute_security_tea_key(const uint32_t client_key[4], uint32_t out_server_enc_key[4]) {
	const uint8_t* c_pszKey = get_key_20050304_myevan() + 37;
	uint32_t aligned_key[4]{};
	std::memcpy(aligned_key, c_pszKey, 16);
	smoke_tea_encrypt(out_server_enc_key, client_key, aligned_key, 16);
}

static void sleep_ms(int ms) {
#ifdef _WIN32
	Sleep(static_cast<DWORD>(ms));
#else
	usleep(static_cast<useconds_t>(ms) * 1000);
#endif
}

static bool recv_exact_timeout(sock_t s, uint8_t* buf, size_t len, int timeout_sec) {
	size_t got = 0;
	while (got < len) {
		fd_set rfds;
		FD_ZERO(&rfds);
		FD_SET(s, &rfds);
		struct timeval tv;
		tv.tv_sec = timeout_sec;
		tv.tv_usec = 0;
		int r = select(static_cast<int>(s + 1), &rfds, nullptr, nullptr, &tv);
		if (r <= 0)
			return false;
		int n = recv(s, reinterpret_cast<char*>(buf + got), static_cast<int>(len - got), 0);
		if (n <= 0)
			return false;
		got += static_cast<size_t>(n);
	}
	return true;
}

static int run_mock_db(int port, const char* ready_file, const char* out_file) {
	sock_t srv = socket(AF_INET, SOCK_STREAM, 0);
	if (srv == INVALID_SOCKET) {
		std::fprintf(stderr, "[mock-db] socket() failed\n");
		return 1;
	}
	int opt = 1;
	setsockopt(srv, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&opt), sizeof(opt));

	struct sockaddr_in addr{};
	addr.sin_family = AF_INET;
	addr.sin_port = htons(static_cast<uint16_t>(port));
	addr.sin_addr.s_addr = inet_addr("127.0.0.1");

	if (bind(srv, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) != 0 ||
		listen(srv, 4) != 0) {
		std::fprintf(stderr, "[mock-db] bind/listen failed on 127.0.0.1:%d\n", port);
		CLOSESOCK(srv);
		return 1;
	}

	if (FILE* fp = std::fopen(ready_file, "w")) {
		std::fprintf(fp, "READY\n");
		std::fclose(fp);
	}

	fd_set rfds;
	FD_ZERO(&rfds);
	FD_SET(srv, &rfds);
	struct timeval tv{12, 0};
	if (select(static_cast<int>(srv + 1), &rfds, nullptr, nullptr, &tv) <= 0) {
		std::fprintf(stderr, "[mock-db] timeout waiting for game db_clientdesc connection\n");
		CLOSESOCK(srv);
		return 1;
	}

	struct sockaddr_in cli_addr{};
#ifdef _WIN32
	int cli_len = sizeof(cli_addr);
#else
	socklen_t cli_len = sizeof(cli_addr);
#endif
	sock_t cli = accept(srv, reinterpret_cast<struct sockaddr*>(&cli_addr), &cli_len);
	if (cli == INVALID_SOCKET) {
		std::fprintf(stderr, "[mock-db] accept() failed\n");
		CLOSESOCK(srv);
		return 1;
	}

	size_t total_bytes = 0;
	uint8_t first_header = 0;
	uint8_t buf[4096];

	for (int iter = 0; iter < 30; ++iter) {
		FD_ZERO(&rfds);
		FD_SET(cli, &rfds);
		struct timeval rtv{0, 100000};
		int sel = select(static_cast<int>(cli + 1), &rfds, nullptr, nullptr, &rtv);
		if (sel > 0) {
			int n = recv(cli, reinterpret_cast<char*>(buf), sizeof(buf), 0);
			if (n <= 0)
				break;
			if (total_bytes == 0 && n > 0)
				first_header = buf[0];
			total_bytes += static_cast<size_t>(n);
		} else if (total_bytes > 0) {
			break;
		}
	}

	if (FILE* fp = std::fopen(out_file, "w")) {
		std::fprintf(fp, "CONNECTED=1\nBYTES=%zu\nFIRST_HEADER=%u\n",
			total_bytes, static_cast<unsigned>(first_header));
		std::fclose(fp);
	}

	for (int iter = 0; iter < 60; ++iter) {
		FD_ZERO(&rfds);
		FD_SET(cli, &rfds);
		struct timeval rtv{0, 100000};
		int sel = select(static_cast<int>(cli + 1), &rfds, nullptr, nullptr, &rtv);
		if (sel > 0) {
			int n = recv(cli, reinterpret_cast<char*>(buf), sizeof(buf), 0);
			if (n <= 0)
				break;
		}
	}

	CLOSESOCK(cli);
	CLOSESOCK(srv);
	return (total_bytes > 0) ? 0 : 1;
}

static sock_t connect_with_retry(int port, int max_attempts) {
	for (int attempt = 0; attempt < max_attempts; ++attempt) {
		sock_t s = socket(AF_INET, SOCK_STREAM, 0);
		if (s == INVALID_SOCKET)
			return INVALID_SOCKET;

		struct sockaddr_in addr{};
		addr.sin_family = AF_INET;
		addr.sin_port = htons(static_cast<uint16_t>(port));
		addr.sin_addr.s_addr = inet_addr("127.0.0.1");

		if (connect(s, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) == 0)
			return s;

		CLOSESOCK(s);
		sleep_ms(100);
	}
	return INVALID_SOCKET;
}

// Completes the unencrypted 32-bit wire handshake (PHASE_HANDSHAKE -> expected_phase).
// Handles multi-round TPacketGCHandshake(0xFF) time-sync challenges until TPacketGCPhase(0xFD, expected_phase) arrives.
static bool complete_wire_handshake(sock_t s, uint8_t expected_phase, const char* tag) {
	SmokePacketGCPhase phasePkt{};
	SmokePacketHandshake hsPkt{};
	uint8_t initBuf[15]{};
	if (!recv_exact_timeout(s, initBuf, sizeof(initBuf), 5)) {
		std::fprintf(stderr, "[%s] FAIL: timeout reading initial 15-byte phase+handshake packet\n", tag);
		return false;
	}

	std::memcpy(&phasePkt, initBuf, sizeof(phasePkt));
	std::memcpy(&hsPkt, initBuf + sizeof(phasePkt), sizeof(hsPkt));

	if (phasePkt.header != 0xFD || phasePkt.phase != 1) {
		std::fprintf(stderr, "[%s] FAIL: expected TPacketGCPhase(0xFD, 1), got (0x%02X, %u)\n",
			tag, phasePkt.header, phasePkt.phase);
		return false;
	}

	if (hsPkt.bHeader != 0xFF || hsPkt.dwHandshake == 0 || hsPkt.lDelta != 0) {
		std::fprintf(stderr, "[%s] FAIL: invalid initial TPacketGCHandshake header=0x%02X handshake=%u time=%u delta=%d\n",
			tag, hsPkt.bHeader, hsPkt.dwHandshake, hsPkt.dwTime, hsPkt.lDelta);
		return false;
	}

	std::printf("  [PASS] [%s] Received 32-bit wire TPacketGCPhase(0xFD, PHASE_HANDSHAKE=1) + TPacketGCHandshake(0xFF, id=%u, time=%u, delta=%d)\n",
		tag, hsPkt.dwHandshake, hsPkt.dwTime, hsPkt.lDelta);

	for (int round = 1; round <= 16; ++round) {
		SmokePacketHandshake reply{};
		reply.bHeader = 0xFF;
		reply.dwHandshake = hsPkt.dwHandshake;
		// Advance dwTime + lDelta slightly ahead to match the next 25-FPS server heart pulse (40ms) so bias is within [0, 50]
		reply.dwTime = hsPkt.dwTime + (hsPkt.lDelta > 0 ? static_cast<uint32_t>(hsPkt.lDelta) : 0u);
		reply.lDelta = 25;

		if (send(s, reinterpret_cast<const char*>(&reply), sizeof(reply), 0) != sizeof(reply)) {
			std::fprintf(stderr, "[%s] FAIL: could not send TPacketCGHandshake reply (round %d)\n", tag, round);
			return false;
		}

		uint8_t nextHeader = 0;
		if (!recv_exact_timeout(s, &nextHeader, 1, 5)) {
			std::fprintf(stderr, "[%s] FAIL: timeout reading post-handshake header (round %d)\n", tag, round);
			return false;
		}

		if (nextHeader == 0xFD) {
			uint8_t newPhase = 0;
			if (!recv_exact_timeout(s, &newPhase, 1, 5)) {
				std::fprintf(stderr, "[%s] FAIL: timeout reading TPacketGCPhase phase byte\n", tag);
				return false;
			}
			if (newPhase != expected_phase) {
				std::fprintf(stderr, "[%s] FAIL: expected phase %u after handshake, got %u\n",
					tag, static_cast<unsigned>(expected_phase), static_cast<unsigned>(newPhase));
				return false;
			}
			std::printf("  [PASS] [%s] Handshake completed in %d round(s) -> TPacketGCPhase(0xFD, phase=%u)\n",
				tag, round, static_cast<unsigned>(newPhase));
			return true;
		} else if (nextHeader == 0xFF) {
			uint8_t rem[12]{};
			if (!recv_exact_timeout(s, rem, sizeof(rem), 5)) {
				std::fprintf(stderr, "[%s] FAIL: timeout reading retry TPacketGCHandshake body\n", tag);
				return false;
			}
			hsPkt.bHeader = 0xFF;
			std::memcpy(&hsPkt.dwHandshake, rem, sizeof(rem));
			continue;
		} else {
			std::fprintf(stderr, "[%s] FAIL: unexpected header 0x%02X during handshake (round %d)\n",
				tag, static_cast<unsigned>(nextHeader), round);
			return false;
		}
	}

	std::fprintf(stderr, "[%s] FAIL: handshake did not converge within 16 rounds\n", tag);
	return false;
}

static int run_probe_game(int port) {
	sock_t s = connect_with_retry(port, 60);
	if (s == INVALID_SOCKET) {
		std::fprintf(stderr, "[probe-game] FAIL: could not connect to 127.0.0.1:%d\n", port);
		return 1;
	}
	bool ok = complete_wire_handshake(s, 2 /* PHASE_LOGIN */, "probe-game");
	CLOSESOCK(s);
	return ok ? 0 : 1;
}

static void sha1_compute(const uint8_t* data, size_t len, uint8_t digest[20]) {
	auto rol32 = [](uint32_t v, unsigned int b) -> uint32_t { return (v << b) | (v >> (32 - b)); };
	uint32_t h0 = 0x67452301u, h1 = 0xEFCDAB89u, h2 = 0x98BADCFEu, h3 = 0x10325476u, h4 = 0xC3D2E1F0u;
	size_t padded_len = ((len + 8) / 64 + 1) * 64;
	uint8_t block[128]{};
	std::memcpy(block, data, len);
	block[len] = 0x80u;
	uint64_t bit_len = static_cast<uint64_t>(len) * 8ULL;
	for (int i = 0; i < 8; ++i)
		block[padded_len - 1 - i] = static_cast<uint8_t>((bit_len >> (i * 8)) & 0xFFu);

	for (size_t chunk = 0; chunk < padded_len; chunk += 64) {
		uint32_t w[80]{};
		for (int i = 0; i < 16; ++i) {
			w[i] = (static_cast<uint32_t>(block[chunk + i * 4]) << 24) |
			       (static_cast<uint32_t>(block[chunk + i * 4 + 1]) << 16) |
			       (static_cast<uint32_t>(block[chunk + i * 4 + 2]) << 8) |
			       static_cast<uint32_t>(block[chunk + i * 4 + 3]);
		}
		for (int i = 16; i < 80; ++i)
			w[i] = rol32(w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16], 1);

		uint32_t a = h0, b = h1, c = h2, d = h3, e = h4;
		for (int i = 0; i < 80; ++i) {
			uint32_t f = 0, k = 0;
			if (i < 20) { f = (b & c) | ((~b) & d); k = 0x5A827999u; }
			else if (i < 40) { f = b ^ c ^ d; k = 0x6ED9EBA1u; }
			else if (i < 60) { f = (b & c) | (b & d) | (c & d); k = 0x8F1BBCDCu; }
			else { f = b ^ c ^ d; k = 0xCA62C1D6u; }
			uint32_t temp = rol32(a, 5) + f + e + k + w[i];
			e = d; d = c; c = rol32(b, 30); b = a; a = temp;
		}
		h0 += a; h1 += b; h2 += c; h3 += d; h4 += e;
	}
	uint32_t hs[5] = { h0, h1, h2, h3, h4 };
	for (int i = 0; i < 5; ++i) {
		digest[i * 4]     = static_cast<uint8_t>((hs[i] >> 24) & 0xFFu);
		digest[i * 4 + 1] = static_cast<uint8_t>((hs[i] >> 16) & 0xFFu);
		digest[i * 4 + 2] = static_cast<uint8_t>((hs[i] >> 8) & 0xFFu);
		digest[i * 4 + 3] = static_cast<uint8_t>(hs[i] & 0xFFu);
	}
}

static void compute_mt2_password_hash(const char* plain, char out_hash[42]) {
	uint8_t stage1[20]{};
	uint8_t stage2[20]{};
	sha1_compute(reinterpret_cast<const uint8_t*>(plain), std::strlen(plain), stage1);
	sha1_compute(stage1, 20, stage2);
	static const char hex[] = "0123456789ABCDEF";
	out_hash[0] = '*';
	for (int i = 0; i < 20; ++i) {
		out_hash[1 + i * 2]     = hex[(stage2[i] >> 4) & 0x0Fu];
		out_hash[1 + i * 2 + 1] = hex[stage2[i] & 0x0Fu];
	}
	out_hash[41] = '\0';
}

static int run_hash_env_pass(const char* out_file) {
	const char* pass = std::getenv("SMOKE_TEST_PASS");
	if (!pass || !*pass || std::strlen(pass) > 16) {
		std::fprintf(stderr, "[hash-env-pass] FAIL: SMOKE_TEST_PASS missing or exceeds 16 chars\n");
		return 1;
	}
	char hash[42]{};
	compute_mt2_password_hash(pass, hash);
	FILE* fp = std::fopen(out_file, "w");
	if (!fp) {
		std::fprintf(stderr, "[hash-env-pass] FAIL: cannot write hash file\n");
		return 1;
	}
	std::fprintf(fp, "%s\n", hash);
	std::fclose(fp);
	return 0;
}

static int run_auth_login(int port, const char* login_arg, const char* passwd_arg, const char* hwid_hex, const char* key_out_file) {
	const char* login = (std::strcmp(login_arg, "@ENV") == 0) ? std::getenv("SMOKE_TEST_LOGIN") : login_arg;
	const char* passwd = (std::strcmp(passwd_arg, "@ENV") == 0) ? std::getenv("SMOKE_TEST_PASS") : passwd_arg;
	if (!login || !*login || !passwd || !*passwd) {
		std::fprintf(stderr, "[auth-login] FAIL: missing login or password environment variable\n");
		return 1;
	}

	sock_t s = connect_with_retry(port, 60);
	if (s == INVALID_SOCKET) {
		std::fprintf(stderr, "[auth-login] FAIL: could not connect to AUTH_SERVER on 127.0.0.1:%d\n", port);
		return 1;
	}

	if (!complete_wire_handshake(s, 10 /* PHASE_AUTH */, "auth-login")) {
		CLOSESOCK(s);
		return 1;
	}

	SmokePacketCGLogin3 pkt{};
	pkt.header = 111; // HEADER_CG_LOGIN3
	std::snprintf(pkt.login, sizeof(pkt.login), "%s", login);
	std::snprintf(pkt.passwd, sizeof(pkt.passwd), "%s", passwd);
	pkt.adwClientKey[0] = 0x11223344u;
	pkt.adwClientKey[1] = 0x55667788u;
	pkt.adwClientKey[2] = 0x99AABBCCu;
	pkt.adwClientKey[3] = 0xDDEEFF00u;
	std::snprintf(pkt.hwid, sizeof(pkt.hwid), "%s", hwid_hex);

	// Pad 321-byte TPacketCGLogin3 to 328 bytes (multiple of 8) and TEA-encrypt with initial key "9382ujfn8237gydb"
	uint8_t plainBuf[328]{};
	uint8_t encBuf[328]{};
	std::memcpy(plainBuf, &pkt, sizeof(pkt));

	uint32_t initKey[4]{};
	get_initial_tea_key(initKey);
	int encLen = smoke_tea_encrypt(
		reinterpret_cast<uint32_t*>(encBuf),
		reinterpret_cast<const uint32_t*>(plainBuf),
		initKey,
		static_cast<int>(sizeof(pkt)));

	if (send(s, reinterpret_cast<const char*>(encBuf), encLen, 0) != encLen) {
		std::fprintf(stderr, "[auth-login] FAIL: could not send TEA-encrypted TPacketCGLogin3 (%d bytes)\n", encLen);
		CLOSESOCK(s);
		return 1;
	}

	// Read first 8-byte TEA block from AUTH_SERVER
	uint8_t respEnc[16]{};
	uint8_t respDec[16]{};
	if (!recv_exact_timeout(s, respEnc, 8, 8)) {
		std::fprintf(stderr, "[auth-login] FAIL: timeout waiting for TPacketGCAuthSuccess from AUTH_SERVER\n");
		CLOSESOCK(s);
		return 1;
	}
	smoke_tea_decrypt(reinterpret_cast<uint32_t*>(respDec), reinterpret_cast<const uint32_t*>(respEnc), initKey, 8);

	if (respDec[0] == 7 /* HEADER_GC_LOGIN_FAILURE */) {
		// Read second 8-byte block of 10-byte TPacketGCLoginFailure
		if (recv_exact_timeout(s, respEnc + 8, 8, 2)) {
			smoke_tea_decrypt(reinterpret_cast<uint32_t*>(respDec + 8), reinterpret_cast<const uint32_t*>(respEnc + 8), initKey, 8);
		}
		SmokePacketGCLoginFailure failPkt{};
		std::memcpy(&failPkt, respDec, sizeof(failPkt));
		failPkt.szStatus[sizeof(failPkt.szStatus) - 1] = '\0';
		std::fprintf(stderr, "[auth-login] FAIL: AUTH_SERVER returned HEADER_GC_LOGIN_FAILURE status='%s'\n", failPkt.szStatus);
		CLOSESOCK(s);
		return 1;
	}

	SmokePacketGCAuthSuccess authOk{};
	std::memcpy(&authOk, respDec, sizeof(authOk));
	if (authOk.bHeader != 150 || authOk.bResult != 1 || authOk.dwLoginKey == 0) {
		std::fprintf(stderr, "[auth-login] FAIL: unexpected auth response header=%u key=%u result=%u\n",
			static_cast<unsigned>(authOk.bHeader), authOk.dwLoginKey, static_cast<unsigned>(authOk.bResult));
		CLOSESOCK(s);
		return 1;
	}

	if (FILE* fp = std::fopen(key_out_file, "w")) {
		std::fprintf(fp, "%u\n", authOk.dwLoginKey);
		std::fclose(fp);
	}

	std::printf("  [PASS] [auth-login] Real AUTH_SERVER authenticated ephemeral account -> HEADER_GC_AUTH_SUCCESS(150) dwLoginKey=%u bResult=%u\n",
		authOk.dwLoginKey, static_cast<unsigned>(authOk.bResult));
	CLOSESOCK(s);
	return 0;
}

static int run_game_login_key(int port, const char* login_arg, uint32_t login_key) {
	const char* login = (std::strcmp(login_arg, "@ENV") == 0) ? std::getenv("SMOKE_TEST_LOGIN") : login_arg;
	if (!login || !*login) {
		std::fprintf(stderr, "[game-login-key] FAIL: missing login environment variable\n");
		return 1;
	}

	sock_t s = connect_with_retry(port, 60);
	if (s == INVALID_SOCKET) {
		std::fprintf(stderr, "[game-login-key] FAIL: could not connect to GAME_SERVER on 127.0.0.1:%d\n", port);
		return 1;
	}

	if (!complete_wire_handshake(s, 2 /* PHASE_LOGIN */, "game-login-key")) {
		CLOSESOCK(s);
		return 1;
	}

	SmokePacketCGLogin2 pkt{};
	pkt.header = 109; // HEADER_CG_LOGIN2
	std::snprintf(pkt.login, sizeof(pkt.login), "%s", login);
	pkt.dwLoginKey = login_key;
	pkt.adwClientKey[0] = 0x11223344u;
	pkt.adwClientKey[1] = 0x55667788u;
	pkt.adwClientKey[2] = 0x99AABBCCu;
	pkt.adwClientKey[3] = 0xDDEEFF00u;

	// Pad 52-byte TPacketCGLogin2 to 56 bytes (multiple of 8) and TEA-encrypt with initial key "9382ujfn8237gydb"
	uint8_t plainBuf[56]{};
	uint8_t encBuf[56]{};
	std::memcpy(plainBuf, &pkt, sizeof(pkt));

	uint32_t initKey[4]{};
	get_initial_tea_key(initKey);
	int encLen = smoke_tea_encrypt(
		reinterpret_cast<uint32_t*>(encBuf),
		reinterpret_cast<const uint32_t*>(plainBuf),
		initKey,
		static_cast<int>(sizeof(pkt)));

	if (send(s, reinterpret_cast<const char*>(encBuf), encLen, 0) != encLen) {
		std::fprintf(stderr, "[game-login-key] FAIL: could not send TEA-encrypted TPacketCGLogin2 (%d bytes)\n", encLen);
		CLOSESOCK(s);
		return 1;
	}

	// CInputLogin::LoginByKey calls d->SetSecurityKey(pinfo->adwClientKey) before sending replies
	uint32_t serverEncKey[4]{};
	compute_security_tea_key(pkt.adwClientKey, serverEncKey);

	// Read TEA-encrypted packets from GAME_SERVER (TPacketGCEmpire [8B] + TPacketGCPhase(PHASE_SELECT=3) [8B] + TPacketGCLoginSuccess)
	uint8_t rxEnc[512]{};
	size_t rxTotal = 0;
	bool sawPhaseSelect = false;
	bool sawEmpire = false;
	bool sawLoginSuccess = false;
	uint8_t empireId = 0;
	uint32_t player0Id = 0;
	char player0Name[25]{};
	uint8_t player0Level = 0;

	for (int iter = 0; iter < 50 && !(sawPhaseSelect && sawLoginSuccess && player0Id != 0); ++iter) {
		fd_set rfds;
		FD_ZERO(&rfds);
		FD_SET(s, &rfds);
		struct timeval tv{0, 100000}; // 100ms
		int sel = select(static_cast<int>(s + 1), &rfds, nullptr, nullptr, &tv);
		if (sel > 0 && rxTotal < sizeof(rxEnc)) {
			int n = recv(s, reinterpret_cast<char*>(rxEnc + rxTotal), static_cast<int>(sizeof(rxEnc) - rxTotal), 0);
			if (n <= 0)
				break;
			rxTotal += static_cast<size_t>(n);

			// Decrypt all complete 8-byte blocks received so far
			size_t alignedLen = (rxTotal / 8) * 8;
			for (size_t off = 0; off < alignedLen; off += 8) {
				uint8_t blk[8]{};
				smoke_tea_decrypt(reinterpret_cast<uint32_t*>(blk), reinterpret_cast<const uint32_t*>(rxEnc + off), serverEncKey, 8);
				if (blk[0] == 90 /* HEADER_GC_EMPIRE */) {
					sawEmpire = true;
					empireId = blk[1];
				} else if (blk[0] == 0xFD /* HEADER_GC_PHASE */ && blk[1] == 3 /* PHASE_SELECT */) {
					sawPhaseSelect = true;
				} else if (blk[0] == 32 /* HEADER_GC_LOGIN_SUCCESS_NEWSLOT */ || blk[0] == 6 /* HEADER_GC_LOGIN_SUCCESS */) {
					sawLoginSuccess = true;
					if (off + 32 <= alignedLen) {
						uint8_t dec32[32]{};
						smoke_tea_decrypt(reinterpret_cast<uint32_t*>(dec32), reinterpret_cast<const uint32_t*>(rxEnc + off), serverEncKey, 32);
						std::memcpy(&player0Id, dec32 + 1, sizeof(uint32_t));
						std::memcpy(player0Name, dec32 + 5, 24);
						player0Name[24] = '\0';
						player0Level = dec32[31];
					}
				} else if (blk[0] == 7 /* HEADER_GC_LOGIN_FAILURE */) {
					std::fprintf(stderr, "[game-login-key] FAIL: GAME_SERVER returned HEADER_GC_LOGIN_FAILURE\n");
					CLOSESOCK(s);
					return 1;
				}
			}
		}
	}

	if (!sawPhaseSelect || !sawLoginSuccess || player0Id != 90011u || std::strcmp(player0Name, "SmokeHero") != 0 || player0Level != 15u) {
		std::fprintf(stderr, "[game-login-key] FAIL: incomplete LoginByKey response (rxTotal=%zu sawEmpire=%d sawPhaseSelect=%d sawLoginSuccess=%d player0Id=%u name='%s' level=%u)\n",
			rxTotal, sawEmpire ? 1 : 0, sawPhaseSelect ? 1 : 0, sawLoginSuccess ? 1 : 0,
			player0Id, player0Name, static_cast<unsigned>(player0Level));
		CLOSESOCK(s);
		return 1;
	}

	std::printf("  [PASS] [game-login-key] Real GAME_SERVER verified dwLoginKey=%u via db_r41023 -> HEADER_GC_EMPIRE(90, empire=%u) + TPacketGCPhase(PHASE_SELECT=3) + HEADER_GC_LOGIN_SUCCESS(32, char[0]={id=%u, name='%s', level=%u}) (%zu wire bytes)\n",
		login_key, static_cast<unsigned>(empireId), player0Id, player0Name, static_cast<unsigned>(player0Level), rxTotal);
	CLOSESOCK(s);
	return 0;
}

static int run_probe_db(int port) {
	sock_t s = connect_with_retry(port, 60);
	if (s == INVALID_SOCKET) {
		std::fprintf(stderr, "[probe-db] FAIL: could not connect to db_r41023 on 127.0.0.1:%d\n", port);
		return 1;
	}
	std::printf("  [PASS] Connected to live db_r41023 TCP listener on 127.0.0.1:%d\n", port);
	CLOSESOCK(s);
	return 0;
}

int main(int argc, char** argv) {
#ifdef _WIN32
	WSADATA wsa;
	WSAStartup(MAKEWORD(2, 2), &wsa);
#endif
	int rc = 1;
	if (argc >= 5 && std::strcmp(argv[1], "--mock-db") == 0) {
		rc = run_mock_db(std::atoi(argv[2]), argv[3], argv[4]);
	} else if (argc >= 3 && std::strcmp(argv[1], "--probe-game") == 0) {
		rc = run_probe_game(std::atoi(argv[2]));
	} else if (argc >= 3 && std::strcmp(argv[1], "--probe-db") == 0) {
		rc = run_probe_db(std::atoi(argv[2]));
	} else if (argc >= 3 && std::strcmp(argv[1], "--hash-env-pass") == 0) {
		rc = run_hash_env_pass(argv[2]);
	} else if (argc >= 7 && std::strcmp(argv[1], "--auth-login") == 0) {
		rc = run_auth_login(std::atoi(argv[2]), argv[3], argv[4], argv[5], argv[6]);
	} else if (argc >= 5 && std::strcmp(argv[1], "--game-login-key") == 0) {
		rc = run_game_login_key(std::atoi(argv[2]), argv[3], static_cast<uint32_t>(std::strtoul(argv[4], nullptr, 10)));
	} else {
		std::fprintf(stderr, "Usage:\n"
			"  %s --mock-db <port> <ready_file> <out_file>\n"
			"  %s --probe-game <port>\n"
			"  %s --probe-db <port>\n"
			"  %s --hash-env-pass <out_file>\n"
			"  %s --auth-login <port> <login|@ENV> <passwd|@ENV> <hwid_hex> <key_out_file>\n"
			"  %s --game-login-key <port> <login|@ENV> <login_key>\n",
			argv[0], argv[0], argv[0], argv[0], argv[0], argv[0]);
	}
#ifdef _WIN32
	WSACleanup();
#endif
	return rc;
}
