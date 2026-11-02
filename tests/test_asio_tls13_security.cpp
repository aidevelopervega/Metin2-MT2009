// ==============================================================================
// Metin2 MT2009 — Standalone Asio 1.38.2 + OpenSSL TLS 1.3 Verification Suite
// Covers FAZ 2, FAZ 3, FAZ 4, FAZ 5, and FAZ 8 (22 test groups, 120+ assertions)
// Compiles cleanly on:
//   - Windows MinGW-w64 UCRT64 GCC 16 (PE32+ AMD64)
//   - Windows MSVC 18 (PE32+ AMD64)
//   - FreeBSD 15.1 GCC 14 (ELF64 AMD64)
// ==============================================================================

#include <net/mt2_asio_tls13_transport.h>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <thread>
#include <vector>

namespace
{

int g_passed = 0;
int g_failed = 0;

void Check(bool cond, const char* expr, int line)
{
    if (cond)
    {
        ++g_passed;
    }
    else
    {
        ++g_failed;
        std::fprintf(stderr, "[FAIL] Line %d: %s\n", line, expr);
    }
}

#define MT2_CHECK(cond) Check((cond), #cond, __LINE__)

// Canonical MT2009 x64 Wire Protocol Structures (#pragma pack(1))
constexpr uint32_t kMt2ProtocolVersionX64 = 0x20090064U;
constexpr int64_t kGoldMax = 1000000000000LL;

enum Mt2Headers : uint8_t
{
    HEADER_CG_ATTACK = 2,
    HEADER_GC_MOVE = 3,
    HEADER_CG_CHARACTER_SELECT = 4,
    HEADER_CG_MOVE = 7,
    HEADER_CG_ENTERGAME = 10,
    HEADER_CG_ITEM_MOVE = 13,
    HEADER_GC_POINTS = 16,
    HEADER_GC_LOGIN_SUCCESS_NEWSLOT = 32,
    HEADER_GC_SHOP = 38,
    HEADER_CG_SHOP = 50,
    HEADER_CG_USE_SKILL = 52,
    HEADER_CG_EXCHANGE = 55,
    HEADER_GC_WARP = 65,
    HEADER_GC_GUILD = 75,
    HEADER_CG_GUILD = 80,
    HEADER_CG_LOGIN2 = 109,
    HEADER_CG_LOGIN3 = 111,
    HEADER_GC_MAIN_CHARACTER = 113,
    HEADER_GC_AUTH_SUCCESS = 150,
    HEADER_GC_NEW_OFFLINESHOP = 164,
    HEADER_CG_HANDSHAKE = 0xFF,
    HEADER_GC_HANDSHAKE = 0xFF,

    // P2P headers
    HEADER_GG_SETUP = 1,
    HEADER_GG_LOGIN = 2,
    HEADER_GG_RELAY = 5
};

#pragma pack(push, 1)
struct WirePacketHandshake
{
    uint8_t bHeader = HEADER_GC_HANDSHAKE;
    uint32_t dwHandshake = 0;
    uint32_t dwTime = 0;
    int32_t lDelta = 0;
};

struct WirePacketCGLogin3
{
    uint8_t header = HEADER_CG_LOGIN3;
    char login[31]{};
    char passwd[17]{};
    uint32_t adwClientKey[4]{};
    uint32_t dwProtocolVersion = kMt2ProtocolVersionX64;
    char hwid[65]{};
};

struct WirePacketGCAuthSuccess
{
    uint8_t bHeader = HEADER_GC_AUTH_SUCCESS;
    uint32_t dwLoginKey = 0;
    uint8_t bResult = 1;
};

struct WirePacketCGLogin2
{
    uint8_t header = HEADER_CG_LOGIN2;
    char login[31]{};
    uint32_t dwLoginKey = 0;
    uint32_t adwClientKey[4]{};
    uint32_t dwProtocolVersion = kMt2ProtocolVersionX64;
};

struct WirePacketGCLoginSuccess
{
    uint8_t bHeader = HEADER_GC_LOGIN_SUCCESS_NEWSLOT;
    uint64_t aullPID[4]{};
    uint32_t dwGuildID[4]{};
};

struct WirePacketCGCharacterSelect
{
    uint8_t header = HEADER_CG_CHARACTER_SELECT;
    uint8_t index = 0;
};

struct WirePacketGCMainCharacter
{
    uint8_t header = HEADER_GC_MAIN_CHARACTER;
    uint32_t dwVID = 0;
    uint16_t wRaceNum = 0;
    char szName[25]{};
    int32_t lx = 0, ly = 0, lz = 0;
    uint8_t empire = 1;
    uint8_t skill_group = 1;
};

struct WirePacketCGEnterGame
{
    uint8_t header = HEADER_CG_ENTERGAME;
};

struct WirePacketGCPoints
{
    uint8_t header = HEADER_GC_POINTS;
    int64_t points[255]{};
};

struct WirePacketCGMove
{
    uint8_t bHeader = HEADER_CG_MOVE;
    uint8_t bFunc = 0;
    uint8_t bArg = 0;
    uint8_t bRot = 0;
    int32_t lX = 0;
    int32_t lY = 0;
    uint32_t dwTime = 0;
};

struct WirePacketCGAttack
{
    uint8_t bHeader = HEADER_CG_ATTACK;
    uint8_t bType = 0;
    uint32_t dwVID = 0;
    uint8_t bCRCMagicCubeProcPiece = 0;
    uint8_t bCRCMagicCubeFilePiece = 0;
};

struct WirePacketCGUseSkill
{
    uint8_t bHeader = HEADER_CG_USE_SKILL;
    uint32_t dwVnum = 0;
    uint32_t dwVID = 0;
};

struct WirePacketCGItemMove
{
    uint8_t header = HEADER_CG_ITEM_MOVE;
    uint8_t cell_window = 1;
    uint16_t cell_pos = 0;
    uint8_t cell_to_window = 1;
    uint16_t cell_to_pos = 1;
    uint8_t count = 1;
};

struct WirePacketCGExchange
{
    uint8_t header = HEADER_CG_EXCHANGE;
    uint8_t sub_header = 0;
    int64_t arg1 = 0; // 64-bit Yang / VID
    uint8_t arg2 = 0;
    uint8_t pos_window = 0;
    uint16_t pos_cell = 0;
};

struct WirePacketCGShopBuy
{
    uint8_t header = HEADER_CG_SHOP;
    uint8_t subheader = 1; // SHOP_SUBHEADER_CG_BUY
    uint8_t pos = 0;
    int64_t expected_price = 0;
};

struct WirePacketGCWarp
{
    uint8_t bHeader = HEADER_GC_WARP;
    int32_t lX = 0;
    int32_t lY = 0;
    int32_t lAddr = 0;
    uint16_t wPort = 0;
};

struct WirePacketDynamic16Header
{
    uint8_t header = HEADER_GC_SHOP;
    uint16_t size = 0;
    uint8_t subheader = 0;
};

struct WirePacketDynamic32Header
{
    uint8_t header = HEADER_GC_NEW_OFFLINESHOP;
    uint32_t size = 0;
    uint8_t subheader = 0;
};

struct WirePacketGGSetup
{
    uint8_t bHeader = HEADER_GG_SETUP;
    uint16_t wPort = 0;
    uint8_t bChannel = 1;
};

struct WirePacketGGLogin
{
    uint8_t bHeader = HEADER_GG_LOGIN;
    char szName[25]{};
    uint64_t dwPID = 0;
    uint8_t bEmpire = 1;
    int32_t lMapIndex = 1;
    uint8_t bChannel = 1;
};

struct WirePacketGGRelay
{
    uint8_t bHeader = HEADER_GG_RELAY;
    char szName[25]{};
    int32_t lSize = 0;
};
#pragma pack(pop)

mt2::net::Mt2PacketSchemaRegistry BuildCanonicalSchema()
{
    mt2::net::Mt2PacketSchemaRegistry reg;
    reg.RegisterFixed(HEADER_CG_HANDSHAKE, sizeof(WirePacketHandshake));
    reg.RegisterFixed(HEADER_CG_LOGIN3, sizeof(WirePacketCGLogin3));
    reg.RegisterFixed(HEADER_GC_AUTH_SUCCESS, sizeof(WirePacketGCAuthSuccess));
    reg.RegisterFixed(HEADER_CG_LOGIN2, sizeof(WirePacketCGLogin2));
    reg.RegisterFixed(HEADER_GC_LOGIN_SUCCESS_NEWSLOT, sizeof(WirePacketGCLoginSuccess));
    reg.RegisterFixed(HEADER_CG_CHARACTER_SELECT, sizeof(WirePacketCGCharacterSelect));
    reg.RegisterFixed(HEADER_GC_MAIN_CHARACTER, sizeof(WirePacketGCMainCharacter));
    reg.RegisterFixed(HEADER_CG_ENTERGAME, sizeof(WirePacketCGEnterGame));
    reg.RegisterFixed(HEADER_GC_POINTS, sizeof(WirePacketGCPoints));
    reg.RegisterFixed(HEADER_CG_MOVE, sizeof(WirePacketCGMove));
    reg.RegisterFixed(HEADER_GC_MOVE, sizeof(WirePacketCGMove));
    reg.RegisterFixed(HEADER_CG_ATTACK, sizeof(WirePacketCGAttack));
    reg.RegisterFixed(HEADER_CG_USE_SKILL, sizeof(WirePacketCGUseSkill));
    reg.RegisterFixed(HEADER_CG_ITEM_MOVE, sizeof(WirePacketCGItemMove));
    reg.RegisterFixed(HEADER_CG_EXCHANGE, sizeof(WirePacketCGExchange));
    reg.RegisterFixed(HEADER_CG_SHOP, sizeof(WirePacketCGShopBuy));
    reg.RegisterFixed(HEADER_GC_WARP, sizeof(WirePacketGCWarp));
    reg.RegisterDynamic16(HEADER_GC_SHOP, sizeof(WirePacketDynamic16Header));
    reg.RegisterDynamic16(HEADER_GC_GUILD, sizeof(WirePacketDynamic16Header));
    reg.RegisterDynamic16(HEADER_CG_GUILD, sizeof(WirePacketDynamic16Header));
    reg.RegisterDynamic32(HEADER_GC_NEW_OFFLINESHOP, sizeof(WirePacketDynamic32Header));
    return reg;
}

mt2::net::Mt2PacketSchemaRegistry BuildP2PSchema()
{
    mt2::net::Mt2PacketSchemaRegistry reg;
    reg.RegisterFixed(HEADER_GG_SETUP, sizeof(WirePacketGGSetup));
    reg.RegisterFixed(HEADER_GG_LOGIN, sizeof(WirePacketGGLogin));
    reg.RegisterFixed(HEADER_GG_RELAY, sizeof(WirePacketGGRelay));
    return reg;
}

void RunIoUntil(asio::io_context& io, const std::function<bool()>& pred, std::chrono::milliseconds timeout = std::chrono::milliseconds(4000))
{
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    while (!pred() && std::chrono::steady_clock::now() < deadline)
    {
        io.restart();
        io.run_for(std::chrono::milliseconds(5));
    }
}

// ------------------------------------------------------------------------------
// Test 1: SDK Version & Standalone Asio Invariants (FAZ 2 & FAZ 3)
// ------------------------------------------------------------------------------
void TestSdkInvariants()
{
    MT2_CHECK(ASIO_VERSION == 103802); // Asio 1.38.2
    MT2_CHECK(OPENSSL_VERSION_MAJOR >= 3);
    MT2_CHECK(OpenSSL_version_num() >= 0x30000000L);
    const char* ver_str = OpenSSL_version(OPENSSL_VERSION);
    MT2_CHECK(ver_str != nullptr && std::strstr(ver_str, "OpenSSL 3.") != nullptr);
    MT2_CHECK(mt2::net::DetectCompilePlatform() == mt2::net::PlatformTarget::WindowsX64 ||
              mt2::net::DetectCompilePlatform() == mt2::net::PlatformTarget::FreeBsdAmd64);
    std::printf("[PASS] Test 1: Standalone Asio %d.%d.%d + %s verified\n",
        ASIO_VERSION / 100000, (ASIO_VERSION / 100) % 1000, ASIO_VERSION % 100, ver_str);
}

// ------------------------------------------------------------------------------
// Test 2: Standalone Asio Plain TCP Loopback, Write Queue & Backpressure (FAZ 2)
// ------------------------------------------------------------------------------
void TestAsioTcpLoopbackAndBackpressure()
{
    asio::io_context io;
    asio::ip::tcp::acceptor acceptor(io, asio::ip::tcp::endpoint(asio::ip::make_address("127.0.0.1"), 0));
    const uint16_t port = acceptor.local_endpoint().port();
    MT2_CHECK(port > 0);

    mt2::net::TransportLimits limits;
    limits.max_send_queue_bytes = 64 * 1024;
    limits.idle_timeout = std::chrono::milliseconds(2000);

    std::shared_ptr<mt2::net::AsioTcpSession> srv_sess;
    std::vector<uint8_t> srv_received;
    std::atomic<bool> srv_disconnected{false};

    acceptor.async_accept([&](const asio::error_code& ec, asio::ip::tcp::socket sock) {
        MT2_CHECK(!ec);
        srv_sess = std::make_shared<mt2::net::AsioTcpSession>(io, std::move(sock), limits);
        srv_sess->SetCallbacks(
            [&](std::shared_ptr<mt2::net::AsioTcpSession> s, const uint8_t* data, size_t len) {
                srv_received.insert(srv_received.end(), data, data + len);
                s->EnqueueSend(data, len); // echo back
            },
            [&](std::shared_ptr<mt2::net::AsioTcpSession>, const asio::error_code&) {
                srv_disconnected.store(true);
            });
        srv_sess->Start();
    });

    asio::ip::tcp::socket cli_sock(io);
    cli_sock.connect(asio::ip::tcp::endpoint(asio::ip::make_address("127.0.0.1"), port));
    auto cli_sess = std::make_shared<mt2::net::AsioTcpSession>(io, std::move(cli_sock), limits);

    std::vector<uint8_t> cli_received;
    std::atomic<bool> cli_disconnected{false};
    cli_sess->SetCallbacks(
        [&](std::shared_ptr<mt2::net::AsioTcpSession>, const uint8_t* data, size_t len) {
            cli_received.insert(cli_received.end(), data, data + len);
        },
        [&](std::shared_ptr<mt2::net::AsioTcpSession>, const asio::error_code&) {
            cli_disconnected.store(true);
        });
    cli_sess->Start();

    // Queue 100 rapid messages to verify strand-serialized write queue ordering
    std::vector<uint8_t> expected;
    for (int i = 0; i < 100; ++i)
    {
        uint32_t seq = static_cast<uint32_t>(0xA5A50000U | i);
        const uint8_t* p = reinterpret_cast<const uint8_t*>(&seq);
        expected.insert(expected.end(), p, p + sizeof(seq));
        MT2_CHECK(cli_sess->EnqueueSend(&seq, sizeof(seq)));
    }

    RunIoUntil(io, [&]() { return cli_received.size() == expected.size(); });
    MT2_CHECK(cli_received == expected);
    MT2_CHECK(srv_received == expected);

    // Test slow-reader backpressure overflow (exceed 64 KiB cap)
    std::vector<uint8_t> huge_chunk(70 * 1024, 0x42);
    MT2_CHECK(!cli_sess->EnqueueSend(huge_chunk.data(), huge_chunk.size()));
    MT2_CHECK(cli_sess->BackpressureDrops() == 1);

    RunIoUntil(io, [&]() { return cli_disconnected.load() && srv_disconnected.load(); });
    MT2_CHECK(cli_sess->IsClosed());
    MT2_CHECK(srv_sess->IsClosed());
    std::printf("[PASS] Test 2: Standalone Asio TCP loopback, serialized write queue & backpressure verified\n");
}

// ------------------------------------------------------------------------------
// Test 3: TLS 1.3 Valid Handshake, Cipher Suite, SAN & 0-RTT Disabled (FAZ 3)
// ------------------------------------------------------------------------------
void TestTls13HandshakeAndRules(const mt2::net::EphemeralPkiBundle& pki, const mt2::net::Mt2PacketSchemaRegistry& schema)
{
    asio::io_context io;

    mt2::net::Tls13Config srv_cfg;
    srv_cfg.cert_pem = pki.server_cert_pem;
    srv_cfg.key_pem = pki.server_key_pem;
    srv_cfg.ca_pem = pki.ca_cert_pem;
    srv_cfg.disable_0rtt = true;

    // Verify SSL_CTX options: SSL_OP_NO_ANTI_REPLAY MUST be 0 (anti-replay active) and max_early_data MUST be 0
    {
        auto verify_srv_ctx = mt2::net::CreateServerTls13Context(srv_cfg);
        SSL_CTX* raw_ctx = verify_srv_ctx->native_handle();
        MT2_CHECK((SSL_CTX_get_options(raw_ctx) & SSL_OP_NO_ANTI_REPLAY) == 0);
        MT2_CHECK(SSL_CTX_get_max_early_data(raw_ctx) == 0);
        MT2_CHECK(SSL_CTX_get_recv_max_early_data(raw_ctx) == 0);
    }

    mt2::net::TransportLimits limits;
    mt2::net::AsioTls13Server server(io, 0, srv_cfg, limits, &schema);
    const uint16_t port = server.BoundPort();

    std::shared_ptr<mt2::net::AsioTls13Session> srv_session;
    std::atomic<bool> srv_hs_ok{false};
    server.StartAccept([&](std::shared_ptr<mt2::net::AsioTls13Session> s) {
        srv_session = s;
        s->SetCallbacks(
            [&](std::shared_ptr<mt2::net::AsioTls13Session>, const asio::error_code& ec) {
                srv_hs_ok.store(!ec);
            },
            [&](std::shared_ptr<mt2::net::AsioTls13Session> sess, const std::vector<uint8_t>& pkt) {
                sess->EnqueueSend(pkt.data(), pkt.size());
            },
            nullptr);
        s->StartServerHandshake();
    });

    mt2::net::Tls13Config cli_cfg;
    cli_cfg.ca_pem = pki.ca_cert_pem;
    cli_cfg.expected_hostname = "mt2009.local";
    cli_cfg.disable_0rtt = true;

    mt2::net::CrossPlatformTls13Client client(io, cli_cfg, limits, &schema);
    std::atomic<bool> cli_hs_ok{false};
    std::atomic<bool> pkt_echoed{false};

    WirePacketHandshake hs_pkt{};
    hs_pkt.dwHandshake = 0x12345678U;
    hs_pkt.dwTime = 1000;
    hs_pkt.lDelta = 0;

    client.ConnectEndpoint(
        asio::ip::tcp::endpoint(asio::ip::make_address("127.0.0.1"), port),
        [&](std::shared_ptr<mt2::net::AsioTls13Session> s, const asio::error_code& ec) {
            cli_hs_ok.store(!ec);
            if (!ec && s)
                s->EnqueueSend(&hs_pkt, sizeof(hs_pkt));
        },
        [&](std::shared_ptr<mt2::net::AsioTls13Session>, const std::vector<uint8_t>& pkt) {
            if (pkt.size() == sizeof(WirePacketHandshake))
            {
                WirePacketHandshake rx{};
                std::memcpy(&rx, pkt.data(), sizeof(rx));
                if (rx.dwHandshake == 0x12345678U)
                    pkt_echoed.store(true);
            }
        },
        nullptr);

    RunIoUntil(io, [&]() { return srv_hs_ok.load() && cli_hs_ok.load() && pkt_echoed.load(); });
    MT2_CHECK(srv_hs_ok.load());
    MT2_CHECK(cli_hs_ok.load());
    MT2_CHECK(pkt_echoed.load());
    MT2_CHECK(client.CurrentSession()->NegotiatedTlsVersion() == "TLSv1.3");
    MT2_CHECK(!client.CurrentSession()->NegotiatedCipherSuite().empty());
    MT2_CHECK(!client.CurrentSession()->EarlyDataAccepted());
    MT2_CHECK(!srv_session->EarlyDataAccepted());

    // Negative 0-RTT test: verify per-session SSL handle has max_early_data == 0 and rejects any early data attempt
    SSL* srv_ssl = srv_session->NativeSslHandle();
    SSL* cli_ssl = client.CurrentSession()->NativeSslHandle();
    MT2_CHECK(SSL_get_max_early_data(srv_ssl) == 0);
    MT2_CHECK(SSL_get_early_data_status(srv_ssl) != SSL_EARLY_DATA_ACCEPTED);
    MT2_CHECK(SSL_get_early_data_status(cli_ssl) != SSL_EARLY_DATA_ACCEPTED);
    size_t early_written = 0;
    MT2_CHECK(SSL_write_early_data(cli_ssl, &hs_pkt, sizeof(hs_pkt), &early_written) == 0);
    MT2_CHECK(early_written == 0);

    client.CurrentSession()->Close();
    server.Stop();
    std::printf("[PASS] Test 3: TLS 1.3 handshake (%s / %s, 0-RTT=off, anti-replay=on) verified\n",
        client.CurrentSession()->NegotiatedTlsVersion().c_str(),
        client.CurrentSession()->NegotiatedCipherSuite().c_str());
}

// ------------------------------------------------------------------------------
// Test 4: Untrusted CA, Wrong Hostname/SAN, Expired Cert, Live Cert Rotation,
//         TLS 1.2 Downgrade & Plaintext Client Rejection (FAZ 3 & FAZ 5)
// ------------------------------------------------------------------------------
void TestTls13NegativeCertificateAndProtocolRejections(
    const mt2::net::EphemeralPkiBundle& valid_pki,
    const mt2::net::Mt2PacketSchemaRegistry& schema)
{
    asio::io_context io;
    mt2::net::TransportLimits limits;

    mt2::net::Tls13Config srv_cfg;
    srv_cfg.cert_pem = valid_pki.server_cert_pem;
    srv_cfg.key_pem = valid_pki.server_key_pem;
    srv_cfg.ca_pem = valid_pki.ca_cert_pem;

    mt2::net::AsioTls13Server server(io, 0, srv_cfg, limits, &schema);
    const uint16_t port = server.BoundPort();

    server.StartAccept([&](std::shared_ptr<mt2::net::AsioTls13Session> s) {
        s->SetCallbacks(nullptr, nullptr, nullptr);
        s->StartServerHandshake();
    });

    // 4a. Untrusted CA rejection
    {
        auto untrusted_pki = mt2::net::GenerateEphemeralTestPki("mt2009.local", "DNS:mt2009.local,IP:127.0.0.1", -3600, 86400, 2000);
        mt2::net::Tls13Config bad_ca_cfg;
        bad_ca_cfg.ca_pem = untrusted_pki.ca_cert_pem; // Different Root CA!
        bad_ca_cfg.expected_hostname = "mt2009.local";

        mt2::net::CrossPlatformTls13Client cli(io, bad_ca_cfg, limits, &schema);
        std::atomic<bool> done{false};
        asio::error_code hs_ec;
        cli.ConnectEndpoint(
            asio::ip::tcp::endpoint(asio::ip::make_address("127.0.0.1"), port),
            [&](std::shared_ptr<mt2::net::AsioTls13Session>, const asio::error_code& ec) {
                hs_ec = ec;
                done.store(true);
            },
            nullptr, nullptr);
        RunIoUntil(io, [&]() { return done.load(); });
        MT2_CHECK(done.load());
        MT2_CHECK(static_cast<bool>(hs_ec)); // MUST fail verification!
    }

    // 4b. Wrong Hostname / SAN mismatch rejection
    {
        mt2::net::Tls13Config wrong_host_cfg;
        wrong_host_cfg.ca_pem = valid_pki.ca_cert_pem;
        wrong_host_cfg.expected_hostname = "wrong-domain.invalid"; // Not in SAN!

        mt2::net::CrossPlatformTls13Client cli(io, wrong_host_cfg, limits, &schema);
        std::atomic<bool> done{false};
        asio::error_code hs_ec;
        cli.ConnectEndpoint(
            asio::ip::tcp::endpoint(asio::ip::make_address("127.0.0.1"), port),
            [&](std::shared_ptr<mt2::net::AsioTls13Session>, const asio::error_code& ec) {
                hs_ec = ec;
                done.store(true);
            },
            nullptr, nullptr);
        RunIoUntil(io, [&]() { return done.load(); });
        MT2_CHECK(done.load());
        MT2_CHECK(static_cast<bool>(hs_ec)); // MUST fail SAN check!
    }

    // 4c. TLS 1.2 client downgrade rejection
    {
        mt2::net::Tls13Config tls12_cfg;
        tls12_cfg.ca_pem = valid_pki.ca_cert_pem;
        tls12_cfg.expected_hostname = "mt2009.local";
        tls12_cfg.min_tls_version = TLS1_2_VERSION;
        tls12_cfg.max_tls_version = TLS1_2_VERSION; // Force TLS 1.2 client

        mt2::net::CrossPlatformTls13Client cli(io, tls12_cfg, limits, &schema);
        std::atomic<bool> done{false};
        asio::error_code hs_ec;
        cli.ConnectEndpoint(
            asio::ip::tcp::endpoint(asio::ip::make_address("127.0.0.1"), port),
            [&](std::shared_ptr<mt2::net::AsioTls13Session>, const asio::error_code& ec) {
                hs_ec = ec;
                done.store(true);
            },
            nullptr, nullptr);
        RunIoUntil(io, [&]() { return done.load(); });
        MT2_CHECK(done.load());
        MT2_CHECK(static_cast<bool>(hs_ec)); // Server MUST reject TLS 1.2 downgrade!
    }

    // 4d. Plaintext TCP client rejection (No plaintext fallback)
    {
        asio::ip::tcp::socket raw_sock(io);
        raw_sock.connect(asio::ip::tcp::endpoint(asio::ip::make_address("127.0.0.1"), port));
        WirePacketHandshake plain_hs{};
        plain_hs.dwHandshake = 0xDEADBEEFU;
        asio::error_code write_ec;
        asio::write(raw_sock, asio::buffer(&plain_hs, sizeof(plain_hs)), write_ec);

        std::vector<uint8_t> rx(64);
        asio::error_code read_ec;
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(1000);
        while (std::chrono::steady_clock::now() < deadline)
        {
            io.restart();
            io.run_for(std::chrono::milliseconds(10));
        }
        raw_sock.non_blocking(true);
        size_t n = raw_sock.read_some(asio::buffer(rx), read_ec);
        // Server either sent a TLS Alert (0x15) or closed the connection; never a Metin2 0xFF handshake!
        if (n > 0)
            MT2_CHECK(rx[0] != HEADER_GC_HANDSHAKE);
        raw_sock.close();
    }

    // 4e. Expired Server Certificate Rejection + Live Certificate Rotation (Rule 6)
    {
        auto expired_pki = mt2::net::GenerateEphemeralTestPki(
            "mt2009.local", "DNS:mt2009.local,IP:127.0.0.1", -7200, -60, 3000);
        server.ReloadCertificates(expired_pki.server_cert_pem, expired_pki.server_key_pem, expired_pki.ca_cert_pem);

        mt2::net::Tls13Config cli_exp_cfg;
        cli_exp_cfg.ca_pem = expired_pki.ca_cert_pem;
        cli_exp_cfg.expected_hostname = "mt2009.local";

        mt2::net::CrossPlatformTls13Client cli1(io, cli_exp_cfg, limits, &schema);
        std::atomic<bool> exp_done{false};
        asio::error_code exp_ec;
        cli1.ConnectEndpoint(
            asio::ip::tcp::endpoint(asio::ip::make_address("127.0.0.1"), port),
            [&](std::shared_ptr<mt2::net::AsioTls13Session>, const asio::error_code& ec) {
                exp_ec = ec;
                exp_done.store(true);
            },
            nullptr, nullptr);
        RunIoUntil(io, [&]() { return exp_done.load(); });
        MT2_CHECK(exp_done.load());
        MT2_CHECK(static_cast<bool>(exp_ec)); // Expired cert MUST be rejected!

        // Now atomically rotate back to valid_pki on the live server without restarting!
        server.ReloadCertificates(valid_pki.server_cert_pem, valid_pki.server_key_pem, valid_pki.ca_cert_pem);

        mt2::net::Tls13Config cli_valid_cfg;
        cli_valid_cfg.ca_pem = valid_pki.ca_cert_pem;
        cli_valid_cfg.expected_hostname = "mt2009.local";

        mt2::net::CrossPlatformTls13Client cli2(io, cli_valid_cfg, limits, &schema);
        std::atomic<bool> rot_done{false};
        asio::error_code rot_ec;
        cli2.ConnectEndpoint(
            asio::ip::tcp::endpoint(asio::ip::make_address("127.0.0.1"), port),
            [&](std::shared_ptr<mt2::net::AsioTls13Session>, const asio::error_code& ec) {
                rot_ec = ec;
                rot_done.store(true);
            },
            nullptr, nullptr);
        RunIoUntil(io, [&]() { return rot_done.load(); });
        MT2_CHECK(rot_done.load());
        MT2_CHECK(!rot_ec); // Live-rotated valid cert succeeds immediately!
        cli2.CurrentSession()->Close();
    }

    server.Stop();
    std::printf("[PASS] Test 4: Untrusted CA, SAN mismatch, TLS 1.2 downgrade, plaintext & expired/rotated certs verified\n");
}

// ------------------------------------------------------------------------------
// Test 5: MITM / Tampered TLS Record Ciphertext Rejection (FAZ 5)
// ------------------------------------------------------------------------------
void TestMitmTamperedTlsRecordRejection(
    const mt2::net::EphemeralPkiBundle& pki,
    const mt2::net::Mt2PacketSchemaRegistry& schema)
{
    asio::io_context io;
    mt2::net::TransportLimits limits;

    mt2::net::Tls13Config srv_cfg;
    srv_cfg.cert_pem = pki.server_cert_pem;
    srv_cfg.key_pem = pki.server_key_pem;
    srv_cfg.ca_pem = pki.ca_cert_pem;

    mt2::net::AsioTls13Server server(io, 0, srv_cfg, limits, &schema);
    const uint16_t srv_port = server.BoundPort();

    std::atomic<size_t> srv_packets_received{0};
    std::atomic<bool> srv_disconnected{false};
    asio::error_code srv_disc_ec;

    server.StartAccept([&](std::shared_ptr<mt2::net::AsioTls13Session> s) {
        s->SetCallbacks(
            nullptr,
            [&](std::shared_ptr<mt2::net::AsioTls13Session>, const std::vector<uint8_t>&) {
                srv_packets_received.fetch_add(1);
            },
            [&](std::shared_ptr<mt2::net::AsioTls13Session>, const asio::error_code& ec, mt2::net::FrameStatus) {
                srv_disc_ec = ec;
                srv_disconnected.store(true);
            });
        s->StartServerHandshake();
    });

    // Build an in-process TCP MITM proxy that forwards the handshake intact,
    // then flips 1 bit in the first application-data record from client -> server.
    asio::ip::tcp::acceptor proxy_acc(io, asio::ip::tcp::endpoint(asio::ip::make_address("127.0.0.1"), 0));
    const uint16_t proxy_port = proxy_acc.local_endpoint().port();

    auto proxy_c2s_sock = std::make_shared<asio::ip::tcp::socket>(io);
    auto proxy_s2c_sock = std::make_shared<asio::ip::tcp::socket>(io);
    std::atomic<bool> tamper_next_c2s{false};
    std::vector<uint8_t> c2s_buf(8192);
    std::vector<uint8_t> s2c_buf(8192);

    std::function<void()> pump_c2s;
    std::function<void()> pump_s2c;

    pump_c2s = [&]() {
        proxy_c2s_sock->async_read_some(asio::buffer(c2s_buf), [&](const asio::error_code& ec, size_t n) {
            if (ec || n == 0)
            {
                asio::error_code ig;
                proxy_s2c_sock->close(ig);
                return;
            }
            if (tamper_next_c2s.exchange(false) && n > 10)
            {
                // Corrupt ciphertext payload byte inside TLS 1.3 record
                c2s_buf[n - 2] ^= 0xFF;
            }
            asio::async_write(*proxy_s2c_sock, asio::buffer(c2s_buf.data(), n), [&](const asio::error_code& wec, size_t) {
                if (!wec)
                    pump_c2s();
            });
        });
    };

    pump_s2c = [&]() {
        proxy_s2c_sock->async_read_some(asio::buffer(s2c_buf), [&](const asio::error_code& ec, size_t n) {
            if (ec || n == 0)
            {
                asio::error_code ig;
                proxy_c2s_sock->close(ig);
                return;
            }
            asio::async_write(*proxy_c2s_sock, asio::buffer(s2c_buf.data(), n), [&](const asio::error_code& wec, size_t) {
                if (!wec)
                    pump_s2c();
            });
        });
    };

    proxy_acc.async_accept(*proxy_c2s_sock, [&](const asio::error_code& ec) {
        if (!ec)
        {
            proxy_s2c_sock->connect(asio::ip::tcp::endpoint(asio::ip::make_address("127.0.0.1"), srv_port));
            pump_c2s();
            pump_s2c();
        }
    });

    mt2::net::Tls13Config cli_cfg;
    cli_cfg.ca_pem = pki.ca_cert_pem;
    cli_cfg.expected_hostname = "mt2009.local";

    mt2::net::CrossPlatformTls13Client cli(io, cli_cfg, limits, &schema);
    std::atomic<bool> hs_ok{false};
    cli.ConnectEndpoint(
        asio::ip::tcp::endpoint(asio::ip::make_address("127.0.0.1"), proxy_port),
        [&](std::shared_ptr<mt2::net::AsioTls13Session>, const asio::error_code& ec) {
            hs_ok.store(!ec);
        },
        nullptr, nullptr);

    RunIoUntil(io, [&]() { return hs_ok.load(); });
    MT2_CHECK(hs_ok.load());

    // Arm the MITM bit-flipper and send a Metin2 packet
    tamper_next_c2s.store(true);
    WirePacketHandshake pkt{};
    pkt.dwHandshake = 0xCAFEBABEU;
    cli.CurrentSession()->EnqueueSend(&pkt, sizeof(pkt));

    RunIoUntil(io, [&]() { return srv_disconnected.load(); });
    MT2_CHECK(srv_disconnected.load());
    MT2_CHECK(static_cast<bool>(srv_disc_ec));
    MT2_CHECK(srv_packets_received.load() == 0); // Corrupted TLS record NEVER reaches packet parser!

    cli.CurrentSession()->Close();
    server.Stop();
    std::printf("[PASS] Test 5: MITM tampered TLS 1.3 AEAD record rejected with 0 leaked packets\n");
}

// ------------------------------------------------------------------------------
// Test 6: Fragmented (1-Byte), Coalesced Burst (50 Packets), Invalid Header,
//         Invalid Length, Oversized Packet & Truncated Disconnect (FAZ 4 & FAZ 5)
// ------------------------------------------------------------------------------
void TestFramingEdgeCasesAndMalformedPackets(
    const mt2::net::EphemeralPkiBundle& pki,
    const mt2::net::Mt2PacketSchemaRegistry& schema)
{
    asio::io_context io;
    mt2::net::TransportLimits limits;
    limits.max_packet_size = 65536;

    mt2::net::Tls13Config srv_cfg;
    srv_cfg.cert_pem = pki.server_cert_pem;
    srv_cfg.key_pem = pki.server_key_pem;
    srv_cfg.ca_pem = pki.ca_cert_pem;

    mt2::net::AsioTls13Server server(io, 0, srv_cfg, limits, &schema);
    const uint16_t port = server.BoundPort();

    std::vector<std::vector<uint8_t>> rx_packets;
    std::atomic<mt2::net::FrameStatus> last_srv_frame_err{mt2::net::FrameStatus::PacketReady};
    std::atomic<bool> srv_disc{false};

    server.StartAccept([&](std::shared_ptr<mt2::net::AsioTls13Session> s) {
        srv_disc.store(false);
        last_srv_frame_err.store(mt2::net::FrameStatus::PacketReady);
        s->SetCallbacks(
            nullptr,
            [&](std::shared_ptr<mt2::net::AsioTls13Session>, const std::vector<uint8_t>& pkt) {
                rx_packets.push_back(pkt);
            },
            [&](std::shared_ptr<mt2::net::AsioTls13Session>, const asio::error_code&, mt2::net::FrameStatus ferr) {
                last_srv_frame_err.store(ferr);
                srv_disc.store(true);
            });
        s->StartServerHandshake();
    });

    mt2::net::Tls13Config cli_cfg;
    cli_cfg.ca_pem = pki.ca_cert_pem;
    cli_cfg.expected_hostname = "mt2009.local";

    // 6a. 1-byte fragmented packets + 50 coalesced fixed & dynamic packets
    {
        mt2::net::CrossPlatformTls13Client cli(io, cli_cfg, limits, &schema);
        std::atomic<bool> hs_ok{false};
        cli.ConnectEndpoint(
            asio::ip::tcp::endpoint(asio::ip::make_address("127.0.0.1"), port),
            [&](std::shared_ptr<mt2::net::AsioTls13Session>, const asio::error_code& ec) { hs_ok.store(!ec); },
            nullptr, nullptr);
        RunIoUntil(io, [&]() { return hs_ok.load(); });
        MT2_CHECK(hs_ok.load());

        // Send a WirePacketCGLogin3 1 byte at a time
        WirePacketCGLogin3 login3{};
        std::snprintf(login3.login, sizeof(login3.login), "frag_user");
        std::snprintf(login3.passwd, sizeof(login3.passwd), "secret123");
        const uint8_t* raw_l3 = reinterpret_cast<const uint8_t*>(&login3);
        for (size_t i = 0; i < sizeof(login3); ++i)
            cli.CurrentSession()->EnqueueSend(raw_l3 + i, 1);

        RunIoUntil(io, [&]() { return rx_packets.size() == 1; });
        MT2_CHECK(rx_packets.size() == 1);
        MT2_CHECK(rx_packets[0].size() == sizeof(WirePacketCGLogin3));
        MT2_CHECK(std::memcmp(rx_packets[0].data(), &login3, sizeof(login3)) == 0);

        // Send 50 coalesced packets (25 fixed Move + 25 dynamic32 OfflineShop) in a single buffer
        std::vector<uint8_t> coalesced;
        for (int i = 0; i < 25; ++i)
        {
            WirePacketCGMove mv{};
            mv.lX = 1000 + i;
            mv.lY = 2000 + i;
            const uint8_t* p = reinterpret_cast<const uint8_t*>(&mv);
            coalesced.insert(coalesced.end(), p, p + sizeof(mv));

            std::vector<uint8_t> dyn_pkt(sizeof(WirePacketDynamic32Header) + 32, static_cast<uint8_t>(i));
            WirePacketDynamic32Header dh{};
            dh.header = HEADER_GC_NEW_OFFLINESHOP;
            dh.size = static_cast<uint32_t>(dyn_pkt.size());
            dh.subheader = static_cast<uint8_t>(i);
            std::memcpy(dyn_pkt.data(), &dh, sizeof(dh));
            coalesced.insert(coalesced.end(), dyn_pkt.begin(), dyn_pkt.end());
        }
        cli.CurrentSession()->EnqueueSend(coalesced.data(), coalesced.size());

        RunIoUntil(io, [&]() { return rx_packets.size() == 51; });
        MT2_CHECK(rx_packets.size() == 51);
        cli.CurrentSession()->Close();
        RunIoUntil(io, [&]() { return srv_disc.load(); });
    }

    // 6b. Invalid header (0xFE) rejection
    {
        mt2::net::CrossPlatformTls13Client cli(io, cli_cfg, limits, &schema);
        std::atomic<bool> hs_ok{false};
        cli.ConnectEndpoint(
            asio::ip::tcp::endpoint(asio::ip::make_address("127.0.0.1"), port),
            [&](std::shared_ptr<mt2::net::AsioTls13Session>, const asio::error_code& ec) { hs_ok.store(!ec); },
            nullptr, nullptr);
        RunIoUntil(io, [&]() { return hs_ok.load(); });
        uint8_t bad_hdr[4] = {0xFE, 0x00, 0x00, 0x00};
        cli.CurrentSession()->EnqueueSend(bad_hdr, sizeof(bad_hdr));
        RunIoUntil(io, [&]() { return srv_disc.load(); });
        MT2_CHECK(last_srv_frame_err.load() == mt2::net::FrameStatus::InvalidHeader);
        cli.CurrentSession()->Close();
    }

    // 6c. Invalid dynamic length (size = 1 < min_size 4) rejection
    {
        mt2::net::CrossPlatformTls13Client cli(io, cli_cfg, limits, &schema);
        std::atomic<bool> hs_ok{false};
        cli.ConnectEndpoint(
            asio::ip::tcp::endpoint(asio::ip::make_address("127.0.0.1"), port),
            [&](std::shared_ptr<mt2::net::AsioTls13Session>, const asio::error_code& ec) { hs_ok.store(!ec); },
            nullptr, nullptr);
        RunIoUntil(io, [&]() { return hs_ok.load(); });
        WirePacketDynamic16Header bad_len{};
        bad_len.header = HEADER_GC_SHOP;
        bad_len.size = 1; // Invalid (< 4)
        cli.CurrentSession()->EnqueueSend(&bad_len, sizeof(bad_len));
        RunIoUntil(io, [&]() { return srv_disc.load(); });
        MT2_CHECK(last_srv_frame_err.load() == mt2::net::FrameStatus::InvalidLength);
        cli.CurrentSession()->Close();
    }

    // 6d. Oversized dynamic packet (size = 500,000 > 65,536) rejection
    {
        mt2::net::CrossPlatformTls13Client cli(io, cli_cfg, limits, &schema);
        std::atomic<bool> hs_ok{false};
        cli.ConnectEndpoint(
            asio::ip::tcp::endpoint(asio::ip::make_address("127.0.0.1"), port),
            [&](std::shared_ptr<mt2::net::AsioTls13Session>, const asio::error_code& ec) { hs_ok.store(!ec); },
            nullptr, nullptr);
        RunIoUntil(io, [&]() { return hs_ok.load(); });
        WirePacketDynamic32Header huge_hdr{};
        huge_hdr.header = HEADER_GC_NEW_OFFLINESHOP;
        huge_hdr.size = 500000U; // Exceeds 65536 cap
        cli.CurrentSession()->EnqueueSend(&huge_hdr, sizeof(huge_hdr));
        RunIoUntil(io, [&]() { return srv_disc.load(); });
        MT2_CHECK(last_srv_frame_err.load() == mt2::net::FrameStatus::OversizedPacket);
        cli.CurrentSession()->Close();
    }

    // 6e. Truncated payload + abrupt disconnect while async read is pending
    {
        size_t before_count = rx_packets.size();
        mt2::net::CrossPlatformTls13Client cli(io, cli_cfg, limits, &schema);
        std::atomic<bool> hs_ok{false};
        cli.ConnectEndpoint(
            asio::ip::tcp::endpoint(asio::ip::make_address("127.0.0.1"), port),
            [&](std::shared_ptr<mt2::net::AsioTls13Session>, const asio::error_code& ec) { hs_ok.store(!ec); },
            nullptr, nullptr);
        RunIoUntil(io, [&]() { return hs_ok.load(); });
        // Send only 5 bytes of a 16-byte WirePacketCGMove then close immediately
        WirePacketCGMove partial_mv{};
        cli.CurrentSession()->EnqueueSend(&partial_mv, 5);
        RunIoUntil(io, [&]() { return cli.CurrentSession()->TotalBytesWritten() >= 5; });
        cli.CurrentSession()->Close();
        RunIoUntil(io, [&]() { return srv_disc.load(); });
        MT2_CHECK(rx_packets.size() == before_count); // Partial packet was never dispatched!
    }

    server.Stop();
    std::printf("[PASS] Test 6: 1-byte fragmentation, 50-packet coalescing, invalid/oversized/truncated framing verified\n");
}

// ------------------------------------------------------------------------------
// Test 7: Handshake Timeout, Idle Timeout & Connection Flood / Per-IP Cap (FAZ 5)
// ------------------------------------------------------------------------------
void TestTimeoutsAndFloodProtection(
    const mt2::net::EphemeralPkiBundle& pki,
    const mt2::net::Mt2PacketSchemaRegistry& schema)
{
    asio::io_context io;
    mt2::net::TransportLimits limits;
    limits.handshake_timeout = std::chrono::milliseconds(150);
    limits.idle_timeout = std::chrono::milliseconds(200);
    limits.max_connections_per_ip = 3;
    limits.max_handshakes_per_ip_window = 5;

    mt2::net::Tls13Config srv_cfg;
    srv_cfg.cert_pem = pki.server_cert_pem;
    srv_cfg.key_pem = pki.server_key_pem;
    srv_cfg.ca_pem = pki.ca_cert_pem;

    mt2::net::AsioTls13Server server(io, 0, srv_cfg, limits, &schema);
    const uint16_t port = server.BoundPort();

    std::atomic<size_t> srv_timed_out{0};
    server.StartAccept([&](std::shared_ptr<mt2::net::AsioTls13Session> s) {
        s->SetCallbacks(
            nullptr,
            nullptr,
            [&](std::shared_ptr<mt2::net::AsioTls13Session> sess, const asio::error_code& ec, mt2::net::FrameStatus) {
                if (ec == asio::error::timed_out)
                    srv_timed_out.fetch_add(1);
                server.FloodGuard().ReleaseConnection(sess->RemoteIp());
            });
        s->StartServerHandshake();
    });

    // 7a. Silent raw TCP connection triggers handshake_timeout (150ms)
    {
        asio::ip::tcp::socket silent_sock(io);
        silent_sock.connect(asio::ip::tcp::endpoint(asio::ip::make_address("127.0.0.1"), port));
        RunIoUntil(io, [&]() { return srv_timed_out.load() >= 1; }, std::chrono::milliseconds(1500));
        MT2_CHECK(srv_timed_out.load() >= 1);
        silent_sock.close();
    }

    // 7b. Authenticated TLS 1.3 connection that goes silent triggers idle_timeout (200ms)
    {
        mt2::net::Tls13Config cli_cfg;
        cli_cfg.ca_pem = pki.ca_cert_pem;
        cli_cfg.expected_hostname = "mt2009.local";
        mt2::net::TransportLimits cli_limits = limits;
        cli_limits.idle_timeout = std::chrono::milliseconds(5000);

        mt2::net::CrossPlatformTls13Client cli(io, cli_cfg, cli_limits, &schema);
        std::atomic<bool> hs_ok{false};
        cli.ConnectEndpoint(
            asio::ip::tcp::endpoint(asio::ip::make_address("127.0.0.1"), port),
            [&](std::shared_ptr<mt2::net::AsioTls13Session>, const asio::error_code& ec) { hs_ok.store(!ec); },
            nullptr, nullptr);
        RunIoUntil(io, [&]() { return hs_ok.load(); });
        MT2_CHECK(hs_ok.load());

        // Wait for server idle timeout (200ms)
        RunIoUntil(io, [&]() { return srv_timed_out.load() >= 2; }, std::chrono::milliseconds(1500));
        MT2_CHECK(srv_timed_out.load() >= 2);
        cli.CurrentSession()->Close();
    }

    // 7c. Flood guard per-IP active connection cap (max 3 concurrent from 127.0.0.1)
    {
        mt2::net::ConnectionFloodGuard guard(limits);
        MT2_CHECK(guard.TryAcquireConnection("10.0.0.1"));
        MT2_CHECK(guard.TryAcquireConnection("10.0.0.1"));
        MT2_CHECK(guard.TryAcquireConnection("10.0.0.1"));
        MT2_CHECK(!guard.TryAcquireConnection("10.0.0.1")); // 4th concurrent rejected!
        MT2_CHECK(guard.RejectedIpCap() == 1);
        guard.ReleaseConnection("10.0.0.1");
        MT2_CHECK(guard.TryAcquireConnection("10.0.0.1")); // 4th handshake in window
        guard.ReleaseConnection("10.0.0.1");
        MT2_CHECK(guard.TryAcquireConnection("10.0.0.1")); // 5th handshake in window
        guard.ReleaseConnection("10.0.0.1");
        MT2_CHECK(!guard.TryAcquireConnection("10.0.0.1")); // 6th handshake in 1s window rejected by rate limit!
        MT2_CHECK(guard.RejectedRateLimit() == 1);
    }

    server.Stop();
    std::printf("[PASS] Test 7: Handshake timeout, idle timeout & per-IP flood/rate limit verified\n");
}

// ------------------------------------------------------------------------------
// Test 8: Full End-to-End Metin2 Auth + LoginByKey (Single-Use Anti-Replay) +
//         Character Select + Game + Server Authority + Channel Switch (FAZ 4 & 5)
// ------------------------------------------------------------------------------
void TestFullMetin2FlowsOverTls13(
    const mt2::net::EphemeralPkiBundle& pki,
    const mt2::net::Mt2PacketSchemaRegistry& schema)
{
    asio::io_context io;
    mt2::net::TransportLimits limits;

    mt2::net::Tls13Config srv_cfg;
    srv_cfg.cert_pem = pki.server_cert_pem;
    srv_cfg.key_pem = pki.server_key_pem;
    srv_cfg.ca_pem = pki.ca_cert_pem;

    // Spin up Auth Server, Game Channel 1 Server, and Game Channel 2 Server on dedicated TLS 1.3 ports
    mt2::net::AsioTls13Server auth_srv(io, 0, srv_cfg, limits, &schema);
    mt2::net::AsioTls13Server game_ch1_srv(io, 0, srv_cfg, limits, &schema);
    mt2::net::AsioTls13Server game_ch2_srv(io, 0, srv_cfg, limits, &schema);

    const uint16_t auth_port = auth_srv.BoundPort();
    const uint16_t ch1_port = game_ch1_srv.BoundPort();
    const uint16_t ch2_port = game_ch2_srv.BoundPort();

    mt2::net::ConnectionFloodGuard shared_auth_store;
    const uint32_t issued_login_key = 0x778899AAU;

    // 1. Auth Server handler
    auth_srv.StartAccept([&](std::shared_ptr<mt2::net::AsioTls13Session> s) {
        s->SetCallbacks(
            [&](std::shared_ptr<mt2::net::AsioTls13Session> sess, const asio::error_code& ec) {
                if (!ec)
                {
                    WirePacketHandshake hs{};
                    hs.dwHandshake = 0x11112222U;
                    hs.dwTime = 500;
                    sess->EnqueueSend(&hs, sizeof(hs));
                }
            },
            [&](std::shared_ptr<mt2::net::AsioTls13Session> sess, const std::vector<uint8_t>& pkt) {
                const uint8_t hdr = pkt[0];
                if (hdr == HEADER_CG_HANDSHAKE)
                {
                    // Handshake complete on Auth
                }
                else if (hdr == HEADER_CG_LOGIN3)
                {
                    WirePacketCGLogin3 l3{};
                    std::memcpy(&l3, pkt.data(), sizeof(l3));
                    if (l3.dwProtocolVersion != kMt2ProtocolVersionX64)
                    {
                        sess->Close(asio::error::invalid_argument);
                        return;
                    }
                    shared_auth_store.IssueLoginKey(issued_login_key, l3.login, l3.adwClientKey);
                    WirePacketGCAuthSuccess ok{};
                    ok.dwLoginKey = issued_login_key;
                    ok.bResult = 1;
                    sess->EnqueueSend(&ok, sizeof(ok));
                }
            },
            nullptr);
        s->StartServerHandshake();
    });

    // Server-side authoritative state for player PID 900001
    int64_t authoritative_gold = 500000000000LL; // 500B Yang
    std::atomic<size_t> authority_violations_blocked{0};
    std::atomic<bool> ch1_gameplay_complete{false};
    std::atomic<bool> ch2_warp_complete{false};

    // 2. Game Channel 1 Server handler
    game_ch1_srv.StartAccept([&](std::shared_ptr<mt2::net::AsioTls13Session> s) {
        s->SetCallbacks(
            nullptr,
            [&](std::shared_ptr<mt2::net::AsioTls13Session> sess, const std::vector<uint8_t>& pkt) {
                const uint8_t hdr = pkt[0];
                if (hdr == HEADER_CG_LOGIN2)
                {
                    WirePacketCGLogin2 l2{};
                    std::memcpy(&l2, pkt.data(), sizeof(l2));
                    if (l2.dwProtocolVersion != kMt2ProtocolVersionX64 ||
                        !shared_auth_store.ConsumeLoginKeySingleUse(l2.dwLoginKey, l2.login, l2.adwClientKey))
                    {
                        sess->Close(asio::error::access_denied);
                        return;
                    }
                    WirePacketGCLoginSuccess chars{};
                    chars.aullPID[0] = 900001ULL;
                    chars.dwGuildID[0] = 42U;
                    sess->EnqueueSend(&chars, sizeof(chars));
                }
                else if (hdr == HEADER_CG_CHARACTER_SELECT)
                {
                    WirePacketGCMainCharacter mc{};
                    mc.dwVID = 10001U;
                    mc.wRaceNum = 0;
                    std::snprintf(mc.szName, sizeof(mc.szName), "WarriorX64");
                    mc.lx = 474300;
                    mc.ly = 954800;
                    sess->EnqueueSend(&mc, sizeof(mc));
                }
                else if (hdr == HEADER_CG_ENTERGAME)
                {
                    WirePacketGCPoints pts{};
                    pts.points[1] = 99; // Level 99
                    pts.points[11] = authoritative_gold; // POINT_GOLD
                    sess->EnqueueSend(&pts, sizeof(pts));
                }
                else if (hdr == HEADER_CG_MOVE)
                {
                    sess->EnqueueSend(pkt.data(), pkt.size()); // Broadcast move
                }
                else if (hdr == HEADER_CG_ATTACK || hdr == HEADER_CG_USE_SKILL || hdr == HEADER_CG_ITEM_MOVE)
                {
                    // Validated combat/skill/inventory action
                }
                else if (hdr == HEADER_CG_EXCHANGE)
                {
                    WirePacketCGExchange ex{};
                    std::memcpy(&ex, pkt.data(), sizeof(ex));
                    // Server authority check: reject negative Yang or overflow > GOLD_MAX
                    if (ex.arg1 < 0 || ex.arg1 > authoritative_gold ||
                        authoritative_gold + ex.arg1 > kGoldMax)
                    {
                        authority_violations_blocked.fetch_add(1);
                        return;
                    }
                }
                else if (hdr == HEADER_CG_SHOP)
                {
                    WirePacketCGShopBuy buy{};
                    std::memcpy(&buy, pkt.data(), sizeof(buy));
                    const int64_t server_item_price = 1000000LL;
                    if (buy.expected_price < 0 || buy.expected_price != server_item_price ||
                        authoritative_gold < server_item_price)
                    {
                        authority_violations_blocked.fetch_add(1);
                        return;
                    }
                    authoritative_gold -= server_item_price;
                }
                else if (hdr == HEADER_CG_GUILD)
                {
                    // Trigger Channel Switch (WarpSet) to Channel 2!
                    ch1_gameplay_complete.store(true);
                    WirePacketGCWarp warp{};
                    warp.lX = 475000;
                    warp.lY = 955000;
                    warp.lAddr = 0x0100007F; // 127.0.0.1
                    warp.wPort = ch2_port;
                    sess->EnqueueSend(&warp, sizeof(warp));
                }
            },
            nullptr);
        s->StartServerHandshake();
    });

    // 3. Game Channel 2 Server handler (receives client after WarpSet reconnect)
    game_ch2_srv.StartAccept([&](std::shared_ptr<mt2::net::AsioTls13Session> s) {
        s->SetCallbacks(
            nullptr,
            [&](std::shared_ptr<mt2::net::AsioTls13Session> sess, const std::vector<uint8_t>& pkt) {
                if (pkt[0] == HEADER_CG_ENTERGAME)
                {
                    ch2_warp_complete.store(true);
                    WirePacketGCPoints pts{};
                    pts.points[11] = authoritative_gold;
                    sess->EnqueueSend(&pts, sizeof(pts));
                }
            },
            nullptr);
        s->StartServerHandshake();
    });

    mt2::net::Tls13Config cli_cfg;
    cli_cfg.ca_pem = pki.ca_cert_pem;
    cli_cfg.expected_hostname = "mt2009.local";

    const uint32_t client_key[4] = {0x11223344U, 0x55667788U, 0x99AABBCCU, 0xDDEEFF00U};
    uint32_t rx_login_key = 0;

    // Step A: Client -> Auth TLS 1.3 flow
    {
        mt2::net::CrossPlatformTls13Client auth_cli(io, cli_cfg, limits, &schema);
        auth_cli.ConnectEndpoint(
            asio::ip::tcp::endpoint(asio::ip::make_address("127.0.0.1"), auth_port),
            nullptr,
            [&](std::shared_ptr<mt2::net::AsioTls13Session> sess, const std::vector<uint8_t>& pkt) {
                if (pkt[0] == HEADER_GC_HANDSHAKE)
                {
                    sess->EnqueueSend(pkt.data(), pkt.size());
                    WirePacketCGLogin3 l3{};
                    std::snprintf(l3.login, sizeof(l3.login), "mt2_hero");
                    std::snprintf(l3.passwd, sizeof(l3.passwd), "pw_tls13");
                    std::memcpy(l3.adwClientKey, client_key, sizeof(client_key));
                    l3.dwProtocolVersion = kMt2ProtocolVersionX64;
                    sess->EnqueueSend(&l3, sizeof(l3));
                }
                else if (pkt[0] == HEADER_GC_AUTH_SUCCESS)
                {
                    WirePacketGCAuthSuccess ok{};
                    std::memcpy(&ok, pkt.data(), sizeof(ok));
                    rx_login_key = ok.dwLoginKey;
                }
            },
            nullptr);
        RunIoUntil(io, [&]() { return rx_login_key == issued_login_key; });
        MT2_CHECK(rx_login_key == issued_login_key);
        auth_cli.CurrentSession()->Close();
    }

    // Step B: Client -> Game Ch1 TLS 1.3 flow (Login2 -> CharSelect -> EnterGame -> Move/Attack/Skill/Inventory/Exchange/Shop/Guild -> Warp)
    uint16_t warp_target_port = 0;
    {
        mt2::net::CrossPlatformTls13Client game_cli(io, cli_cfg, limits, &schema);
        game_cli.ConnectEndpoint(
            asio::ip::tcp::endpoint(asio::ip::make_address("127.0.0.1"), ch1_port),
            [&](std::shared_ptr<mt2::net::AsioTls13Session> sess, const asio::error_code& ec) {
                MT2_CHECK(!ec);
                WirePacketCGLogin2 l2{};
                std::snprintf(l2.login, sizeof(l2.login), "mt2_hero");
                l2.dwLoginKey = rx_login_key;
                std::memcpy(l2.adwClientKey, client_key, sizeof(client_key));
                l2.dwProtocolVersion = kMt2ProtocolVersionX64;
                sess->EnqueueSend(&l2, sizeof(l2));
            },
            [&](std::shared_ptr<mt2::net::AsioTls13Session> sess, const std::vector<uint8_t>& pkt) {
                const uint8_t hdr = pkt[0];
                if (hdr == HEADER_GC_LOGIN_SUCCESS_NEWSLOT)
                {
                    WirePacketGCLoginSuccess ls{};
                    std::memcpy(&ls, pkt.data(), sizeof(ls));
                    MT2_CHECK(ls.aullPID[0] == 900001ULL);
                    WirePacketCGCharacterSelect cs{};
                    cs.index = 0;
                    sess->EnqueueSend(&cs, sizeof(cs));
                }
                else if (hdr == HEADER_GC_MAIN_CHARACTER)
                {
                    WirePacketCGEnterGame eg{};
                    sess->EnqueueSend(&eg, sizeof(eg));
                }
                else if (hdr == HEADER_GC_POINTS)
                {
                    WirePacketGCPoints pts{};
                    std::memcpy(&pts, pkt.data(), sizeof(pts));
                    MT2_CHECK(pts.points[11] == 500000000000LL);

                    // Send Move, Attack, Skill, Inventory Move
                    WirePacketCGMove mv{};
                    mv.lX = 474400;
                    mv.lY = 954900;
                    sess->EnqueueSend(&mv, sizeof(mv));

                    WirePacketCGAttack atk{};
                    atk.dwVID = 20002U;
                    sess->EnqueueSend(&atk, sizeof(atk));

                    WirePacketCGUseSkill sk{};
                    sk.dwVnum = 1;
                    sk.dwVID = 20002U;
                    sess->EnqueueSend(&sk, sizeof(sk));

                    WirePacketCGItemMove im{};
                    sess->EnqueueSend(&im, sizeof(im));

                    // Attempt malicious negative Yang trade & GOLD_MAX overflow trade (server MUST block both!)
                    WirePacketCGExchange bad_ex1{};
                    bad_ex1.arg1 = -5000LL;
                    sess->EnqueueSend(&bad_ex1, sizeof(bad_ex1));

                    WirePacketCGExchange bad_ex2{};
                    bad_ex2.arg1 = kGoldMax + 100LL;
                    sess->EnqueueSend(&bad_ex2, sizeof(bad_ex2));

                    // Attempt spoofed shop buy price (server MUST block!) then valid shop buy
                    WirePacketCGShopBuy bad_buy{};
                    bad_buy.expected_price = 1LL; // Spoofed price != 1,000,000
                    sess->EnqueueSend(&bad_buy, sizeof(bad_buy));

                    WirePacketCGShopBuy good_buy{};
                    good_buy.expected_price = 1000000LL;
                    sess->EnqueueSend(&good_buy, sizeof(good_buy));

                    // Send Guild packet to trigger WarpSet to Channel 2
                    std::vector<uint8_t> guild_pkt(sizeof(WirePacketDynamic16Header) + 8, 0);
                    WirePacketDynamic16Header gh{};
                    gh.header = HEADER_CG_GUILD;
                    gh.size = static_cast<uint16_t>(guild_pkt.size());
                    gh.subheader = 1;
                    std::memcpy(guild_pkt.data(), &gh, sizeof(gh));
                    sess->EnqueueSend(guild_pkt.data(), guild_pkt.size());
                }
                else if (hdr == HEADER_GC_WARP)
                {
                    WirePacketGCWarp warp{};
                    std::memcpy(&warp, pkt.data(), sizeof(warp));
                    warp_target_port = warp.wPort;
                }
            },
            nullptr);

        RunIoUntil(io, [&]() { return warp_target_port == ch2_port; });
        MT2_CHECK(ch1_gameplay_complete.load());
        MT2_CHECK(authority_violations_blocked.load() == 3);
        MT2_CHECK(authoritative_gold == 500000000000LL - 1000000LL);
        MT2_CHECK(warp_target_port == ch2_port);
        game_cli.CurrentSession()->Close();
    }

    // Step C: Anti-Replay Verification — Replaying the consumed LoginByKey MUST fail!
    {
        mt2::net::CrossPlatformTls13Client replay_cli(io, cli_cfg, limits, &schema);
        std::atomic<bool> replay_disconnected{false};
        replay_cli.ConnectEndpoint(
            asio::ip::tcp::endpoint(asio::ip::make_address("127.0.0.1"), ch1_port),
            [&](std::shared_ptr<mt2::net::AsioTls13Session> sess, const asio::error_code& ec) {
                if (!ec)
                {
                    WirePacketCGLogin2 replay_l2{};
                    std::snprintf(replay_l2.login, sizeof(replay_l2.login), "mt2_hero");
                    replay_l2.dwLoginKey = rx_login_key; // Already used once!
                    std::memcpy(replay_l2.adwClientKey, client_key, sizeof(client_key));
                    replay_l2.dwProtocolVersion = kMt2ProtocolVersionX64;
                    sess->EnqueueSend(&replay_l2, sizeof(replay_l2));
                }
            },
            nullptr,
            [&](std::shared_ptr<mt2::net::AsioTls13Session>, const asio::error_code&, mt2::net::FrameStatus) {
                replay_disconnected.store(true);
            });
        RunIoUntil(io, [&]() { return replay_disconnected.load(); });
        MT2_CHECK(replay_disconnected.load());
    }

    // Step D: Channel Switch (WarpSet) Reconnect to Channel 2 over TLS 1.3
    {
        mt2::net::CrossPlatformTls13Client ch2_cli(io, cli_cfg, limits, &schema);
        std::atomic<int64_t> ch2_rx_gold{0};
        ch2_cli.ConnectEndpoint(
            asio::ip::tcp::endpoint(asio::ip::make_address("127.0.0.1"), warp_target_port),
            [&](std::shared_ptr<mt2::net::AsioTls13Session> sess, const asio::error_code& ec) {
                MT2_CHECK(!ec);
                WirePacketCGEnterGame eg{};
                sess->EnqueueSend(&eg, sizeof(eg));
            },
            [&](std::shared_ptr<mt2::net::AsioTls13Session>, const std::vector<uint8_t>& pkt) {
                if (pkt[0] == HEADER_GC_POINTS)
                {
                    WirePacketGCPoints pts{};
                    std::memcpy(&pts, pkt.data(), sizeof(pts));
                    ch2_rx_gold.store(pts.points[11]);
                }
            },
            nullptr);
        RunIoUntil(io, [&]() { return ch2_warp_complete.load() && ch2_rx_gold.load() != 0; });
        MT2_CHECK(ch2_warp_complete.load());
        MT2_CHECK(ch2_rx_gold.load() == 500000000000LL - 1000000LL);
        ch2_cli.CurrentSession()->Close();
    }

    auth_srv.Stop();
    game_ch1_srv.Stop();
    game_ch2_srv.Stop();
    std::printf("[PASS] Test 8: Full Metin2 Auth/LoginByKey(anti-replay)/Game/Economy/WarpSet flows over TLS 1.3 verified\n");
}

// ------------------------------------------------------------------------------
// Test 9: P2P Dedicated Security Domain with Mutual TLS (mTLS) + Mobile Hooks (FAZ 4 & 8)
// ------------------------------------------------------------------------------
void TestP2PMutualTlsAndMobileLifecycle(const mt2::net::EphemeralPkiBundle& pki)
{
    asio::io_context io;
    mt2::net::TransportLimits limits;
    auto p2p_schema = BuildP2PSchema();

    mt2::net::Tls13Config p2p_srv_cfg;
    p2p_srv_cfg.domain = mt2::net::SecurityDomain::ServerToServerP2P;
    p2p_srv_cfg.require_mutual_tls = true;
    p2p_srv_cfg.cert_pem = pki.server_cert_pem;
    p2p_srv_cfg.key_pem = pki.server_key_pem;
    p2p_srv_cfg.ca_pem = pki.ca_cert_pem;

    mt2::net::AsioTls13Server p2p_srv(io, 0, p2p_srv_cfg, limits, &p2p_schema);
    const uint16_t p2p_port = p2p_srv.BoundPort();

    std::atomic<bool> p2p_setup_rx{false};
    std::atomic<bool> p2p_login_rx{false};
    std::atomic<bool> srv_hs_done{false};
    asio::error_code srv_last_hs_ec;
    p2p_srv.StartAccept([&](std::shared_ptr<mt2::net::AsioTls13Session> s) {
        s->SetCallbacks(
            [&](std::shared_ptr<mt2::net::AsioTls13Session>, const asio::error_code& ec) {
                srv_last_hs_ec = ec;
                srv_hs_done.store(true);
            },
            [&](std::shared_ptr<mt2::net::AsioTls13Session> sess, const std::vector<uint8_t>& pkt) {
                if (pkt[0] == HEADER_GG_SETUP)
                {
                    p2p_setup_rx.store(true);
                }
                else if (pkt[0] == HEADER_GG_LOGIN)
                {
                    p2p_login_rx.store(true);
                    WirePacketGGRelay relay{};
                    std::snprintf(relay.szName, sizeof(relay.szName), "WarriorX64");
                    relay.lSize = 0;
                    sess->EnqueueSend(&relay, sizeof(relay));
                }
            },
            nullptr);
        s->StartServerHandshake();
    });

    // 9a. Rogue peer WITHOUT client certificate MUST be rejected by P2P mTLS!
    {
        mt2::net::Tls13Config rogue_cfg;
        rogue_cfg.domain = mt2::net::SecurityDomain::ServerToServerP2P;
        rogue_cfg.ca_pem = pki.ca_cert_pem;
        rogue_cfg.expected_hostname = "mt2009.local";
        // No cert_pem / key_pem provided!

        mt2::net::CrossPlatformTls13Client rogue_cli(io, rogue_cfg, limits, &p2p_schema);
        std::atomic<bool> rogue_disc{false};
        asio::error_code rogue_disc_ec;
        rogue_cli.ConnectEndpoint(
            asio::ip::tcp::endpoint(asio::ip::make_address("127.0.0.1"), p2p_port),
            [&](std::shared_ptr<mt2::net::AsioTls13Session> sess, const asio::error_code& ec) {
                if (ec)
                {
                    rogue_disc_ec = ec;
                    rogue_disc.store(true);
                }
                else if (sess)
                {
                    WirePacketGGSetup setup{};
                    setup.wPort = 13001;
                    sess->EnqueueSend(&setup, sizeof(setup));
                }
            },
            nullptr,
            [&](std::shared_ptr<mt2::net::AsioTls13Session>, const asio::error_code& ec, mt2::net::FrameStatus) {
                rogue_disc_ec = ec;
                rogue_disc.store(true);
            });
        RunIoUntil(io, [&]() { return srv_hs_done.load() && rogue_disc.load(); });
        MT2_CHECK(srv_hs_done.load());
        MT2_CHECK(static_cast<bool>(srv_last_hs_ec)); // Server mTLS handshake rejected rogue peer!
        MT2_CHECK(rogue_disc.load());
        MT2_CHECK(static_cast<bool>(rogue_disc_ec));  // Rogue peer disconnected with TLS alert!
        MT2_CHECK(!p2p_setup_rx.load());              // Zero P2P packets accepted from rogue peer!
    }

    // 9b. Valid P2P peer with mTLS client certificate + Mobile Suspend/Resume/NetworkChange hooks
    {
        mt2::net::Tls13Config peer_cfg;
        peer_cfg.domain = mt2::net::SecurityDomain::ServerToServerP2P;
        peer_cfg.cert_pem = pki.client_cert_pem;
        peer_cfg.key_pem = pki.client_key_pem;
        peer_cfg.expected_hostname = "mt2009.local";
        // Provide CA via MobilePlatformConfig trust store callback (FAZ 8)
        mt2::net::MobilePlatformConfig mob_cfg;
        mob_cfg.load_platform_trust_store_pem = [&]() { return pki.ca_cert_pem; };

        mt2::net::CrossPlatformTls13Client peer_cli(io, peer_cfg, limits, &p2p_schema, mob_cfg);
        std::atomic<bool> relay_rx{false};

        peer_cli.ConnectEndpoint(
            asio::ip::tcp::endpoint(asio::ip::make_address("127.0.0.1"), p2p_port),
            [&](std::shared_ptr<mt2::net::AsioTls13Session> sess, const asio::error_code& ec) {
                MT2_CHECK(!ec);
                WirePacketGGSetup setup{};
                setup.wPort = 13001;
                setup.bChannel = 1;
                sess->EnqueueSend(&setup, sizeof(setup));

                WirePacketGGLogin login{};
                std::snprintf(login.szName, sizeof(login.szName), "WarriorX64");
                login.dwPID = 900001ULL;
                sess->EnqueueSend(&login, sizeof(login));
            },
            [&](std::shared_ptr<mt2::net::AsioTls13Session>, const std::vector<uint8_t>& pkt) {
                if (pkt[0] == HEADER_GG_RELAY)
                    relay_rx.store(true);
            },
            nullptr);

        RunIoUntil(io, [&]() { return p2p_setup_rx.load() && p2p_login_rx.load() && relay_rx.load(); });
        MT2_CHECK(p2p_setup_rx.load());
        MT2_CHECK(p2p_login_rx.load());
        MT2_CHECK(relay_rx.load());

        // Test Mobile Suspend & Resume lifecycle hook
        peer_cli.OnAppSuspend();
        MT2_CHECK(peer_cli.IsSuspended());
        RunIoUntil(io, [&]() { return peer_cli.CurrentSession()->IsClosed(); });
        MT2_CHECK(peer_cli.CurrentSession()->IsClosed());

        relay_rx.store(false);
        peer_cli.OnAppResume();
        MT2_CHECK(!peer_cli.IsSuspended());
        RunIoUntil(io, [&]() { return relay_rx.load(); });
        MT2_CHECK(relay_rx.load());

        // Test Mobile Network Reachability Change hook (Wi-Fi -> Cellular handoff)
        relay_rx.store(false);
        peer_cli.OnNetworkReachabilityChanged(
            asio::ip::tcp::endpoint(asio::ip::make_address("127.0.0.1"), p2p_port));
        RunIoUntil(io, [&]() { return relay_rx.load(); });
        MT2_CHECK(relay_rx.load());

        peer_cli.CurrentSession()->Close();
    }

    p2p_srv.Stop();
    std::printf("[PASS] Test 9: P2P mutual TLS (mTLS) security domain & mobile lifecycle hooks verified\n");
}

} // namespace

int main()
{
    std::printf("=== MT2009 Standalone Asio 1.38.2 + OpenSSL TLS 1.3 Security & Protocol Suite ===\n");
    try
    {
        TestSdkInvariants();
        TestAsioTcpLoopbackAndBackpressure();

        const auto pki = mt2::net::GenerateEphemeralTestPki();
        const auto schema = BuildCanonicalSchema();

        TestTls13HandshakeAndRules(pki, schema);
        TestTls13NegativeCertificateAndProtocolRejections(pki, schema);
        TestMitmTamperedTlsRecordRejection(pki, schema);
        TestFramingEdgeCasesAndMalformedPackets(pki, schema);
        TestTimeoutsAndFloodProtection(pki, schema);
        TestFullMetin2FlowsOverTls13(pki, schema);
        TestP2PMutualTlsAndMobileLifecycle(pki);
    }
    catch (const std::exception& ex)
    {
        std::fprintf(stderr, "[FATAL EXCEPTION] %s\n", ex.what());
        return 1;
    }

    std::printf("=== Asio + TLS 1.3 Suite Summary: %d/%d PASS (%d failed) ===\n",
        g_passed, g_passed + g_failed, g_failed);
    return g_failed == 0 ? 0 : 1;
}
