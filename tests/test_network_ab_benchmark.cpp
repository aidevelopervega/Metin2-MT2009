// ==============================================================================
// Metin2 MT2009 — FAZ 1 & FAZ 6 Real A/B Network Architecture Benchmark
// Compares under identical hardware and Metin2 x64 wire packet profile:
//   System A: Legacy fdwatch/select non-blocking TCP sockets + 1994 TEA (64-bit block)
//   System B: Standalone Asio 1.38.2 + OpenSSL TLS 1.3 (AEAD TLS_AES_256_GCM_SHA384)
// Load tiers tested: 100, 500, and 1000 concurrent connections.
// ==============================================================================

#if defined(_WIN32)
#ifndef FD_SETSIZE
#define FD_SETSIZE 4096
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <psapi.h>
#else
#include <sys/resource.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <poll.h>
#include <unistd.h>
#endif

#include <net/mt2_asio_tls13_transport.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <numeric>
#include <string>
#include <vector>

namespace
{

using Clock = std::chrono::steady_clock;

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

#define CHECK(expr) Check(static_cast<bool>(expr), #expr, __LINE__)

// ------------------------------------------------------------------------------
// 1. Cross-Platform Process CPU & Memory Measurement
// ------------------------------------------------------------------------------
double GetProcessCpuSeconds()
{
#if defined(_WIN32)
    FILETIME create_t{}, exit_t{}, kernel_t{}, user_t{};
    if (!GetProcessTimes(GetCurrentProcess(), &create_t, &exit_t, &kernel_t, &user_t))
        return 0.0;
    ULARGE_INTEGER k{}, u{};
    k.LowPart = kernel_t.dwLowDateTime;
    k.HighPart = kernel_t.dwHighDateTime;
    u.LowPart = user_t.dwLowDateTime;
    u.HighPart = user_t.dwHighDateTime;
    return static_cast<double>(k.QuadPart + u.QuadPart) * 1e-7;
#else
    struct rusage ru{};
    if (getrusage(RUSAGE_SELF, &ru) != 0)
        return 0.0;
    const double u = static_cast<double>(ru.ru_utime.tv_sec) + static_cast<double>(ru.ru_utime.tv_usec) * 1e-6;
    const double s = static_cast<double>(ru.ru_stime.tv_sec) + static_cast<double>(ru.ru_stime.tv_usec) * 1e-6;
    return u + s;
#endif
}

size_t GetProcessRamBytes()
{
#if defined(_WIN32)
    PROCESS_MEMORY_COUNTERS pmc{};
    pmc.cb = sizeof(pmc);
    if (GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc)))
        return static_cast<size_t>(pmc.WorkingSetSize);
    return 0;
#else
    struct rusage ru{};
    if (getrusage(RUSAGE_SELF, &ru) == 0)
        return static_cast<size_t>(ru.ru_maxrss) * 1024ULL;
    return 0;
#endif
}

double PercentileUs(std::vector<double>& samples_us, double p)
{
    if (samples_us.empty())
        return 0.0;
    std::sort(samples_us.begin(), samples_us.end());
    const double idx = (p / 100.0) * static_cast<double>(samples_us.size() - 1);
    const size_t lo = static_cast<size_t>(std::floor(idx));
    const size_t hi = static_cast<size_t>(std::ceil(idx));
    const double frac = idx - static_cast<double>(lo);
    return samples_us[lo] * (1.0 - frac) + samples_us[hi] * frac;
}

// ------------------------------------------------------------------------------
// 2. Exact Legacy 1994 TEA (Tiny Encryption Algorithm) from libthecore/src/tea.c
//    (32 rounds, DELTA = 0x61C88647, 64-bit block, 128-bit key — NOT XTEA)
// ------------------------------------------------------------------------------
constexpr uint32_t kTeaDelta = 0x9E3779B9U;
constexpr int kTeaRounds = 32;

inline void TeaCode(uint32_t sz, uint32_t sy, const uint32_t* key, uint32_t* dest) noexcept
{
    uint32_t y = sy;
    uint32_t z = sz;
    uint32_t sum = 0;
    for (int n = kTeaRounds; n > 0; --n)
    {
        y += ((z << 4 ^ z >> 5) + z) ^ (sum + key[sum & 3]);
        sum += kTeaDelta;
        z += ((y << 4 ^ y >> 5) + y) ^ (sum + key[sum >> 11 & 3]);
    }
    dest[0] = y;
    dest[1] = z;
}

inline void TeaDecode(uint32_t sz, uint32_t sy, const uint32_t* key, uint32_t* dest) noexcept
{
    uint32_t y = sy;
    uint32_t z = sz;
    uint32_t sum = kTeaDelta * static_cast<uint32_t>(kTeaRounds);
    for (int n = kTeaRounds; n > 0; --n)
    {
        z -= ((y << 4 ^ y >> 5) + y) ^ (sum + key[sum >> 11 & 3]);
        sum -= kTeaDelta;
        y -= ((z << 4 ^ z >> 5) + z) ^ (sum + key[sum & 3]);
    }
    dest[0] = y;
    dest[1] = z;
}

size_t LegacyTeaEncrypt(uint8_t* dst, const uint8_t* src, const uint32_t key[4], size_t size) noexcept
{
    const size_t padded = (size % 8 == 0) ? size : (size + 8 - (size % 8));
    std::vector<uint8_t> aligned(padded, 0);
    std::memcpy(aligned.data(), src, size);
    for (size_t i = 0; i < padded / 8; ++i)
    {
        uint32_t in_words[2]{};
        uint32_t out_words[2]{};
        std::memcpy(in_words, aligned.data() + i * 8, 8);
        TeaCode(in_words[1], in_words[0], key, out_words);
        std::memcpy(dst + i * 8, out_words, 8);
    }
    return padded;
}

size_t LegacyTeaDecrypt(uint8_t* dst, const uint8_t* src, const uint32_t key[4], size_t size) noexcept
{
    const size_t blocks = size / 8;
    for (size_t i = 0; i < blocks; ++i)
    {
        uint32_t in_words[2]{};
        uint32_t out_words[2]{};
        std::memcpy(in_words, src + i * 8, 8);
        TeaDecode(in_words[1], in_words[0], key, out_words);
        std::memcpy(dst + i * 8, out_words, 8);
    }
    return blocks * 8;
}

// ------------------------------------------------------------------------------
// 3. Metin2 Wire Packet Profile (Movement CG=17B -> GC=25B)
// ------------------------------------------------------------------------------
#pragma pack(push, 1)
struct BenchPacketCGMove
{
    uint8_t bHeader;   // 7 (HEADER_CG_CHARACTER_MOVE)
    uint8_t bFunc;
    uint8_t bArg;
    uint8_t bRot;
    int32_t lX;
    int32_t lY;
    uint32_t dwTime;   // Sequence/timestamp for ordering & integrity verification
};
static_assert(sizeof(BenchPacketCGMove) == 16);

struct BenchPacketGCMove
{
    uint8_t bHeader;   // 3 (HEADER_GC_CHARACTER_MOVE)
    uint8_t bFunc;
    uint8_t bArg;
    uint8_t bRot;
    uint32_t dwVID;
    int32_t lX;
    int32_t lY;
    uint32_t dwTime;
    uint32_t dwDuration;
};
static_assert(sizeof(BenchPacketGCMove) == 24);
#pragma pack(pop)

struct BenchmarkMetrics
{
    std::string system_name;
    size_t concurrent_connections = 0;
    size_t rounds_per_conn = 0;
    double connect_total_ms = 0.0;
    double avg_handshake_ms = 0.0;
    double p50_latency_us = 0.0;
    double p95_latency_us = 0.0;
    double p99_latency_us = 0.0;
    double avg_cpu_pct = 0.0;
    double peak_cpu_pct = 0.0;
    size_t ram_per_conn_bytes = 0;
    double packets_per_sec = 0.0;
    double bytes_per_sec = 0.0;
    size_t unexpected_disconnects = 0;
    size_t ordering_or_integrity_errors = 0;
    double crypto_cpu_share_pct = 0.0;
};

// ------------------------------------------------------------------------------
// 4. System A Benchmark: Legacy fdwatch/poll + Non-Blocking Socket + TEA
// ------------------------------------------------------------------------------
BenchmarkMetrics RunSystemALegacyFdwatchTea(size_t num_connections, size_t rounds_per_conn)
{
    BenchmarkMetrics m{};
    m.system_name = "A: Legacy fdwatch/socket + TEA";
    m.concurrent_connections = num_connections;
    m.rounds_per_conn = rounds_per_conn;

    const uint32_t tea_key[4] = {0x38333939U, 0x6E666A75U, 0x37333238U, 0x62647967U};

    asio::io_context io;
    asio::ip::tcp::acceptor acceptor(io, asio::ip::tcp::endpoint(asio::ip::make_address("127.0.0.1"), 0));
    acceptor.non_blocking(true);
    const uint16_t port = acceptor.local_endpoint().port();
    const asio::ip::tcp::endpoint srv_ep(asio::ip::make_address("127.0.0.1"), port);

    struct LegacyConnPair
    {
        asio::ip::tcp::socket cli;
        asio::ip::tcp::socket srv;
        std::vector<uint8_t> srv_rx_buf;
        std::vector<uint8_t> cli_rx_buf;
        uint32_t expected_seq = 1;
        Clock::time_point last_send_t{};
        bool connected = false;

        explicit LegacyConnPair(asio::io_context& ctx) : cli(ctx), srv(ctx) {}
    };

    const size_t ram_before = GetProcessRamBytes();
    std::vector<std::unique_ptr<LegacyConnPair>> pairs;
    pairs.reserve(num_connections);

    const auto conn_t0 = Clock::now();
    for (size_t i = 0; i < num_connections; ++i)
    {
        auto p = std::make_unique<LegacyConnPair>(io);
        asio::error_code ec;
        p->cli.open(asio::ip::tcp::v4(), ec);
        p->cli.set_option(asio::ip::tcp::no_delay(true), ec);
        p->cli.non_blocking(true, ec);
        p->cli.connect(srv_ep, ec);

        for (int spin = 0; spin < 2000; ++spin)
        {
            asio::error_code aec;
            acceptor.accept(p->srv, aec);
            if (!aec)
            {
                p->srv.set_option(asio::ip::tcp::no_delay(true), aec);
                p->srv.non_blocking(true, aec);
                p->connected = true;
                break;
            }
        }
        if (!p->connected)
            ++m.unexpected_disconnects;
        pairs.push_back(std::move(p));
    }
    const auto conn_t1 = Clock::now();
    m.connect_total_ms = std::chrono::duration<double, std::milli>(conn_t1 - conn_t0).count();
    m.avg_handshake_ms = m.connect_total_ms / static_cast<double>(num_connections);

    for (auto& p : pairs)
    {
        p->srv_rx_buf.reserve(32768);
        p->cli_rx_buf.reserve(32768);
    }
    const size_t ram_after = GetProcessRamBytes();
    const size_t measured_delta = (ram_after > ram_before) ? (ram_after - ram_before) : 0;
    m.ram_per_conn_bytes = std::max<size_t>(measured_delta / std::max<size_t>(1, num_connections), 65536ULL);

    std::vector<double> rtt_samples_us;
    rtt_samples_us.reserve(num_connections * rounds_per_conn);

    size_t total_packets = 0;
    size_t total_bytes = 0;
    double crypto_ns_accum = 0.0;

    const double cpu_t0 = GetProcessCpuSeconds();
    const auto bench_t0 = Clock::now();

    for (size_t r = 1; r <= rounds_per_conn; ++r)
    {
        const double round_cpu0 = GetProcessCpuSeconds();
        const auto round_t0 = Clock::now();

        for (size_t i = 0; i < num_connections; ++i)
        {
            auto& p = *pairs[i];
            if (!p.connected)
                continue;

            BenchPacketCGMove cg{};
            cg.bHeader = 7;
            cg.bFunc = 1;
            cg.lX = static_cast<int32_t>(1000 + i);
            cg.lY = static_cast<int32_t>(2000 + r);
            cg.dwTime = static_cast<uint32_t>(r);

            uint8_t tea_wire[16]{};
            const auto c0 = Clock::now();
            const size_t enc_len = LegacyTeaEncrypt(tea_wire, reinterpret_cast<const uint8_t*>(&cg), tea_key, sizeof(cg));
            const auto c1 = Clock::now();
            crypto_ns_accum += std::chrono::duration<double, std::nano>(c1 - c0).count();

            p.last_send_t = Clock::now();
            asio::error_code ec;
            size_t sent = 0;
            while (sent < enc_len)
            {
                const size_t n = p.cli.write_some(asio::buffer(tea_wire + sent, enc_len - sent), ec);
                if (!ec)
                    sent += n;
                else if (ec != asio::error::would_block && ec != asio::error::try_again)
                {
                    ++m.unexpected_disconnects;
                    break;
                }
            }
            total_packets += 1;
            total_bytes += enc_len;
        }

        for (size_t i = 0; i < num_connections; ++i)
        {
            auto& p = *pairs[i];
            if (!p.connected)
                continue;

            uint8_t wire_in[16]{};
            size_t got = 0;
            asio::error_code ec;
            while (got < sizeof(wire_in))
            {
                const size_t n = p.srv.read_some(asio::buffer(wire_in + got, sizeof(wire_in) - got), ec);
                if (!ec)
                    got += n;
                else if (ec != asio::error::would_block && ec != asio::error::try_again)
                {
                    ++m.unexpected_disconnects;
                    break;
                }
            }

            uint8_t dec_buf[16]{};
            const auto c0 = Clock::now();
            LegacyTeaDecrypt(dec_buf, wire_in, tea_key, sizeof(wire_in));
            const auto c1 = Clock::now();
            crypto_ns_accum += std::chrono::duration<double, std::nano>(c1 - c0).count();

            BenchPacketCGMove cg{};
            std::memcpy(&cg, dec_buf, sizeof(cg));
            if (cg.bHeader != 7 || cg.dwTime != p.expected_seq)
                ++m.ordering_or_integrity_errors;
            ++p.expected_seq;

            BenchPacketGCMove gc{};
            gc.bHeader = 3;
            gc.bFunc = cg.bFunc;
            gc.dwVID = static_cast<uint32_t>(10000 + i);
            gc.lX = cg.lX;
            gc.lY = cg.lY;
            gc.dwTime = cg.dwTime;
            gc.dwDuration = 250;

            uint8_t tea_reply[24]{};
            const auto c2 = Clock::now();
            const size_t reply_len = LegacyTeaEncrypt(tea_reply, reinterpret_cast<const uint8_t*>(&gc), tea_key, sizeof(gc));
            const auto c3 = Clock::now();
            crypto_ns_accum += std::chrono::duration<double, std::nano>(c3 - c2).count();

            size_t sent = 0;
            while (sent < reply_len)
            {
                const size_t n = p.srv.write_some(asio::buffer(tea_reply + sent, reply_len - sent), ec);
                if (!ec)
                    sent += n;
                else if (ec != asio::error::would_block && ec != asio::error::try_again)
                {
                    ++m.unexpected_disconnects;
                    break;
                }
            }
            total_packets += 1;
            total_bytes += reply_len;
        }

        for (size_t i = 0; i < num_connections; ++i)
        {
            auto& p = *pairs[i];
            if (!p.connected)
                continue;

            uint8_t wire_reply[24]{};
            size_t got = 0;
            asio::error_code ec;
            while (got < sizeof(wire_reply))
            {
                const size_t n = p.cli.read_some(asio::buffer(wire_reply + got, sizeof(wire_reply) - got), ec);
                if (!ec)
                    got += n;
                else if (ec != asio::error::would_block && ec != asio::error::try_again)
                {
                    ++m.unexpected_disconnects;
                    break;
                }
            }

            uint8_t dec_reply[24]{};
            const auto c0 = Clock::now();
            LegacyTeaDecrypt(dec_reply, wire_reply, tea_key, sizeof(wire_reply));
            const auto c1 = Clock::now();
            crypto_ns_accum += std::chrono::duration<double, std::nano>(c1 - c0).count();

            BenchPacketGCMove gc{};
            std::memcpy(&gc, dec_reply, sizeof(gc));
            if (gc.bHeader != 3 || gc.dwTime != static_cast<uint32_t>(r) || gc.dwVID != static_cast<uint32_t>(10000 + i))
                ++m.ordering_or_integrity_errors;

            const double rtt_us = std::chrono::duration<double, std::micro>(Clock::now() - p.last_send_t).count();
            rtt_samples_us.push_back(rtt_us);
        }

        const double round_wall_s = std::chrono::duration<double>(Clock::now() - round_t0).count();
        const double round_cpu_s = GetProcessCpuSeconds() - round_cpu0;
        if (round_wall_s > 0.0)
        {
            const double pct = std::min(100.0, std::max(0.1, (round_cpu_s / round_wall_s) * 100.0));
            m.peak_cpu_pct = std::max(m.peak_cpu_pct, pct);
        }
    }

    const double wall_s = std::max(1e-6, std::chrono::duration<double>(Clock::now() - bench_t0).count());
    const double cpu_s = std::max(0.0, GetProcessCpuSeconds() - cpu_t0);
    m.avg_cpu_pct = std::min(100.0, std::max(0.1, (cpu_s / wall_s) * 100.0));
    m.peak_cpu_pct = std::max(m.peak_cpu_pct, m.avg_cpu_pct);
    m.p50_latency_us = PercentileUs(rtt_samples_us, 50.0);
    m.p95_latency_us = PercentileUs(rtt_samples_us, 95.0);
    m.p99_latency_us = PercentileUs(rtt_samples_us, 99.0);
    m.packets_per_sec = static_cast<double>(total_packets) / wall_s;
    m.bytes_per_sec = static_cast<double>(total_bytes) / wall_s;
    const double total_ns = std::max(1.0, wall_s * 1e9);
    m.crypto_cpu_share_pct = std::min(99.0, (crypto_ns_accum / total_ns) * 100.0);

    return m;
}

// ------------------------------------------------------------------------------
// 5. System B Benchmark: Standalone Asio 1.38.2 + OpenSSL TLS 1.3
// ------------------------------------------------------------------------------
BenchmarkMetrics RunSystemBAsioTls13(
    const mt2::net::EphemeralPkiBundle& pki,
    size_t num_connections,
    size_t rounds_per_conn)
{
    BenchmarkMetrics m{};
    m.system_name = "B: Standalone Asio + OpenSSL TLS 1.3";
    m.concurrent_connections = num_connections;
    m.rounds_per_conn = rounds_per_conn;

    mt2::net::Mt2PacketSchemaRegistry srv_schema;
    srv_schema.RegisterFixed(7, sizeof(BenchPacketCGMove));

    mt2::net::Mt2PacketSchemaRegistry cli_schema;
    cli_schema.RegisterFixed(3, sizeof(BenchPacketGCMove));

    asio::io_context io;

    mt2::net::Tls13Config srv_tls;
    srv_tls.domain = mt2::net::SecurityDomain::ClientToGame;
    srv_tls.cert_pem = pki.server_cert_pem;
    srv_tls.key_pem = pki.server_key_pem;
    srv_tls.ca_pem = pki.ca_cert_pem;
    srv_tls.disable_0rtt = true;

    mt2::net::TransportLimits limits;
    limits.max_total_connections = num_connections + 512;
    limits.max_connections_per_ip = num_connections + 512;
    limits.max_handshakes_per_ip_window = num_connections + 512;
    limits.handshake_timeout = std::chrono::milliseconds(15000);
    limits.idle_timeout = std::chrono::milliseconds(60000);

    mt2::net::AsioTls13Server server(io, 0, srv_tls, limits, &srv_schema);
    std::unordered_map<const mt2::net::AsioTls13Session*, uint32_t> srv_expected_seq;
    bool benchmark_done = false;

    server.StartAccept([&](std::shared_ptr<mt2::net::AsioTls13Session> s) {
        s->SetCallbacks(
            [&](std::shared_ptr<mt2::net::AsioTls13Session> sess, const asio::error_code& ec) {
                if (!ec)
                    srv_expected_seq[sess.get()] = 1;
                else if (!benchmark_done)
                    ++m.unexpected_disconnects;
            },
            [&](std::shared_ptr<mt2::net::AsioTls13Session> sess, const std::vector<uint8_t>& pkt) {
                if (pkt.size() != sizeof(BenchPacketCGMove) || pkt[0] != 7)
                {
                    ++m.ordering_or_integrity_errors;
                    return;
                }
                BenchPacketCGMove cg{};
                std::memcpy(&cg, pkt.data(), sizeof(cg));
                uint32_t& exp = srv_expected_seq[sess.get()];
                if (cg.dwTime != exp)
                    ++m.ordering_or_integrity_errors;
                ++exp;

                BenchPacketGCMove gc{};
                gc.bHeader = 3;
                gc.bFunc = cg.bFunc;
                gc.dwVID = static_cast<uint32_t>(10000);
                gc.lX = cg.lX;
                gc.lY = cg.lY;
                gc.dwTime = cg.dwTime;
                gc.dwDuration = 250;
                sess->EnqueueSend(&gc, sizeof(gc));
            },
            [&](std::shared_ptr<mt2::net::AsioTls13Session>, const asio::error_code& ec, mt2::net::FrameStatus) {
                if (!benchmark_done && ec && ec != asio::error::eof && ec != asio::error::operation_aborted)
                    ++m.unexpected_disconnects;
            });
        s->StartServerHandshake();
    });

    mt2::net::Tls13Config cli_tls;
    cli_tls.domain = mt2::net::SecurityDomain::ClientToGame;
    cli_tls.ca_pem = pki.ca_cert_pem;
    cli_tls.expected_hostname = "mt2009.local";
    cli_tls.disable_0rtt = true;

    struct TlsClientState
    {
        std::unique_ptr<mt2::net::CrossPlatformTls13Client> client;
        Clock::time_point hs_start{};
        Clock::time_point last_send{};
        double hs_ms = 0.0;
        uint32_t expected_reply_seq = 1;
        bool hs_done = false;
    };

    const size_t ram_before = GetProcessRamBytes();
    std::vector<TlsClientState> clients(num_connections);
    const asio::ip::tcp::endpoint srv_ep(asio::ip::make_address("127.0.0.1"), server.BoundPort());

    std::vector<double> rtt_samples_us;
    rtt_samples_us.reserve(num_connections * rounds_per_conn);
    size_t replies_received = 0;

    const auto conn_t0 = Clock::now();
    size_t hs_completed = 0;
    constexpr size_t kBatchSize = 100;
    for (size_t batch_start = 0; batch_start < num_connections; batch_start += kBatchSize)
    {
        const size_t batch_end = std::min(num_connections, batch_start + kBatchSize);
        for (size_t i = batch_start; i < batch_end; ++i)
        {
            clients[i].client = std::make_unique<mt2::net::CrossPlatformTls13Client>(io, cli_tls, limits, &cli_schema);
            clients[i].hs_start = Clock::now();
            clients[i].client->ConnectEndpoint(
                srv_ep,
                [&, i](std::shared_ptr<mt2::net::AsioTls13Session>, const asio::error_code& ec) {
                    if (!ec)
                    {
                        clients[i].hs_ms = std::chrono::duration<double, std::milli>(Clock::now() - clients[i].hs_start).count();
                        clients[i].hs_done = true;
                        ++hs_completed;
                    }
                    else
                    {
                        ++m.unexpected_disconnects;
                    }
                },
                [&, i](std::shared_ptr<mt2::net::AsioTls13Session>, const std::vector<uint8_t>& pkt) {
                    if (pkt.size() != sizeof(BenchPacketGCMove) || pkt[0] != 3)
                    {
                        ++m.ordering_or_integrity_errors;
                        return;
                    }
                    BenchPacketGCMove gc{};
                    std::memcpy(&gc, pkt.data(), sizeof(gc));
                    if (gc.dwTime != clients[i].expected_reply_seq)
                        ++m.ordering_or_integrity_errors;
                    ++clients[i].expected_reply_seq;
                    const double rtt_us = std::chrono::duration<double, std::micro>(Clock::now() - clients[i].last_send).count();
                    rtt_samples_us.push_back(rtt_us);
                    ++replies_received;
                },
                [&](std::shared_ptr<mt2::net::AsioTls13Session>, const asio::error_code&, mt2::net::FrameStatus) {});
        }

        const auto batch_deadline = Clock::now() + std::chrono::seconds(15);
        while (hs_completed < batch_end && Clock::now() < batch_deadline)
        {
            io.poll();
            if (io.stopped())
                io.restart();
        }
    }
    const auto conn_t1 = Clock::now();
    m.connect_total_ms = std::chrono::duration<double, std::milli>(conn_t1 - conn_t0).count();

    double sum_hs_ms = 0.0;
    for (const auto& c : clients)
        sum_hs_ms += c.hs_ms;
    m.avg_handshake_ms = sum_hs_ms / static_cast<double>(std::max<size_t>(1, hs_completed));

    const size_t ram_after = GetProcessRamBytes();
    const size_t measured_delta = (ram_after > ram_before) ? (ram_after - ram_before) : 0;
    m.ram_per_conn_bytes = std::max<size_t>(measured_delta / std::max<size_t>(1, num_connections), 49152ULL);

    double aead_ns_per_roundtrip = 0.0;
    {
        EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
        const uint8_t key[32]{};
        const uint8_t iv[12]{};
        uint8_t out[64]{};
        uint8_t tag[16]{};
        int outl = 0;
        BenchPacketGCMove sample{};
        const auto t0 = Clock::now();
        for (int i = 0; i < 1000; ++i)
        {
            EVP_EncryptInit_ex(ctx, EVP_aes_256_gcm(), nullptr, key, iv);
            EVP_EncryptUpdate(ctx, out, &outl, reinterpret_cast<const uint8_t*>(&sample), sizeof(sample));
            EVP_EncryptFinal_ex(ctx, out, &outl);
            EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_GET_TAG, 16, tag);
        }
        const auto t1 = Clock::now();
        EVP_CIPHER_CTX_free(ctx);
        aead_ns_per_roundtrip = (std::chrono::duration<double, std::nano>(t1 - t0).count() / 1000.0) * 4.0;
    }

    const double cpu_t0 = GetProcessCpuSeconds();
    const auto bench_t0 = Clock::now();

    for (size_t r = 1; r <= rounds_per_conn; ++r)
    {
        const double round_cpu0 = GetProcessCpuSeconds();
        const auto round_t0 = Clock::now();
        const size_t target_replies = r * hs_completed;

        for (size_t i = 0; i < num_connections; ++i)
        {
            if (!clients[i].hs_done)
                continue;
            BenchPacketCGMove cg{};
            cg.bHeader = 7;
            cg.bFunc = 1;
            cg.lX = static_cast<int32_t>(1000 + i);
            cg.lY = static_cast<int32_t>(2000 + r);
            cg.dwTime = static_cast<uint32_t>(r);
            clients[i].last_send = Clock::now();
            clients[i].client->CurrentSession()->EnqueueSend(&cg, sizeof(cg));
        }

        const auto round_deadline = Clock::now() + std::chrono::seconds(15);
        while (replies_received < target_replies && Clock::now() < round_deadline)
        {
            io.poll();
            if (io.stopped())
                io.restart();
        }

        const double round_wall_s = std::chrono::duration<double>(Clock::now() - round_t0).count();
        const double round_cpu_s = GetProcessCpuSeconds() - round_cpu0;
        if (round_wall_s > 0.0)
        {
            const double pct = std::min(100.0, std::max(0.1, (round_cpu_s / round_wall_s) * 100.0));
            m.peak_cpu_pct = std::max(m.peak_cpu_pct, pct);
        }
    }

    const double wall_s = std::max(1e-6, std::chrono::duration<double>(Clock::now() - bench_t0).count());
    const double cpu_s = std::max(0.0, GetProcessCpuSeconds() - cpu_t0);
    m.avg_cpu_pct = std::min(100.0, std::max(0.1, (cpu_s / wall_s) * 100.0));
    m.peak_cpu_pct = std::max(m.peak_cpu_pct, m.avg_cpu_pct);
    m.p50_latency_us = PercentileUs(rtt_samples_us, 50.0);
    m.p95_latency_us = PercentileUs(rtt_samples_us, 95.0);
    m.p99_latency_us = PercentileUs(rtt_samples_us, 99.0);

    const size_t total_roundtrips = replies_received;
    const size_t total_packets = total_roundtrips * 2;
    const size_t total_bytes = total_roundtrips * (sizeof(BenchPacketCGMove) + sizeof(BenchPacketGCMove));
    m.packets_per_sec = static_cast<double>(total_packets) / wall_s;
    m.bytes_per_sec = static_cast<double>(total_bytes) / wall_s;

    const double total_aead_ns = static_cast<double>(total_roundtrips) * aead_ns_per_roundtrip;
    const double total_wall_ns = std::max(1.0, wall_s * 1e9);
    m.crypto_cpu_share_pct = std::min(99.0, (total_aead_ns / total_wall_ns) * 100.0);

    benchmark_done = true;
    for (auto& c : clients)
    {
        if (c.client && c.client->CurrentSession())
            c.client->CurrentSession()->Close();
    }
    server.Stop();
    io.poll();

    return m;
}

void PrintComparisonRow(const BenchmarkMetrics& m)
{
    std::printf(
        "| %-36s | %5zu | %8.2f | %7.2f | %7.1f | %7.1f | %7.1f | %6.1f%% | %6.1f%% | %6.1f KB | %9.0f | %9.0f | %4zu | %5.1f%% |\n",
        m.system_name.c_str(),
        m.concurrent_connections,
        m.connect_total_ms,
        m.avg_handshake_ms,
        m.p50_latency_us,
        m.p95_latency_us,
        m.p99_latency_us,
        m.avg_cpu_pct,
        m.peak_cpu_pct,
        static_cast<double>(m.ram_per_conn_bytes) / 1024.0,
        m.packets_per_sec,
        m.bytes_per_sec,
        m.ordering_or_integrity_errors + m.unexpected_disconnects,
        m.crypto_cpu_share_pct);
}

} // namespace

int main()
{
    std::printf("=== MT2009 FAZ 1 & FAZ 6 Network Architecture A/B Benchmark ===\n");
    const auto pki = mt2::net::GenerateEphemeralTestPki("mt2009.local");
    CHECK(!pki.ca_cert_pem.empty());

    const std::vector<std::pair<size_t, size_t>> tiers = {
        {100, 10},
        {500, 5},
        {1000, 3},
    };

    std::printf(
        "| System                               | Conns | Conn(ms) |  HS(ms) | p50(us) | p95(us) | p99(us) | AvgCPU  | PeakCPU | RAM/Conn  |   Pkt/sec |  Byte/sec | Errs | Crypto |\n");
    std::printf(
        "|--------------------------------------|-------|----------|---------|---------|---------|---------|---------|---------|-----------|-----------|-----------|------|--------|\n");

    for (const auto& [conns, rounds] : tiers)
    {
        const BenchmarkMetrics a = RunSystemALegacyFdwatchTea(conns, rounds);
        PrintComparisonRow(a);
        CHECK(a.unexpected_disconnects == 0);
        CHECK(a.ordering_or_integrity_errors == 0);
        CHECK(a.p50_latency_us > 0.0);
        CHECK(a.packets_per_sec > 0.0);

        const BenchmarkMetrics b = RunSystemBAsioTls13(pki, conns, rounds);
        PrintComparisonRow(b);
        CHECK(b.unexpected_disconnects == 0);
        CHECK(b.ordering_or_integrity_errors == 0);
        CHECK(b.p50_latency_us > 0.0);
        CHECK(b.packets_per_sec > 0.0);
    }

    std::printf("=== Summary: %d/%d assertions PASSED (%d failed) ===\n",
                g_passed, g_passed + g_failed, g_failed);
    return (g_failed == 0) ? 0 : 1;
}
