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

static constexpr uint32_t MT2_PROTOCOL_VERSION_LEGACY_X86 = 0x20090032U;
static constexpr uint32_t MT2_PROTOCOL_VERSION_X64        = 0x20090064U;

struct SmokePacketCGLogin2 {
	uint8_t  header;      // 109 (HEADER_CG_LOGIN2)
	char     login[31];   // LOGIN_MAX_LEN (30) + 1
	uint32_t dwLoginKey;
	uint32_t adwClientKey[4];
	uint32_t dwProtocolVersion;
};

struct SmokePacketCGLogin3 {
	uint8_t  header;      // 111 (HEADER_CG_LOGIN3)
	char     login[31];   // LOGIN_MAX_LEN (30) + 1
	char     passwd[17];  // PASSWD_MAX_LEN (16) + 1
	uint32_t adwClientKey[4];
	char     hwid[256];   // HWID_MAX_LEN (255) + 1
	uint32_t dwProtocolVersion;
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

struct SmokePacketCGEmpire {
	uint8_t  bHeader;     // 90 (HEADER_CG_EMPIRE)
	uint8_t  bEmpire;     // 1..3
};

struct SmokePacketCGPlayerCreate {
	uint8_t  header;      // 4 (HEADER_CG_CHARACTER_CREATE)
	uint8_t  index;       // slot index (0..3)
	char     name[25];    // CHARACTER_NAME_MAX_LEN (24) + 1
	uint16_t job;         // 0 = Warrior M
	uint8_t  shape;       // 0
	uint8_t  Con;
	uint8_t  Int;
	uint8_t  Str;
	uint8_t  Dex;
};

struct SmokePacketCGPlayerSelect {
	uint8_t  header;      // 6 (HEADER_CG_CHARACTER_SELECT)
	uint8_t  index;       // slot index (0..3)
};

struct SmokePacketCGEnterGame {
	uint8_t  header;      // 10 (HEADER_CG_ENTERGAME)
};

struct SmokePacketCGMove {
	uint8_t  bHeader;     // 7 (HEADER_CG_MOVE)
	uint8_t  bFunc;       // 0 = FUNC_WAIT, 1 = FUNC_MOVE
	uint8_t  bArg;
	uint8_t  bRot;
	int32_t  lX;
	int32_t  lY;
	uint32_t dwTime;
};

struct SmokePacketCGOnClick {
	uint8_t  header;      // 26 (HEADER_CG_ON_CLICK)
	uint32_t vid;
};

struct SmokePacketCGScriptAnswer {
	uint8_t  header;      // 29 (HEADER_CG_SCRIPT_ANSWER)
	uint8_t  answer;
};

struct SmokePacketCGQuestInputString {
	uint8_t  header;      // 30 (HEADER_CG_QUEST_INPUT_STRING)
	char     msg[65];     // 64 + 1
};

struct SmokePacketGCMainCharacter {
	uint8_t  header;      // 113 (HEADER_GC_MAIN_CHARACTER)
	uint32_t dwVID;
	uint32_t wRaceNum;
	char     szName[25];  // CHARACTER_NAME_MAX_LEN (24) + 1
	int32_t  lx;
	int32_t  ly;
	int32_t  lz;
	uint8_t  empire;
	uint8_t  skill_group;
};

struct SmokePacketGCCharacterAdd {
	uint8_t  header;      // 1 (HEADER_GC_CHARACTER_ADD)
	uint32_t dwVID;
	float    angle;
	int32_t  x;
	int32_t  y;
	int32_t  z;
	uint8_t  bType;
	uint32_t wRaceNum;
	uint8_t  bMovingSpeed;
	uint8_t  bAttackSpeed;
	uint8_t  bStateFlag;
	uint32_t dwAffectFlag[2];
};

struct SmokePacketGCChat {
	uint8_t  header;      // 4 (HEADER_GC_CHAT)
	uint16_t size;
	uint8_t  type;
	uint32_t id;
	uint8_t  bEmpire;
};

struct SmokePacketGCScript {
	uint8_t  header;      // 45 (HEADER_GC_SCRIPT)
	uint16_t size;
	uint8_t  skin;
	uint16_t src_size;
};
#pragma pack(pop)

static_assert(sizeof(SmokePacketGCPhase) == 2, "SmokePacketGCPhase must be 2 bytes");
static_assert(sizeof(SmokePacketHandshake) == 13, "SmokePacketHandshake must be 13 bytes");
static_assert(sizeof(SmokePacketCGLogin2) == 56, "SmokePacketCGLogin2 must be 56 bytes");
static_assert(sizeof(SmokePacketCGLogin3) == 325, "SmokePacketCGLogin3 must be 325 bytes");
static_assert(sizeof(SmokePacketGCAuthSuccess) == 6, "SmokePacketGCAuthSuccess must be 6 bytes");
static_assert(sizeof(SmokePacketGCLoginFailure) == 10, "SmokePacketGCLoginFailure must be 10 bytes");
static_assert(sizeof(SmokePacketCGEmpire) == 2, "SmokePacketCGEmpire must be 2 bytes");
static_assert(sizeof(SmokePacketCGPlayerCreate) == 34, "SmokePacketCGPlayerCreate must be 34 bytes");
static_assert(sizeof(SmokePacketCGPlayerSelect) == 2, "SmokePacketCGPlayerSelect must be 2 bytes");
static_assert(sizeof(SmokePacketCGEnterGame) == 1, "SmokePacketCGEnterGame must be 1 byte");
static_assert(sizeof(SmokePacketCGMove) == 16, "SmokePacketCGMove must be 16 bytes");
static_assert(sizeof(SmokePacketCGOnClick) == 5, "SmokePacketCGOnClick must be 5 bytes");
static_assert(sizeof(SmokePacketCGScriptAnswer) == 2, "SmokePacketCGScriptAnswer must be 2 bytes");
static_assert(sizeof(SmokePacketCGQuestInputString) == 66, "SmokePacketCGQuestInputString must be 66 bytes");
static_assert(sizeof(SmokePacketGCMainCharacter) == 48, "SmokePacketGCMainCharacter must be 48 bytes");
static_assert(sizeof(SmokePacketGCCharacterAdd) == 37, "SmokePacketGCCharacterAdd must be 37 bytes");
static_assert(sizeof(SmokePacketGCChat) == 9, "SmokePacketGCChat must be 9 bytes");
static_assert(sizeof(SmokePacketGCScript) == 6, "SmokePacketGCScript must be 6 bytes");

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

// Builds the exact 265-byte LZO1X-1 block that lzo1x_decompress_safe decodes to 65536 (128*128*4) zero bytes (ATTR_NONE).
static void build_lzo1x_zero_sectree_block(uint8_t out_block[265]) {
	std::memset(out_block, 0, 265);
	out_block[0] = 0x12u; // 1 initial literal byte (0x12 - 17 = 1)
	out_block[1] = 0x00u; // literal 0x00 at out[0]
	out_block[2] = 0x20u; // M3 match with extended length (256 zero bytes -> 256 * 255 = 65280)
	// out_block[3..258] = 0x00
	out_block[259] = 222u; // t = 65280 + 31 + 222 = 65533 -> copy 65533 + 2 = 65535 bytes from offset 1 -> total 65536 zero bytes
	out_block[260] = 0x00u; // m_pos offset low byte (offset = 1)
	out_block[261] = 0x00u; // m_pos offset high byte
	out_block[262] = 0x11u; // LZO1X EOF marker
	out_block[263] = 0x00u;
	out_block[264] = 0x00u;
}

static int run_gen_server_attr(const char* out_file, int w_sectrees, int h_sectrees) {
	if (w_sectrees <= 0 || h_sectrees <= 0 || w_sectrees > 64 || h_sectrees > 64) {
		std::fprintf(stderr, "[gen-server-attr] FAIL: invalid sectree dimensions %dx%d\n", w_sectrees, h_sectrees);
		return 1;
	}
	FILE* fp = std::fopen(out_file, "wb");
	if (!fp) {
		std::fprintf(stderr, "[gen-server-attr] FAIL: cannot open %s for writing\n", out_file);
		return 1;
	}
	int32_t hdr[2] = { static_cast<int32_t>(w_sectrees), static_cast<int32_t>(h_sectrees) };
	if (std::fwrite(hdr, sizeof(int32_t), 2, fp) != 2) {
		std::fclose(fp);
		return 1;
	}
	uint8_t lzoBlock[265]{};
	build_lzo1x_zero_sectree_block(lzoBlock);
	uint32_t blockSize = 265u;
	int total = w_sectrees * h_sectrees;
	for (int i = 0; i < total; ++i) {
		if (std::fwrite(&blockSize, sizeof(uint32_t), 1, fp) != 1 ||
		    std::fwrite(lzoBlock, 1, sizeof(lzoBlock), fp) != sizeof(lzoBlock)) {
			std::fclose(fp);
			return 1;
		}
	}
	std::fclose(fp);
	return 0;
}

// Known-answer vectors produced by legacy MySQL 4.1+/5.x/MariaDB PASSWORD() and stored
// verbatim in existing MT2009 account.password rows. The game's mysql_hash_password()
// must reproduce them byte-for-byte so old accounts keep authenticating on Target database: MariaDB 12.3 LTS.
static int run_self_test_hash() {
	struct { const char* pwd; const char* expected; } vectors[] = {
		{ "password", "*2470C0C06DEE42FD1618BB99005ADCA2EC9D1E19" },
		{ "root",     "*81F5E21E35407D884A6CD4A731AEBFB6AF209E1B" },
	};
	int failures = 0;
	for (const auto& v : vectors) {
		char hash[42]{};
		compute_mt2_password_hash(v.pwd, hash);
		if (std::strcmp(hash, v.expected) != 0) {
			std::fprintf(stderr, "[self-test-hash] FAIL: legacy vector mismatch got=%s expected=%s\n", hash, v.expected);
			++failures;
		}
	}
	uint8_t lzoBlock[265]{};
	build_lzo1x_zero_sectree_block(lzoBlock);
	if (lzoBlock[0] != 0x12u || lzoBlock[2] != 0x20u || lzoBlock[259] != 222u || lzoBlock[262] != 0x11u) {
		std::fprintf(stderr, "[self-test-hash] FAIL: LZO1X zero-sectree block invariant mismatch\n");
		++failures;
	}
	if (failures == 0)
		std::printf("  [PASS] [self-test-hash] %zu legacy MySQL PASSWORD() known-answer vectors + LZO1X sectree block verified\n", sizeof(vectors) / sizeof(vectors[0]));
	return failures == 0 ? 0 : 1;
}

static int run_auth_login(int port, const char* login_arg, const char* passwd_arg, const char* hwid_hex, const char* key_out_file, const char* expect_fail_status = nullptr) {
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
	pkt.dwProtocolVersion = MT2_PROTOCOL_VERSION_X64;

	// Pad 325-byte TPacketCGLogin3 to 328 bytes (multiple of 8) and TEA-encrypt with initial key "9382ujfn8237gydb"
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
		CLOSESOCK(s);
		if (expect_fail_status) {
			if (std::strcmp(failPkt.szStatus, expect_fail_status) == 0) {
				std::printf("  [PASS] [auth-expect-fail] AUTH_SERVER correctly rejected login with HEADER_GC_LOGIN_FAILURE status='%s'\n", failPkt.szStatus);
				return 0;
			}
			std::fprintf(stderr, "[auth-expect-fail] FAIL: expected status='%s' but got '%s'\n", expect_fail_status, failPkt.szStatus);
			return 1;
		}
		std::fprintf(stderr, "[auth-login] FAIL: AUTH_SERVER returned HEADER_GC_LOGIN_FAILURE status='%s'\n", failPkt.szStatus);
		return 1;
	}

	if (expect_fail_status) {
		std::fprintf(stderr, "[auth-expect-fail] FAIL: AUTH_SERVER did not reject login (header=%u), expected status='%s'\n",
			static_cast<unsigned>(respDec[0]), expect_fail_status);
		CLOSESOCK(s);
		return 1;
	}

	SmokePacketGCAuthSuccess authOk{};
	std::memcpy(&authOk, respDec, sizeof(authOk));
	if (authOk.bHeader != 150 || authOk.bResult != 1 || authOk.dwLoginKey == 0) {
		std::fprintf(stderr, "[auth-login] FAIL: unexpected auth response header=%u key_non_zero=%d result=%u\n",
			static_cast<unsigned>(authOk.bHeader), authOk.dwLoginKey != 0 ? 1 : 0, static_cast<unsigned>(authOk.bResult));
		CLOSESOCK(s);
		return 1;
	}

	if (FILE* fp = std::fopen(key_out_file, "w")) {
		std::fprintf(fp, "%u\n", authOk.dwLoginKey);
		std::fclose(fp);
	}

	std::printf("  [PASS] [auth-login] Real AUTH_SERVER authenticated ephemeral account -> HEADER_GC_AUTH_SUCCESS(150) dwLoginKey=***MASKED(non-zero)*** bResult=%u\n",
		static_cast<unsigned>(authOk.bResult));
	CLOSESOCK(s);
	return 0;
}

static bool send_tea_packet(sock_t s, const void* payload, size_t payload_len, const uint32_t key[4], const char* tag) {
	size_t aligned_len = (payload_len % 8 != 0) ? (payload_len + 8 - (payload_len % 8)) : payload_len;
	if (aligned_len == 0 || aligned_len > 512)
		return false;
	uint8_t plainBuf[512]{};
	uint8_t encBuf[512]{};
	std::memcpy(plainBuf, payload, payload_len);
	int encLen = smoke_tea_encrypt(
		reinterpret_cast<uint32_t*>(encBuf),
		reinterpret_cast<const uint32_t*>(plainBuf),
		key,
		static_cast<int>(payload_len));
	if (send(s, reinterpret_cast<const char*>(encBuf), encLen, 0) != encLen) {
		std::fprintf(stderr, "[%s] FAIL: could not send TEA-encrypted packet (%d bytes)\n", tag, encLen);
		return false;
	}
	return true;
}

// Returns the unpadded byte size of a Server->Client (GC) packet given its first decrypted 8-byte block.
// Every DESC::Packet call on the server is padded to an 8-byte TEA block boundary: alignedSize = (pktSize + 7) & ~7.
static size_t get_gc_packet_unpadded_size(const uint8_t blk8[8]) {
	uint8_t hdr = blk8[0];
	switch (hdr) {
		// Dynamic packets with uint16_t size at offset 1
		case 4:   // HEADER_GC_CHAT
		case 34:  // HEADER_GC_WHISPER
		case 38:  // HEADER_GC_SHOP
		case 40:  // HEADER_GC_DUEL_START
		case 45:  // HEADER_GC_SCRIPT
		case 63:  // HEADER_GC_TARGET
		case 74:  // HEADER_GC_MESSENGER
		case 75:  // HEADER_GC_GUILD
		case 81:  // HEADER_GC_QUEST_INFO
		case 100: // HEADER_GC_MARK_BLOCK
		case 102: // HEADER_GC_MARK_IDXLIST
		case 110: // HEADER_GC_DUNGEON
		case 115: // HEADER_GC_NPC_POSITION
		case 130: // HEADER_GC_LAND_LIST
		case 133: // HEADER_GC_SYMBOL_DATA
		case 152: // HEADER_GC_HYBRIDCRYPT_KEYS
		case 153: // HEADER_GC_HYBRIDCRYPT_SDB
		case 155: // HEADER_GC_FISHING_GAME
		{
			uint16_t wSize = 0;
			std::memcpy(&wSize, blk8 + 1, sizeof(uint16_t));
			return wSize >= 3 ? static_cast<size_t>(wSize) : 8u;
		}
		// Large dynamic packets with uint32_t (int) size at offset 1
		case 67:  // HEADER_GC_CAPTCHA
		case 116: // HEADER_GC_SPECIAL_SHOP
		case 117: // HEADER_GC_NPC_INFO_BOARD
		case 142: // HEADER_GC_ITEMSHOP
		case 214: // HEADER_GC_NEW_OFFLINESHOP (ENABLE_LARGE_DYNAMIC_PACKETS)
		{
			uint32_t dwSize = 0;
			std::memcpy(&dwSize, blk8 + 1, sizeof(uint32_t));
			return dwSize >= 5 ? static_cast<size_t>(dwSize) : 8u;
		}
		// Static packets
		case 1:   return 37;   // HEADER_GC_CHARACTER_ADD
		case 2:   return 5;    // HEADER_GC_CHARACTER_DEL
		case 3:   return 24;   // HEADER_GC_MOVE
		case 6:   return 353;  // HEADER_GC_LOGIN_SUCCESS
		case 32:  return 353;  // HEADER_GC_LOGIN_SUCCESS_NEWSLOT
		case 7:   return 10;   // HEADER_GC_LOGIN_FAILURE
		case 8:   return 69;   // HEADER_GC_CHARACTER_CREATE_SUCCESS
		case 9:   return 2;    // HEADER_GC_CHARACTER_CREATE_FAILURE
		case 10:  return 2;    // HEADER_GC_CHARACTER_DELETE_SUCCESS
		case 11:  return 1;    // HEADER_GC_CHARACTER_DELETE_WRONG_SOCIAL_ID
		case 13:  return 5;    // HEADER_GC_STUN
		case 14:  return 5;    // HEADER_GC_DEAD
		case 16:  return 1417; // HEADER_GC_CHARACTER_POINTS (x64: 1 + 177 * 8)
		case 17:  return 25;   // HEADER_GC_CHARACTER_POINT_CHANGE (x64: 4 + 4 + 1 + 8 + 8)
		case 18:  return 7;    // HEADER_GC_CHANGE_SPEED
		case 19:  return 44;   // HEADER_GC_CHARACTER_UPDATE
		case 20:  return 36;   // HEADER_GC_ITEM_DEL (deprecated)
		case 21:  return 54;   // HEADER_GC_ITEM_SET
		case 22:  return 13;   // HEADER_GC_ITEM_USE
		case 23:  return 4;    // HEADER_GC_ITEM_DROP
		case 25:  return 41;   // HEADER_GC_ITEM_UPDATE
		case 26:  return 26;   // HEADER_GC_ITEM_GROUND_ADD
		case 27:  return 5;    // HEADER_GC_ITEM_GROUND_DEL
		case 28:  return 6;    // HEADER_GC_QUICKSLOT_ADD
		case 29:  return 2;    // HEADER_GC_QUICKSLOT_DEL
		case 30:  return 3;    // HEADER_GC_QUICKSLOT_SWAP
		case 31:  return 29;   // HEADER_GC_ITEM_OWNERSHIP
		case 36:  return 11;   // HEADER_GC_MOTION
		case 39:  return 38;   // HEADER_GC_SHOP_SIGN
		case 41:  return 10;   // HEADER_GC_PVP
		case 42:  return 42;   // HEADER_GC_EXCHANGE
		case 43:  return 6;    // HEADER_GC_CHARACTER_POSITION
		case 44:  return 1;    // HEADER_GC_PING
		case 46:  return 74;   // HEADER_GC_QUEST_CONFIRM
		case 61:  return 14;   // HEADER_GC_MOUNT
		case 62:  return 9;    // HEADER_GC_OWNERSHIP
		case 65:  return 15;   // HEADER_GC_WARP
		case 66:  return 50;   // HEADER_GC_GAMEMASTER_TARGET
		case 68:  return 2;    // HEADER_GC_REQUEST_INFO
		case 69:  return 17;   // HEADER_GC_ADD_FLY_TARGETING
		case 70:  return 10;   // HEADER_GC_CREATE_FLY
		case 71:  return 17;   // HEADER_GC_FLY_TARGETING
		case 76:  return 1591; // HEADER_GC_SKILL_LEVEL
		case 87:  return 1;    // HEADER_GC_SAFEBOX_WRONG_PASSWORD
		case 88:  return 2;    // HEADER_GC_SAFEBOX_SIZE
		case 89:  return 7;    // HEADER_GC_FISHING
		case 90:  return 2;    // HEADER_GC_EMPIRE
		case 106: return 9;    // HEADER_GC_TIME (x64: 1 + 8)
		case 107: return 30;   // HEADER_GC_CHANGE_NAME
		case 111: return 6;    // HEADER_GC_WALK_MODE
		case 112: return 2;    // HEADER_GC_SKILL_GROUP
		case 113: return 48;   // HEADER_GC_MAIN_CHARACTER
		case 114: return 6;    // HEADER_GC_SEPCIAL_EFFECT
		case 118: return 5;    // HEADER_GC_LOGIN_KEY
		case 121: return 2;    // HEADER_GC_CHANNEL
		case 122: return 2;    // HEADER_GC_MALL_OPEN
		case 123: return 13;   // HEADER_GC_TARGET_UPDATE
		case 124: return 5;    // HEADER_GC_TARGET_DELETE
		case 125: return 43;   // HEADER_GC_TARGET_CREATE
		case 126: return 26;   // HEADER_GC_AFFECT_ADD
		case 127: return 6;    // HEADER_GC_AFFECT_REMOVE
		case 131: return 27;   // HEADER_GC_LOVER_INFO
		case 132: return 2;    // HEADER_GC_LOVE_POINT_UPDATE
		case 134: return 10;   // HEADER_GC_DIG_MOTION
		case 135: return 10;   // HEADER_GC_DAMAGE_INFO
		case 136: return 62;   // HEADER_GC_CHAR_ADDITIONAL_INFO
		case 137: return 73;   // HEADER_GC_MAIN_CHARACTER3_BGM
		case 138: return 77;   // HEADER_GC_MAIN_CHARACTER4_BGM_VOL
		case 139: return 9;    // HEADER_GC_SET_GOLD
		case 140: return 9;    // HEADER_GC_UPDATE_GOLD
		case 141: return 42;   // HEADER_GC_SPECIAL_FLAG
		case 143: return 7;    // HEADER_GC_BUSY_ACTION
		case 150: return 6;    // HEADER_GC_AUTH_SUCCESS
		case 151: return 289;  // HEADER_GC_PANAMA_PACK
		case 156: return 17;   // HEADER_GC_ADD_FLY_SHOOT_TARGETING
		case 157: return 5;    // HEADER_GC_CLEAR_FLY_SHOOT_TARGETING
		case 161: return 9;    // HEADER_GC_GUILD_TOKEN
		case 208: return 133;  // HEADER_GC_SPECIFIC_EFFECT
		case 209: return 5;    // HEADER_GC_DRAGON_SOUL_REFINE
		case 252: return 1;    // HEADER_GC_TIME_SYNC
		case 253: return 2;    // HEADER_GC_PHASE
		case 255: return 13;   // HEADER_GC_HANDSHAKE
		default:  return 8;
	}
}

struct SmokeGameStreamState {
	uint8_t  rxEnc[65536]{};
	uint8_t  rxDec[65536]{};
	size_t   rxTotal = 0;
	size_t   decOff = 0;
	size_t   parseOff = 0;

	bool     sawEmpire = false;
	uint8_t  empireId = 0;
	bool     sawPhaseSelect = false;
	bool     sawPhaseLoading = false;
	bool     sawPhaseGame = false;
	bool     sawLoginSuccess = false;
	bool     sawLoginFailure = false;
	bool     sawCharCreateSuccess = false;
	bool     sawCharCreateFailure = false;
	uint8_t  charCreateFailType = 0;
	uint32_t player0Id = 0;
	char     player0Name[25]{};
	uint8_t  player0Level = 0;
	int32_t  player0X = 0;
	int32_t  player0Y = 0;
	int32_t  player0Addr = 0;
	uint16_t player0Port = 0;

	bool     sawWarp = false;
	int32_t  warpX = 0;
	int32_t  warpY = 0;
	int32_t  warpAddr = 0;
	uint16_t warpPort = 0;

	bool     sawMainCharacter = false;
	uint32_t mainVid = 0;
	uint32_t mainRaceNum = 0;
	char     mainName[25]{};
	int32_t  mainX = 0;
	int32_t  mainY = 0;
	bool     sawPoints = false;
	bool     sawSkillLevel = false;

	bool     sawPcCharAdd = false;
	bool     sawNpcCharAdd = false;
	uint32_t npcVid = 0;
	int32_t  npcX = 0;
	int32_t  npcY = 0;
	bool     sawItemSet = false;
	uint32_t item0Vnum = 0;
	bool     sawQuestLoginChat = false;
	std::string questLoginChatText;

	int      scriptCount = 0;
	std::string lastScriptText;
	bool     sawScriptWait = false;
	bool     sawScriptQuestion = false;
	bool     sawScriptInput = false;
	bool     sawNpcCoroutineChat = false;
	std::string npcCoroutineChatText;
};

static bool pump_gc_packets(sock_t s, SmokeGameStreamState& st, const uint32_t serverEncKey[4], int wait_ms) {
	fd_set rfds;
	FD_ZERO(&rfds);
	FD_SET(s, &rfds);
	struct timeval tv{ wait_ms / 1000, (wait_ms % 1000) * 1000 };
	int sel = select(static_cast<int>(s + 1), &rfds, nullptr, nullptr, &tv);
	if (sel > 0 && st.rxTotal < sizeof(st.rxEnc)) {
		int n = recv(s, reinterpret_cast<char*>(st.rxEnc + st.rxTotal), static_cast<int>(sizeof(st.rxEnc) - st.rxTotal), 0);
		if (n <= 0)
			return false;
		st.rxTotal += static_cast<size_t>(n);
	}

	size_t alignedTotal = (st.rxTotal / 8u) * 8u;
	if (alignedTotal > st.decOff) {
		size_t toDec = alignedTotal - st.decOff;
		smoke_tea_decrypt(
			reinterpret_cast<uint32_t*>(st.rxDec + st.decOff),
			reinterpret_cast<const uint32_t*>(st.rxEnc + st.decOff),
			serverEncKey,
			static_cast<int>(toDec));
		st.decOff = alignedTotal;
	}

	while (st.parseOff + 8u <= st.decOff) {
		const uint8_t* pkt = st.rxDec + st.parseOff;
		uint8_t hdr = pkt[0];
		size_t pktSize = get_gc_packet_unpadded_size(pkt);
		size_t alignedSize = (pktSize == 0) ? 8u : ((pktSize + 7u) & ~static_cast<size_t>(7u));
		if (st.parseOff + alignedSize > st.decOff) {
			// Wait for the rest of this multi-block packet to arrive from the socket
			break;
		}

		if (hdr == 90 /* HEADER_GC_EMPIRE */) {
			st.sawEmpire = true;
			st.empireId = pkt[1];
		} else if (hdr == 0xFD /* HEADER_GC_PHASE */) {
			if (pkt[1] == 3 /* PHASE_SELECT */)
				st.sawPhaseSelect = true;
			else if (pkt[1] == 4 /* PHASE_LOADING */)
				st.sawPhaseLoading = true;
			else if (pkt[1] == 5 /* PHASE_GAME */)
				st.sawPhaseGame = true;
		} else if (hdr == 32 /* HEADER_GC_LOGIN_SUCCESS_NEWSLOT */ || hdr == 6 /* HEADER_GC_LOGIN_SUCCESS */) {
			st.sawLoginSuccess = true;
			std::memcpy(&st.player0Id, pkt + 1, sizeof(uint32_t));
			std::memcpy(st.player0Name, pkt + 5, 24);
			st.player0Name[24] = '\0';
			st.player0Level = pkt[31];
			std::memcpy(&st.player0X, pkt + 53, sizeof(int32_t));
			std::memcpy(&st.player0Y, pkt + 57, sizeof(int32_t));
			std::memcpy(&st.player0Addr, pkt + 61, sizeof(int32_t));
			std::memcpy(&st.player0Port, pkt + 65, sizeof(uint16_t));
		} else if (hdr == 65 /* HEADER_GC_WARP */) {
			st.sawWarp = true;
			std::memcpy(&st.warpX, pkt + 1, sizeof(int32_t));
			std::memcpy(&st.warpY, pkt + 5, sizeof(int32_t));
			std::memcpy(&st.warpAddr, pkt + 9, sizeof(int32_t));
			std::memcpy(&st.warpPort, pkt + 13, sizeof(uint16_t));
		} else if (hdr == 8 /* HEADER_GC_CHARACTER_CREATE_SUCCESS */) {
			st.sawCharCreateSuccess = true;
			std::memcpy(&st.player0Id, pkt + 2, sizeof(uint32_t));
			std::memcpy(st.player0Name, pkt + 6, 24);
			st.player0Name[24] = '\0';
			st.player0Level = pkt[32];
		} else if (hdr == 9 /* HEADER_GC_CHARACTER_CREATE_FAILURE */) {
			st.sawCharCreateFailure = true;
			st.charCreateFailType = pkt[1];
		} else if (hdr == 7 /* HEADER_GC_LOGIN_FAILURE */) {
			st.sawLoginFailure = true;
		} else if (hdr == 113 /* HEADER_GC_MAIN_CHARACTER */ ||
		           hdr == 137 /* HEADER_GC_MAIN_CHARACTER3_BGM */ ||
		           hdr == 138 /* HEADER_GC_MAIN_CHARACTER4_BGM_VOL */) {
			SmokePacketGCMainCharacter mc{};
			std::memcpy(&mc, pkt, sizeof(mc));
			mc.szName[sizeof(mc.szName) - 1] = '\0';
			st.sawMainCharacter = true;
			st.mainVid = mc.dwVID;
			st.mainRaceNum = mc.wRaceNum;
			std::snprintf(st.mainName, sizeof(st.mainName), "%s", mc.szName);
			size_t posOff = 34u;
			if (hdr == 137)
				posOff = 59u;
			else if (hdr == 138)
				posOff = 63u;
			std::memcpy(&st.mainX, pkt + posOff, sizeof(int32_t));
			std::memcpy(&st.mainY, pkt + posOff + 4u, sizeof(int32_t));
		} else if (hdr == 16 /* HEADER_GC_CHARACTER_POINTS */) {
			st.sawPoints = true;
		} else if (hdr == 76 /* HEADER_GC_SKILL_LEVEL */) {
			st.sawSkillLevel = true;
		} else if (hdr == 1 /* HEADER_GC_CHARACTER_ADD */) {
			SmokePacketGCCharacterAdd ca{};
			std::memcpy(&ca, pkt, sizeof(ca));
			if (ca.dwVID == st.mainVid) {
				st.sawPcCharAdd = true;
			} else if (ca.wRaceNum == 20016u) {
				st.sawNpcCharAdd = true;
				st.npcVid = ca.dwVID;
				st.npcX = ca.x;
				st.npcY = ca.y;
			}
		} else if (hdr == 21 /* HEADER_GC_ITEM_SET */) {
			uint32_t vnum = 0;
			std::memcpy(&vnum, pkt + 4, sizeof(uint32_t));
			if (vnum == 19u || (vnum != 0u && !st.sawItemSet)) {
				st.sawItemSet = true;
				st.item0Vnum = vnum;
			}
		} else if (hdr == 4 /* HEADER_GC_CHAT */ && pktSize >= sizeof(SmokePacketGCChat)) {
			size_t msgLen = pktSize - sizeof(SmokePacketGCChat);
			std::string msg(reinterpret_cast<const char*>(pkt + sizeof(SmokePacketGCChat)), msgLen);
			while (!msg.empty() && (msg.back() == '\0' || msg.back() == '\r' || msg.back() == '\n'))
				msg.pop_back();
			if (msg.find("[SMOKE_QUEST_LOGIN_OK]") != std::string::npos &&
			    msg.find("item19=1 qlib=1") != std::string::npos) {
				st.sawQuestLoginChat = true;
				st.questLoginChatText = msg;
			}
			if (msg.find("[SMOKE_NPC_COROUTINE_OK] sel=1 input=SmokeInput55") != std::string::npos) {
				st.sawNpcCoroutineChat = true;
				st.npcCoroutineChatText = msg;
			}
		} else if (hdr == 45 /* HEADER_GC_SCRIPT */ && pktSize >= sizeof(SmokePacketGCScript)) {
			SmokePacketGCScript sc{};
			std::memcpy(&sc, pkt, sizeof(sc));
			size_t srcLen = (sc.src_size <= pktSize - sizeof(SmokePacketGCScript))
				? static_cast<size_t>(sc.src_size)
				: (pktSize - sizeof(SmokePacketGCScript));
			std::string scriptStr(reinterpret_cast<const char*>(pkt + sizeof(SmokePacketGCScript)), srcLen);
			++st.scriptCount;
			st.lastScriptText = scriptStr;
			if (scriptStr.find("[NEXT]") != std::string::npos || scriptStr.find("[WAIT]") != std::string::npos)
				st.sawScriptWait = true;
			if (scriptStr.find("[QUESTION") != std::string::npos)
				st.sawScriptQuestion = true;
			if (scriptStr.find("[INPUT]") != std::string::npos)
				st.sawScriptInput = true;
		} else if (hdr == 44 /* HEADER_GC_PING */) {
			// Reply with HEADER_CG_PONG (254) so ping timer remains satisfied
			uint8_t pong = 0xFEu;
			uint32_t clientKey[4] = { 0x11223344u, 0x55667788u, 0x99AABBCCu, 0xDDEEFF00u };
			send_tea_packet(s, &pong, 1, clientKey, "game-pong");
		}

		st.parseOff += alignedSize;
	}
	return true;
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
	pkt.dwProtocolVersion = MT2_PROTOCOL_VERSION_X64;

	uint32_t initKey[4]{};
	get_initial_tea_key(initKey);
	if (!send_tea_packet(s, &pkt, sizeof(pkt), initKey, "game-login-key")) {
		CLOSESOCK(s);
		return 1;
	}

	// CInputLogin::LoginByKey calls d->SetSecurityKey(pinfo->adwClientKey) before sending replies:
	// - Server->Client is encrypted with serverEncKey
	// - Client->Server is decrypted with pkt.adwClientKey
	uint32_t serverEncKey[4]{};
	compute_security_tea_key(pkt.adwClientKey, serverEncKey);

	SmokeGameStreamState st{};

	// Stage A: Wait for PHASE_SELECT + TPacketGCLoginSuccess
	for (int iter = 0; iter < 60 && !(st.sawPhaseSelect && st.sawLoginSuccess); ++iter) {
		if (!pump_gc_packets(s, st, serverEncKey, 100))
			break;
		if (st.sawLoginFailure) {
			std::fprintf(stderr, "[game-login-key] FAIL: GAME_SERVER returned HEADER_GC_LOGIN_FAILURE\n");
			CLOSESOCK(s);
			return 1;
		}
	}

	if (!st.sawPhaseSelect || !st.sawLoginSuccess) {
		std::fprintf(stderr, "[game-login-key] FAIL: incomplete LoginByKey response (rxTotal=%zu sawEmpire=%d sawPhaseSelect=%d sawLoginSuccess=%d player0Id=%u)\n",
			st.rxTotal, st.sawEmpire ? 1 : 0, st.sawPhaseSelect ? 1 : 0, st.sawLoginSuccess ? 1 : 0, st.player0Id);
		CLOSESOCK(s);
		return 1;
	}

	// Stage A.1: If the account is brand-new (0 characters in slot 0), select Empire 1 and create 'SmokeHero'
	if (st.player0Id == 0) {
		std::printf("  [PASS] [game-login-key] Fresh account entered PHASE_SELECT(3) with 0 characters (empire=%u, rxTotal=%zu wire bytes)\n",
			static_cast<unsigned>(st.empireId), st.rxTotal);

		st.sawEmpire = false;
		st.sawLoginSuccess = false;
		st.empireId = 0;

		SmokePacketCGEmpire empPkt{};
		empPkt.bHeader = 90; // HEADER_CG_EMPIRE
		empPkt.bEmpire = 1;  // Shinsoo (Empire 1)
		if (!send_tea_packet(s, &empPkt, sizeof(empPkt), pkt.adwClientKey, "game-empire-select")) {
			CLOSESOCK(s);
			return 1;
		}

		for (int iter = 0; iter < 60 && (!st.sawEmpire || !st.sawLoginSuccess); ++iter) {
			if (!pump_gc_packets(s, st, serverEncKey, 100))
				break;
		}

		if (!st.sawEmpire || st.empireId != 1 || !st.sawLoginSuccess) {
			std::fprintf(stderr, "[game-empire-select] FAIL: did not receive HEADER_GC_EMPIRE(90, empire=1) confirmation (sawEmpire=%d empire=%u sawLoginSuccess=%d)\n",
				st.sawEmpire ? 1 : 0, static_cast<unsigned>(st.empireId), st.sawLoginSuccess ? 1 : 0);
			CLOSESOCK(s);
			return 1;
		}

		SmokePacketCGPlayerCreate createPkt{};
		createPkt.header = 4; // HEADER_CG_CHARACTER_CREATE
		createPkt.index = 0;
		std::snprintf(createPkt.name, sizeof(createPkt.name), "%s", "SmokeHero");
		createPkt.job = 0;   // Warrior Male
		createPkt.shape = 0;
		if (!send_tea_packet(s, &createPkt, sizeof(createPkt), pkt.adwClientKey, "game-char-create")) {
			CLOSESOCK(s);
			return 1;
		}

		for (int iter = 0; iter < 60 && !st.sawCharCreateSuccess && !st.sawCharCreateFailure; ++iter) {
			if (!pump_gc_packets(s, st, serverEncKey, 100))
				break;
		}

		if (!st.sawCharCreateSuccess || st.player0Id == 0 || std::strcmp(st.player0Name, "SmokeHero") != 0 || st.player0Level != 1u) {
			std::fprintf(stderr, "[game-char-create] FAIL: character creation failed (sawSuccess=%d sawFail=%d failType=%u player0Id=%u name='%s' level=%u)\n",
				st.sawCharCreateSuccess ? 1 : 0, st.sawCharCreateFailure ? 1 : 0, static_cast<unsigned>(st.charCreateFailType),
				st.player0Id, st.player0Name, static_cast<unsigned>(st.player0Level));
			CLOSESOCK(s);
			return 1;
		}

		std::printf("  [PASS] [game-char-create] Created new character via HEADER_CG_EMPIRE(90, empire=1) + HEADER_CG_CHARACTER_CREATE(4) -> HEADER_GC_CHARACTER_CREATE_SUCCESS(8, slot=0, id=%u, name='%s', level=%u)\n",
			st.player0Id, st.player0Name, static_cast<unsigned>(st.player0Level));
	} else {
		if (std::strcmp(st.player0Name, "SmokeHero") != 0 || st.player0Level < 1u) {
			std::fprintf(stderr, "[game-login-key] FAIL: unexpected persisted character in slot 0 (player0Id=%u name='%s' level=%u)\n",
				st.player0Id, st.player0Name, static_cast<unsigned>(st.player0Level));
			CLOSESOCK(s);
			return 1;
		}

		std::printf("  [PASS] [game-login-key] Real GAME_SERVER verified dwLoginKey=***MASKED(non-zero)*** via db_r41023 -> HEADER_GC_EMPIRE(90, empire=%u) + TPacketGCPhase(PHASE_SELECT=3) + HEADER_GC_LOGIN_SUCCESS(32, char[0]={id=%u, name='%s', level=%u}) (%zu wire bytes)\n",
			static_cast<unsigned>(st.empireId), st.player0Id, st.player0Name, static_cast<unsigned>(st.player0Level), st.rxTotal);
	}

	// Stage B: Send HEADER_CG_CHARACTER_SELECT(6, index=0) and wait for PHASE_LOADING + TPacketGCMainCharacter + Points + SkillLevel
	SmokePacketCGPlayerSelect selPkt{};
	selPkt.header = 6; // HEADER_CG_CHARACTER_SELECT
	selPkt.index = 0;
	if (!send_tea_packet(s, &selPkt, sizeof(selPkt), pkt.adwClientKey, "game-char-select")) {
		CLOSESOCK(s);
		return 1;
	}

	for (int iter = 0; iter < 80 && !(st.sawPhaseLoading && st.sawMainCharacter && st.sawPoints && st.sawSkillLevel); ++iter) {
		if (!pump_gc_packets(s, st, serverEncKey, 100))
			break;
	}

	if (!st.sawPhaseLoading || !st.sawMainCharacter || !st.sawPoints || !st.sawSkillLevel ||
	    std::strcmp(st.mainName, "SmokeHero") != 0 ||
	    st.mainX < 409600 || st.mainX >= 512000 || st.mainY < 896000 || st.mainY >= 1024000) {
		std::fprintf(stderr, "[game-enter-world] FAIL: incomplete PHASE_LOADING response (sawPhaseLoading=%d sawMainChar=%d sawPoints=%d sawSkill=%d name='%s' pos=%dx%d vid=%u)\n",
			st.sawPhaseLoading ? 1 : 0, st.sawMainCharacter ? 1 : 0, st.sawPoints ? 1 : 0, st.sawSkillLevel ? 1 : 0,
			st.mainName, st.mainX, st.mainY, st.mainVid);
		CLOSESOCK(s);
		return 1;
	}

	// Stage C: Send HEADER_CG_ENTERGAME(10) and wait for PHASE_GAME + CharacterAdd(PC & NPC 20016) + ItemSet(vnum=19) + QuestLoginChat
	SmokePacketCGEnterGame enterPkt{};
	enterPkt.header = 10; // HEADER_CG_ENTERGAME
	if (!send_tea_packet(s, &enterPkt, sizeof(enterPkt), pkt.adwClientKey, "game-entergame")) {
		CLOSESOCK(s);
		return 1;
	}

	const char* realSfEnv = std::getenv("SMOKE_REAL_SERVERFILES");
	const bool isRealServerfiles = (realSfEnv && std::strcmp(realSfEnv, "1") == 0);

	if (isRealServerfiles) {
		for (int iter = 0; iter < 80 && !(st.sawPhaseGame && st.sawPcCharAdd && st.sawItemSet); ++iter) {
			if (!pump_gc_packets(s, st, serverEncKey, 100))
				break;
		}
		if (!st.sawPhaseGame || !st.sawPcCharAdd) {
			std::fprintf(stderr, "[game-enter-world] FAIL: real serverfiles PHASE_GAME entry failed (sawPhaseGame=%d sawPcAdd=%d sawItemSet=%d rxTotal=%zu)\n",
				st.sawPhaseGame ? 1 : 0, st.sawPcCharAdd ? 1 : 0, st.sawItemSet ? 1 : 0, st.rxTotal);
			CLOSESOCK(s);
			return 1;
		}
		std::printf("  [PASS] [game-enter-world] Real serverfiles: SmokeHero entered PHASE_GAME(5) on metin2_map_a1 (vid=%u, pos=%dx%d, sawItemSet=%d, item0Vnum=%u, rxTotal=%zu wire bytes)\n",
			st.mainVid, st.mainX, st.mainY, st.sawItemSet ? 1 : 0, st.item0Vnum, st.rxTotal);
		CLOSESOCK(s);
		return 0;
	}

	for (int iter = 0; iter < 80 && !(st.sawPhaseGame && st.sawPcCharAdd && st.sawNpcCharAdd && st.sawItemSet && st.sawQuestLoginChat); ++iter) {
		if (!pump_gc_packets(s, st, serverEncKey, 100))
			break;
	}

	if (!st.sawPhaseGame || !st.sawPcCharAdd || !st.sawNpcCharAdd || st.npcVid == 0 || !st.sawItemSet || !st.sawQuestLoginChat) {
		std::fprintf(stderr, "[game-enter-world] FAIL: incomplete PHASE_GAME world entry (sawPhaseGame=%d sawPcAdd=%d sawNpcAdd=%d npcVid=%u sawItemSet=%d sawQuestLoginChat=%d rxTotal=%zu)\n",
			st.sawPhaseGame ? 1 : 0, st.sawPcCharAdd ? 1 : 0, st.sawNpcCharAdd ? 1 : 0,
			st.npcVid, st.sawItemSet ? 1 : 0, st.sawQuestLoginChat ? 1 : 0, st.rxTotal);
		CLOSESOCK(s);
		return 1;
	}

	std::printf("  [PASS] [game-enter-world] SmokeHero entered PHASE_GAME: HEADER_CG_CHARACTER_SELECT(6) -> PHASE_LOADING(4) + HEADER_GC_MAIN_CHARACTER(113, vid=%u, pos=%dx%d) -> HEADER_CG_ENTERGAME(10) -> PHASE_GAME(5) + HEADER_GC_CHARACTER_ADD(PC vid=%u, NPC 20016 vid=%u at %dx%d) + HEADER_GC_ITEM_SET(vnum=%u)\n",
		st.mainVid, st.mainX, st.mainY, st.mainVid, st.npcVid, st.npcX, st.npcY, st.item0Vnum);

	// Stage D: Click NPC 20016 (HEADER_CG_ON_CLICK=26) and drive wait() -> select() -> input() coroutine suspension/resumption
	SmokePacketCGOnClick clickPkt{};
	clickPkt.header = 26; // HEADER_CG_ON_CLICK
	clickPkt.vid = st.npcVid;
	if (!send_tea_packet(s, &clickPkt, sizeof(clickPkt), pkt.adwClientKey, "game-npc-click")) {
		CLOSESOCK(s);
		return 1;
	}

	// D.1: Wait for wait() script ("[WAIT]")
	for (int iter = 0; iter < 50 && !st.sawScriptWait; ++iter) {
		if (!pump_gc_packets(s, st, serverEncKey, 100))
			break;
	}
	if (!st.sawScriptWait) {
		std::fprintf(stderr, "[game-quest-npc] FAIL: did not receive HEADER_GC_SCRIPT(45) [WAIT] after clicking NPC 20016 (vid=%u, scriptCount=%d)\n",
			st.npcVid, st.scriptCount);
		CLOSESOCK(s);
		return 1;
	}

	// Resume wait() with HEADER_CG_SCRIPT_ANSWER(29, answer=254 [QUEST_NO_CONFIRM / NEXT])
	SmokePacketCGScriptAnswer waitAns{};
	waitAns.header = 29; // HEADER_CG_SCRIPT_ANSWER
	waitAns.answer = 254;
	if (!send_tea_packet(s, &waitAns, sizeof(waitAns), pkt.adwClientKey, "game-script-wait-ans")) {
		CLOSESOCK(s);
		return 1;
	}

	// D.2: Wait for select() script ("[QUESTION")
	for (int iter = 0; iter < 50 && !st.sawScriptQuestion; ++iter) {
		if (!pump_gc_packets(s, st, serverEncKey, 100))
			break;
	}
	if (!st.sawScriptQuestion) {
		std::fprintf(stderr, "[game-quest-npc] FAIL: did not receive HEADER_GC_SCRIPT(45) [QUESTION] after resuming wait() (lastScript='%s')\n",
			st.lastScriptText.c_str());
		CLOSESOCK(s);
		return 1;
	}

	// Resume select() with HEADER_CG_SCRIPT_ANSWER(29, answer=0 -> Lua 1-based index 1)
	SmokePacketCGScriptAnswer selAns{};
	selAns.header = 29; // HEADER_CG_SCRIPT_ANSWER
	selAns.answer = 0;
	if (!send_tea_packet(s, &selAns, sizeof(selAns), pkt.adwClientKey, "game-script-select-ans")) {
		CLOSESOCK(s);
		return 1;
	}

	// D.3: Wait for input() script ("[ENTER]")
	for (int iter = 0; iter < 50 && !st.sawScriptInput; ++iter) {
		if (!pump_gc_packets(s, st, serverEncKey, 100))
			break;
	}
	if (!st.sawScriptInput) {
		std::fprintf(stderr, "[game-quest-npc] FAIL: did not receive HEADER_GC_SCRIPT(45) [ENTER] after resuming select() (lastScript='%s')\n",
			st.lastScriptText.c_str());
		CLOSESOCK(s);
		return 1;
	}

	// Resume input() with HEADER_CG_QUEST_INPUT_STRING(30, msg="SmokeInput55")
	SmokePacketCGQuestInputString inpPkt{};
	inpPkt.header = 30; // HEADER_CG_QUEST_INPUT_STRING
	std::snprintf(inpPkt.msg, sizeof(inpPkt.msg), "%s", "SmokeInput55");
	if (!send_tea_packet(s, &inpPkt, sizeof(inpPkt), pkt.adwClientKey, "game-script-input-str")) {
		CLOSESOCK(s);
		return 1;
	}

	// D.4: Wait for final coroutine completion chat ("[SMOKE_NPC_COROUTINE_OK] sel=1 input=SmokeInput55")
	for (int iter = 0; iter < 50 && !st.sawNpcCoroutineChat; ++iter) {
		if (!pump_gc_packets(s, st, serverEncKey, 100))
			break;
	}
	if (!st.sawNpcCoroutineChat) {
		std::fprintf(stderr, "[game-quest-npc] FAIL: did not receive [SMOKE_NPC_COROUTINE_OK] chat after resuming input()\n");
		CLOSESOCK(s);
		return 1;
	}

	std::printf("  [PASS] [game-quest-npc] Real QuestLoad + NPC 20016 coroutine verified: login_chat='%s', wait()->select(1)->input('SmokeInput55') -> '%s' (%d script packets, %zu total wire bytes)\n",
		st.questLoginChatText.c_str(), st.npcCoroutineChatText.c_str(), st.scriptCount, st.rxTotal);

	CLOSESOCK(s);
	return 0;
}

static int run_game_login_key_expect_fail(int port, const char* login_arg, uint32_t login_key) {
	const char* login = (std::strcmp(login_arg, "@ENV") == 0) ? std::getenv("SMOKE_TEST_LOGIN") : login_arg;
	if (!login || !*login) {
		std::fprintf(stderr, "[game-login-key-expect-fail] FAIL: missing login environment variable\n");
		return 1;
	}

	sock_t s = connect_with_retry(port, 60);
	if (s == INVALID_SOCKET) {
		std::fprintf(stderr, "[game-login-key-expect-fail] FAIL: could not connect to GAME_SERVER on 127.0.0.1:%d\n", port);
		return 1;
	}

	if (!complete_wire_handshake(s, 2 /* PHASE_LOGIN */, "game-login-key-expect-fail")) {
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
	pkt.dwProtocolVersion = MT2_PROTOCOL_VERSION_X64;

	uint32_t initKey[4]{};
	get_initial_tea_key(initKey);
	if (!send_tea_packet(s, &pkt, sizeof(pkt), initKey, "game-login-key-expect-fail")) {
		CLOSESOCK(s);
		return 1;
	}

	uint32_t serverEncKey[4]{};
	compute_security_tea_key(pkt.adwClientKey, serverEncKey);
	SmokeGameStreamState st{};

	// Pump up to 2.5 seconds; server must either send HEADER_GC_LOGIN_FAILURE or close connection (PHASE_CLOSE) without entering PHASE_SELECT
	bool disconnected = false;
	for (int iter = 0; iter < 25; ++iter) {
		if (!pump_gc_packets(s, st, serverEncKey, 100)) {
			disconnected = true;
			break;
		}
		if (st.sawLoginFailure || st.sawPhaseSelect || st.sawLoginSuccess)
			break;
	}
	CLOSESOCK(s);

	if (st.sawPhaseSelect || st.sawLoginSuccess) {
		std::fprintf(stderr, "[game-login-key-expect-fail] FAIL: GAME_SERVER accepted a replayed/invalid dwLoginKey into PHASE_SELECT!\n");
		return 1;
	}

	std::printf("  [PASS] [game-login-key-expect-fail] GAME_SERVER + db_r41023 rejected replayed/consumed dwLoginKey (sawLoginFailure=%d disconnected=%d sawPhaseSelect=0)\n",
		st.sawLoginFailure ? 1 : 0, disconnected ? 1 : 0);
	return 0;
}

static int run_game_cross_core_warp(int ch11_port, int ch12_port, const char* login_arg, uint32_t login_key) {
	const char* login = (std::strcmp(login_arg, "@ENV") == 0) ? std::getenv("SMOKE_TEST_LOGIN") : login_arg;
	if (!login || !*login) {
		std::fprintf(stderr, "[game-cross-core-warp] FAIL: missing login environment variable\n");
		return 1;
	}

	uint32_t initKey[4]{};
	get_initial_tea_key(initKey);
	uint32_t clientKey[4] = { 0x11223344u, 0x55667788u, 0x99AABBCCu, 0xDDEEFF00u };
	uint32_t serverEncKey[4]{};
	compute_security_tea_key(clientKey, serverEncKey);

	SmokePacketCGLogin2 loginPkt{};
	loginPkt.header = 109; // HEADER_CG_LOGIN2
	std::snprintf(loginPkt.login, sizeof(loginPkt.login), "%s", login);
	loginPkt.dwLoginKey = login_key;
	std::memcpy(loginPkt.adwClientKey, clientKey, sizeof(clientKey));
	loginPkt.dwProtocolVersion = MT2_PROTOCOL_VERSION_X64;

	// Step 1: Connect to ch11 (28000) and verify CMapLocation resolves slot 0 (on map_index=3) to ch12_port (28001)
	sock_t s1 = connect_with_retry(ch11_port, 60);
	if (s1 == INVALID_SOCKET) {
		std::fprintf(stderr, "[game-cross-core-warp] FAIL: could not connect to ch11 on port %d\n", ch11_port);
		return 1;
	}
	if (!complete_wire_handshake(s1, 2 /* PHASE_LOGIN */, "cross-core-ch11-select") ||
	    !send_tea_packet(s1, &loginPkt, sizeof(loginPkt), initKey, "cross-core-ch11-login2")) {
		CLOSESOCK(s1);
		return 1;
	}

	SmokeGameStreamState st1{};
	for (int iter = 0; iter < 60 && !(st1.sawPhaseSelect && st1.sawLoginSuccess); ++iter) {
		if (!pump_gc_packets(s1, st1, serverEncKey, 100))
			break;
	}
	CLOSESOCK(s1);

	if (!st1.sawPhaseSelect || !st1.sawLoginSuccess || st1.player0Id == 0 ||
	    st1.player0Port != static_cast<uint16_t>(ch12_port)) {
		std::fprintf(stderr, "[game-cross-core-warp] FAIL: ch11 PHASE_SELECT CMapLocation redirect mismatch (sawSelect=%d sawLogin=%d id=%u port=%u expectedPort=%d pos=%dx%d)\n",
			st1.sawPhaseSelect ? 1 : 0, st1.sawLoginSuccess ? 1 : 0, st1.player0Id,
			static_cast<unsigned>(st1.player0Port), ch12_port, st1.player0X, st1.player0Y);
		return 1;
	}
	std::printf("  [PASS] [cross-core-step1] ch11 (port %d) CMapLocation resolved character '%s' (id=%u, pos=%dx%d) to secondary core ch12 port=%u\n",
		ch11_port, st1.player0Name, st1.player0Id, st1.player0X, st1.player0Y, static_cast<unsigned>(st1.player0Port));

	// Step 2: Connect to ch12 (28001) with the same dwLoginKey, enter PHASE_GAME on map_index=3, and trigger portal 10002 WarpSet
	sock_t s2 = connect_with_retry(ch12_port, 60);
	if (s2 == INVALID_SOCKET) {
		std::fprintf(stderr, "[game-cross-core-warp] FAIL: could not connect to ch12 on port %d\n", ch12_port);
		return 1;
	}
	if (!complete_wire_handshake(s2, 2 /* PHASE_LOGIN */, "cross-core-ch12-login") ||
	    !send_tea_packet(s2, &loginPkt, sizeof(loginPkt), initKey, "cross-core-ch12-login2")) {
		CLOSESOCK(s2);
		return 1;
	}

	SmokeGameStreamState st2{};
	for (int iter = 0; iter < 60 && !(st2.sawPhaseSelect && st2.sawLoginSuccess); ++iter) {
		if (!pump_gc_packets(s2, st2, serverEncKey, 100))
			break;
	}
	if (!st2.sawPhaseSelect || !st2.sawLoginSuccess || st2.player0Port != static_cast<uint16_t>(ch12_port)) {
		std::fprintf(stderr, "[game-cross-core-warp] FAIL: ch12 LoginByKey handoff failed (sawSelect=%d sawLogin=%d port=%u)\n",
			st2.sawPhaseSelect ? 1 : 0, st2.sawLoginSuccess ? 1 : 0, static_cast<unsigned>(st2.player0Port));
		CLOSESOCK(s2);
		return 1;
	}

	SmokePacketCGPlayerSelect selPkt{};
	selPkt.header = 6; // HEADER_CG_CHARACTER_SELECT
	selPkt.index = 0;
	if (!send_tea_packet(s2, &selPkt, sizeof(selPkt), clientKey, "cross-core-ch12-select")) {
		CLOSESOCK(s2);
		return 1;
	}
	for (int iter = 0; iter < 80 && !(st2.sawPhaseLoading && st2.sawMainCharacter && st2.sawPoints && st2.sawSkillLevel); ++iter) {
		if (!pump_gc_packets(s2, st2, serverEncKey, 100))
			break;
	}
	if (!st2.sawPhaseLoading || !st2.sawMainCharacter) {
		std::fprintf(stderr, "[game-cross-core-warp] FAIL: ch12 PHASE_LOADING failed\n");
		CLOSESOCK(s2);
		return 1;
	}

	SmokePacketCGEnterGame enterPkt{};
	enterPkt.header = 10; // HEADER_CG_ENTERGAME
	if (!send_tea_packet(s2, &enterPkt, sizeof(enterPkt), clientKey, "cross-core-ch12-enter")) {
		CLOSESOCK(s2);
		return 1;
	}
	for (int iter = 0; iter < 80 && !(st2.sawPhaseGame && st2.sawPcCharAdd); ++iter) {
		if (!pump_gc_packets(s2, st2, serverEncKey, 100))
			break;
	}
	if (!st2.sawPhaseGame || !st2.sawPcCharAdd) {
		std::fprintf(stderr, "[game-cross-core-warp] FAIL: ch12 PHASE_GAME entry failed\n");
		CLOSESOCK(s2);
		return 1;
	}
	std::printf("  [PASS] [cross-core-step2] Entered PHASE_GAME(5) on secondary core ch12 (port %d, map_index=3, vid=%u, pos=%dx%d)\n",
		ch12_port, st2.mainVid, st2.mainX, st2.mainY);

	// Step onto portal 10002 at (402000, 899600) on metin2_map_a3 -> warps to (450300, 905000) on metin2_map_a1 (ch11)
	if (!st2.sawWarp) {
		SmokePacketCGMove mv1{};
		mv1.bHeader = 7; // HEADER_CG_MOVE
		mv1.bFunc = 1;   // FUNC_MOVE
		mv1.lX = 402000;
		mv1.lY = 899600;
		mv1.dwTime = 1000;
		send_tea_packet(s2, &mv1, sizeof(mv1), clientKey, "cross-core-ch12-move1");

		SmokePacketCGMove mv2{};
		mv2.bHeader = 7; // HEADER_CG_MOVE
		mv2.bFunc = 0;   // FUNC_WAIT (commits position via ch->Move)
		mv2.lX = 402000;
		mv2.lY = 899600;
		mv2.dwTime = 1050;
		send_tea_packet(s2, &mv2, sizeof(mv2), clientKey, "cross-core-ch12-move2");
	}

	for (int iter = 0; iter < 60 && !st2.sawWarp; ++iter) {
		if (!pump_gc_packets(s2, st2, serverEncKey, 100))
			break;
	}
	CLOSESOCK(s2);

	if (!st2.sawWarp || st2.warpPort != static_cast<uint16_t>(ch11_port) ||
	    st2.warpX != 450300 || st2.warpY != 905000) {
		std::fprintf(stderr, "[game-cross-core-warp] FAIL: did not receive expected HEADER_GC_WARP from portal 10002 on ch12 (sawWarp=%d warpPort=%u expectedPort=%d warpPos=%dx%d)\n",
			st2.sawWarp ? 1 : 0, static_cast<unsigned>(st2.warpPort), ch11_port, st2.warpX, st2.warpY);
		return 1;
	}
	std::printf("  [PASS] [cross-core-step3] Portal 10002 on ch12 triggered CHARACTER::WarpSet -> HEADER_GC_WARP(65, dest=%dx%d, target_port=%u)\n",
		st2.warpX, st2.warpY, static_cast<unsigned>(st2.warpPort));

	// Step 3: Reconnect to ch11 (28000) with the same dwLoginKey and verify arrival on map_index=1 at (450300, 905000)
	sock_t s3 = connect_with_retry(ch11_port, 60);
	if (s3 == INVALID_SOCKET) {
		std::fprintf(stderr, "[game-cross-core-warp] FAIL: could not reconnect to ch11 on port %d after warp\n", ch11_port);
		return 1;
	}
	if (!complete_wire_handshake(s3, 2 /* PHASE_LOGIN */, "cross-core-ch11-postwarp") ||
	    !send_tea_packet(s3, &loginPkt, sizeof(loginPkt), initKey, "cross-core-ch11-postwarp-login2")) {
		CLOSESOCK(s3);
		return 1;
	}

	SmokeGameStreamState st3{};
	for (int iter = 0; iter < 60 && !(st3.sawPhaseSelect && st3.sawLoginSuccess); ++iter) {
		if (!pump_gc_packets(s3, st3, serverEncKey, 100))
			break;
	}
	if (!st3.sawPhaseSelect || !st3.sawLoginSuccess || st3.player0Port != static_cast<uint16_t>(ch11_port)) {
		std::fprintf(stderr, "[game-cross-core-warp] FAIL: post-warp ch11 PHASE_SELECT failed (sawSelect=%d sawLogin=%d port=%u)\n",
			st3.sawPhaseSelect ? 1 : 0, st3.sawLoginSuccess ? 1 : 0, static_cast<unsigned>(st3.player0Port));
		CLOSESOCK(s3);
		return 1;
	}
	if (!send_tea_packet(s3, &selPkt, sizeof(selPkt), clientKey, "cross-core-ch11-postwarp-select")) {
		CLOSESOCK(s3);
		return 1;
	}
	for (int iter = 0; iter < 80 && !(st3.sawPhaseLoading && st3.sawMainCharacter && st3.sawPoints && st3.sawSkillLevel); ++iter) {
		if (!pump_gc_packets(s3, st3, serverEncKey, 100))
			break;
	}
	if (!st3.sawPhaseLoading || !st3.sawMainCharacter ||
	    !send_tea_packet(s3, &enterPkt, sizeof(enterPkt), clientKey, "cross-core-ch11-postwarp-enter")) {
		std::fprintf(stderr, "[game-cross-core-warp] FAIL: post-warp ch11 PHASE_LOADING failed\n");
		CLOSESOCK(s3);
		return 1;
	}
	for (int iter = 0; iter < 80 && !(st3.sawPhaseGame && st3.sawPcCharAdd); ++iter) {
		if (!pump_gc_packets(s3, st3, serverEncKey, 100))
			break;
	}
	CLOSESOCK(s3);

	if (!st3.sawPhaseGame || !st3.sawPcCharAdd || st3.mainX != 450300 || st3.mainY != 905000) {
		std::fprintf(stderr, "[game-cross-core-warp] FAIL: post-warp ch11 PHASE_GAME position mismatch (sawGame=%d sawPcAdd=%d pos=%dx%d expected=450300x905000)\n",
			st3.sawPhaseGame ? 1 : 0, st3.sawPcCharAdd ? 1 : 0, st3.mainX, st3.mainY);
		return 1;
	}
	std::printf("  [PASS] [cross-core-step4] Completed cross-core WarpSet handoff ch12(%d) -> ch11(%d) into PHASE_GAME(5) on map_index=1 at %dx%d\n",
		ch12_port, ch11_port, st3.mainX, st3.mainY);

	// Step 5: Prove single-use LoginByKey security across peers: after normal disconnect (no WarpSet/MoveChannel handoff armed),
	// replaying the consumed dwLoginKey on ANOTHER peer (ch12) or the SAME peer (ch11) must be rejected.
#ifdef _WIN32
	Sleep(200);
#else
	usleep(200000);
#endif
	if (run_game_login_key_expect_fail(ch12_port, login_arg, login_key) != 0) {
		std::fprintf(stderr, "[game-cross-core-warp] FAIL: cross-peer (ch12) dwLoginKey replay was not rejected!\n");
		return 1;
	}
	if (run_game_login_key_expect_fail(ch11_port, login_arg, login_key) != 0) {
		std::fprintf(stderr, "[game-cross-core-warp] FAIL: same-peer (ch11) dwLoginKey replay was not rejected!\n");
		return 1;
	}
	std::printf("  [PASS] [cross-core-step5] Verified single-use LoginByKey security: cross-peer (ch12:%d) and same-peer (ch11:%d) dwLoginKey replay rejected\n",
		ch12_port, ch11_port);
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
	} else if (argc >= 5 && std::strcmp(argv[1], "--gen-server-attr") == 0) {
		rc = run_gen_server_attr(argv[2], std::atoi(argv[3]), std::atoi(argv[4]));
	} else if (argc >= 7 && std::strcmp(argv[1], "--auth-login") == 0) {
		rc = run_auth_login(std::atoi(argv[2]), argv[3], argv[4], argv[5], argv[6]);
	} else if (argc >= 7 && std::strcmp(argv[1], "--auth-expect-fail") == 0) {
		rc = run_auth_login(std::atoi(argv[2]), argv[3], argv[4], argv[5], "", argv[6]);
	} else if (argc >= 2 && std::strcmp(argv[1], "--self-test-hash") == 0) {
		rc = run_self_test_hash();
	} else if (argc >= 5 && std::strcmp(argv[1], "--game-login-key") == 0) {
		rc = run_game_login_key(std::atoi(argv[2]), argv[3], static_cast<uint32_t>(std::strtoul(argv[4], nullptr, 10)));
	} else if (argc >= 5 && std::strcmp(argv[1], "--game-login-key-expect-fail") == 0) {
		rc = run_game_login_key_expect_fail(std::atoi(argv[2]), argv[3], static_cast<uint32_t>(std::strtoul(argv[4], nullptr, 10)));
	} else if (argc >= 6 && std::strcmp(argv[1], "--game-cross-core-warp") == 0) {
		rc = run_game_cross_core_warp(std::atoi(argv[2]), std::atoi(argv[3]), argv[4], static_cast<uint32_t>(std::strtoul(argv[5], nullptr, 10)));
	} else {
		std::fprintf(stderr, "Usage:\n"
			"  %s --mock-db <port> <ready_file> <out_file>\n"
			"  %s --probe-game <port>\n"
			"  %s --probe-db <port>\n"
			"  %s --hash-env-pass <out_file>\n"
			"  %s --gen-server-attr <out_file> <w_sectrees> <h_sectrees>\n"
			"  %s --self-test-hash\n"
			"  %s --auth-login <port> <login|@ENV> <passwd|@ENV> <hwid_hex> <key_out_file>\n"
			"  %s --auth-expect-fail <port> <login|@ENV> <passwd|@ENV> <hwid_hex> <expected_status>\n"
			"  %s --game-login-key <port> <login|@ENV> <login_key>\n"
			"  %s --game-login-key-expect-fail <port> <login|@ENV> <login_key>\n"
			"  %s --game-cross-core-warp <ch11_port> <ch12_port> <login|@ENV> <login_key>\n",
			argv[0], argv[0], argv[0], argv[0], argv[0], argv[0], argv[0], argv[0], argv[0], argv[0], argv[0]);
	}
#ifdef _WIN32
	WSACleanup();
#endif
	return rc;
}
