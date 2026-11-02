#pragma once
// ==============================================================================
// Metin2 MT2009 — C++20 Standalone Asio 1.38.2 + OpenSSL TLS 1.3 Transport Layer
// Single-source cross-platform network transport for:
//   - Windows AMD64 (MSVC 18 PE32+ & MinGW-w64 UCRT64 GCC 16 PE32+)
//   - FreeBSD 15.1 AMD64 (GCC 14 ELF64)
//   - Future Android ARM64 & iOS ARM64 clients (platform-independent C++20 API)
// ==============================================================================

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma push_macro("min")
#pragma push_macro("max")
#undef min
#undef max
#endif

#ifndef ASIO_STANDALONE
#define ASIO_STANDALONE 1
#endif
#ifndef ASIO_NO_DEPRECATED
#define ASIO_NO_DEPRECATED 1
#endif

#include <asio.hpp>
#include <asio/ssl.hpp>

#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/rand.h>
#include <openssl/ssl.h>
#include <openssl/x509.h>
#include <openssl/x509v3.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace mt2::net
{

// ------------------------------------------------------------------------------
// 1. Security Domains & Configuration
// ------------------------------------------------------------------------------
enum class SecurityDomain : uint8_t
{
    ClientToAuth = 1,
    ClientToGame = 2,
    ServerToServerP2P = 3,
    GameToDbInternal = 4
};

inline constexpr const char* kDefaultTls13CipherSuites =
    "TLS_AES_256_GCM_SHA384:TLS_CHACHA20_POLY1305_SHA256:TLS_AES_128_GCM_SHA256";

inline constexpr const char* kDefaultTls13Groups = "X25519:P-256:P-384";

struct TransportLimits
{
    size_t max_packet_size = 65536;
    size_t max_recv_buffer_bytes = 262144;      // 256 KiB per connection
    size_t max_send_queue_bytes = 524288;       // 512 KiB per connection
    size_t max_total_connections = 4096;
    size_t max_connections_per_ip = 64;
    size_t max_handshakes_per_ip_window = 30;
    std::chrono::milliseconds handshake_rate_window{1000};
    std::chrono::milliseconds handshake_timeout{5000};
    std::chrono::milliseconds idle_timeout{60000};
    size_t read_chunk_size = 8192;
};

struct Tls13Config
{
    SecurityDomain domain = SecurityDomain::ClientToGame;
    std::string cert_pem;
    std::string key_pem;
    std::string ca_pem;
    std::string expected_hostname = "mt2009.local";
    std::string ciphersuites = kDefaultTls13CipherSuites;
    std::string groups = kDefaultTls13Groups;
    bool require_mutual_tls = false;   // Default true when domain == ServerToServerP2P
    bool disable_0rtt = true;          // Mandatory rule #4: 0-RTT disabled
    uint16_t min_tls_version = TLS1_3_VERSION;
    uint16_t max_tls_version = TLS1_3_VERSION;
    // Negative-test hooks (NEVER enabled in production):
    bool test_only_allow_verify_none = false;
};

// ------------------------------------------------------------------------------
// 2. Mobile Platform Abstraction (Android ARM64 / iOS ARM64 Readiness — FAZ 8)
// ------------------------------------------------------------------------------
enum class PlatformTarget : uint8_t
{
    WindowsX64 = 1,
    FreeBsdAmd64 = 2,
    AndroidArm64 = 3,
    IosArm64 = 4,
    Unknown = 255
};

constexpr PlatformTarget DetectCompilePlatform() noexcept
{
#if defined(__ANDROID__) && defined(__aarch64__)
    return PlatformTarget::AndroidArm64;
#elif defined(__APPLE__) && defined(__aarch64__)
    return PlatformTarget::IosArm64;
#elif defined(_WIN64)
    return PlatformTarget::WindowsX64;
#elif defined(__FreeBSD__) && defined(__x86_64__)
    return PlatformTarget::FreeBsdAmd64;
#else
    return PlatformTarget::Unknown;
#endif
}

struct MobilePlatformConfig
{
    bool prefer_ipv6_dual_stack = true;
    bool tcp_nodelay = true;
    bool tcp_keepalive = true;
    uint32_t keepalive_idle_sec = 30;
    std::chrono::milliseconds reconnect_backoff_initial{500};
    std::chrono::milliseconds reconnect_backoff_max{10000};
    std::function<std::string()> load_platform_trust_store_pem;
};

// ------------------------------------------------------------------------------
// 3. Ephemeral X.509 v3 PKI Generator for Automated Tests (Zero Git Private Keys)
// ------------------------------------------------------------------------------
struct EphemeralPkiBundle
{
    std::string ca_cert_pem;
    std::string ca_key_pem;
    std::string server_cert_pem;
    std::string server_key_pem;
    std::string client_cert_pem;
    std::string client_key_pem;
};

namespace detail
{

struct EvpPkeyDeleter
{
    void operator()(EVP_PKEY* p) const noexcept { if (p) EVP_PKEY_free(p); }
};
struct EvpPkeyCtxDeleter
{
    void operator()(EVP_PKEY_CTX* p) const noexcept { if (p) EVP_PKEY_CTX_free(p); }
};
struct X509Deleter
{
    void operator()(X509* p) const noexcept { if (p) X509_free(p); }
};
struct X509ExtDeleter
{
    void operator()(X509_EXTENSION* p) const noexcept { if (p) X509_EXTENSION_free(p); }
};
struct BioDeleter
{
    void operator()(BIO* p) const noexcept { if (p) BIO_free_all(p); }
};

inline std::unique_ptr<EVP_PKEY, EvpPkeyDeleter> GenerateEcP256Key()
{
    std::unique_ptr<EVP_PKEY_CTX, EvpPkeyCtxDeleter> pctx(
        EVP_PKEY_CTX_new_id(EVP_PKEY_EC, nullptr));
    if (!pctx || EVP_PKEY_keygen_init(pctx.get()) <= 0)
        throw std::runtime_error("EVP_PKEY_keygen_init failed");
    if (EVP_PKEY_CTX_set_ec_paramgen_curve_nid(pctx.get(), NID_X9_62_prime256v1) <= 0)
        throw std::runtime_error("EVP_PKEY_CTX_set_ec_paramgen_curve_nid failed");
    EVP_PKEY* raw_pkey = nullptr;
    if (EVP_PKEY_keygen(pctx.get(), &raw_pkey) <= 0 || !raw_pkey)
        throw std::runtime_error("EVP_PKEY_keygen failed");
    return std::unique_ptr<EVP_PKEY, EvpPkeyDeleter>(raw_pkey);
}

inline void AddX509Ext(X509* issuer, X509* subj, int nid, const char* value)
{
    X509V3_CTX ctx;
    X509V3_set_ctx_nodb(&ctx);
    X509V3_set_ctx(&ctx, issuer, subj, nullptr, nullptr, 0);
    std::unique_ptr<X509_EXTENSION, X509ExtDeleter> ext(
        X509V3_EXT_conf_nid(nullptr, &ctx, nid, const_cast<char*>(value)));
    if (!ext || X509_add_ext(subj, ext.get(), -1) != 1)
        throw std::runtime_error(std::string("X509V3_EXT_conf_nid failed for: ") + value);
}

inline std::pair<std::string, std::string> CertAndKeyToPem(X509* cert, EVP_PKEY* pkey)
{
    std::unique_ptr<BIO, BioDeleter> cert_bio(BIO_new(BIO_s_mem()));
    std::unique_ptr<BIO, BioDeleter> key_bio(BIO_new(BIO_s_mem()));
    if (!cert_bio || !key_bio)
        throw std::runtime_error("BIO_new failed");
    if (PEM_write_bio_X509(cert_bio.get(), cert) != 1)
        throw std::runtime_error("PEM_write_bio_X509 failed");
    if (PEM_write_bio_PrivateKey(key_bio.get(), pkey, nullptr, nullptr, 0, nullptr, nullptr) != 1)
        throw std::runtime_error("PEM_write_bio_PrivateKey failed");

    BUF_MEM* c_mem = nullptr;
    BUF_MEM* k_mem = nullptr;
    BIO_get_mem_ptr(cert_bio.get(), &c_mem);
    BIO_get_mem_ptr(key_bio.get(), &k_mem);
    return {
        std::string(c_mem->data, c_mem->length),
        std::string(k_mem->data, k_mem->length)
    };
}

} // namespace detail

inline EphemeralPkiBundle GenerateEphemeralTestPki(
    const std::string& server_cn = "mt2009.local",
    const std::string& san_ext = "DNS:mt2009.local,DNS:localhost,IP:127.0.0.1",
    long not_before_offset_sec = -3600,
    long not_after_offset_sec = 86400,
    uint64_t serial_base = 1000)
{
    EphemeralPkiBundle out;

    // 1. Root CA (ECDSA P-256)
    auto ca_key = detail::GenerateEcP256Key();
    std::unique_ptr<X509, detail::X509Deleter> ca_cert(X509_new());
    X509_set_version(ca_cert.get(), 2); // X.509 v3
    ASN1_INTEGER_set_uint64(X509_get_serialNumber(ca_cert.get()), serial_base);
    X509_gmtime_adj(X509_getm_notBefore(ca_cert.get()), -7200);
    X509_gmtime_adj(X509_getm_notAfter(ca_cert.get()), 172800);
    X509_set_pubkey(ca_cert.get(), ca_key.get());

    X509_NAME* ca_name = X509_get_subject_name(ca_cert.get());
    X509_NAME_add_entry_by_txt(ca_name, "O", MBSTRING_ASC,
        reinterpret_cast<const unsigned char*>("MT2009 Test Root CA"), -1, -1, 0);
    X509_NAME_add_entry_by_txt(ca_name, "CN", MBSTRING_ASC,
        reinterpret_cast<const unsigned char*>("MT2009 Ephemeral Root CA"), -1, -1, 0);
    X509_set_issuer_name(ca_cert.get(), ca_name);

    detail::AddX509Ext(ca_cert.get(), ca_cert.get(), NID_basic_constraints, "critical,CA:TRUE");
    detail::AddX509Ext(ca_cert.get(), ca_cert.get(), NID_key_usage, "critical,keyCertSign,cRLSign");
    detail::AddX509Ext(ca_cert.get(), ca_cert.get(), NID_subject_key_identifier, "hash");

    if (X509_sign(ca_cert.get(), ca_key.get(), EVP_sha256()) <= 0)
        throw std::runtime_error("X509_sign CA failed");
    std::tie(out.ca_cert_pem, out.ca_key_pem) = detail::CertAndKeyToPem(ca_cert.get(), ca_key.get());

    // 2. Server Leaf Certificate
    auto srv_key = detail::GenerateEcP256Key();
    std::unique_ptr<X509, detail::X509Deleter> srv_cert(X509_new());
    X509_set_version(srv_cert.get(), 2);
    ASN1_INTEGER_set_uint64(X509_get_serialNumber(srv_cert.get()), serial_base + 1);
    X509_gmtime_adj(X509_getm_notBefore(srv_cert.get()), not_before_offset_sec);
    X509_gmtime_adj(X509_getm_notAfter(srv_cert.get()), not_after_offset_sec);
    X509_set_pubkey(srv_cert.get(), srv_key.get());

    X509_NAME* srv_name = X509_get_subject_name(srv_cert.get());
    X509_NAME_add_entry_by_txt(srv_name, "O", MBSTRING_ASC,
        reinterpret_cast<const unsigned char*>("MT2009 Server"), -1, -1, 0);
    X509_NAME_add_entry_by_txt(srv_name, "CN", MBSTRING_ASC,
        reinterpret_cast<const unsigned char*>(server_cn.c_str()), -1, -1, 0);
    X509_set_issuer_name(srv_cert.get(), ca_name);

    detail::AddX509Ext(ca_cert.get(), srv_cert.get(), NID_basic_constraints, "critical,CA:FALSE");
    detail::AddX509Ext(ca_cert.get(), srv_cert.get(), NID_key_usage, "critical,digitalSignature,keyEncipherment");
    detail::AddX509Ext(ca_cert.get(), srv_cert.get(), NID_ext_key_usage, "serverAuth,clientAuth");
    if (!san_ext.empty())
        detail::AddX509Ext(ca_cert.get(), srv_cert.get(), NID_subject_alt_name, san_ext.c_str());

    if (X509_sign(srv_cert.get(), ca_key.get(), EVP_sha256()) <= 0)
        throw std::runtime_error("X509_sign Server failed");
    std::tie(out.server_cert_pem, out.server_key_pem) = detail::CertAndKeyToPem(srv_cert.get(), srv_key.get());

    // 3. Client / P2P Peer Leaf Certificate (for mTLS / P2P domain)
    auto cli_key = detail::GenerateEcP256Key();
    std::unique_ptr<X509, detail::X509Deleter> cli_cert(X509_new());
    X509_set_version(cli_cert.get(), 2);
    ASN1_INTEGER_set_uint64(X509_get_serialNumber(cli_cert.get()), serial_base + 2);
    X509_gmtime_adj(X509_getm_notBefore(cli_cert.get()), not_before_offset_sec);
    X509_gmtime_adj(X509_getm_notAfter(cli_cert.get()), not_after_offset_sec);
    X509_set_pubkey(cli_cert.get(), cli_key.get());

    X509_NAME* cli_name = X509_get_subject_name(cli_cert.get());
    X509_NAME_add_entry_by_txt(cli_name, "O", MBSTRING_ASC,
        reinterpret_cast<const unsigned char*>("MT2009 Peer"), -1, -1, 0);
    X509_NAME_add_entry_by_txt(cli_name, "CN", MBSTRING_ASC,
        reinterpret_cast<const unsigned char*>("p2p.mt2009.local"), -1, -1, 0);
    X509_set_issuer_name(cli_cert.get(), ca_name);

    detail::AddX509Ext(ca_cert.get(), cli_cert.get(), NID_basic_constraints, "critical,CA:FALSE");
    detail::AddX509Ext(ca_cert.get(), cli_cert.get(), NID_key_usage, "critical,digitalSignature");
    detail::AddX509Ext(ca_cert.get(), cli_cert.get(), NID_ext_key_usage, "clientAuth,serverAuth");
    detail::AddX509Ext(ca_cert.get(), cli_cert.get(), NID_subject_alt_name, "DNS:p2p.mt2009.local,IP:127.0.0.1");

    if (X509_sign(cli_cert.get(), ca_key.get(), EVP_sha256()) <= 0)
        throw std::runtime_error("X509_sign Client failed");
    std::tie(out.client_cert_pem, out.client_key_pem) = detail::CertAndKeyToPem(cli_cert.get(), cli_key.get());

    return out;
}

// ------------------------------------------------------------------------------
// 4. OpenSSL TLS 1.3 Context Factory & Live Certificate Reload
// ------------------------------------------------------------------------------
inline void ApplyCommonTls13ContextRules(asio::ssl::context& ctx, const Tls13Config& cfg)
{
    SSL_CTX* native = ctx.native_handle();

    // Enforce TLS 1.3 only (unless a negative downgrade test explicitly requests TLS 1.2)
    if (SSL_CTX_set_min_proto_version(native, cfg.min_tls_version) != 1 ||
        SSL_CTX_set_max_proto_version(native, cfg.max_tls_version) != 1)
    {
        throw std::runtime_error("Failed to set TLS protocol version bounds on SSL_CTX");
    }

    // Rule 4: Disable TLS 1.3 0-RTT early data, compression, and renegotiation
    uint64_t opts = SSL_OP_NO_SSLv2 | SSL_OP_NO_SSLv3 | SSL_OP_NO_TLSv1 |
                    SSL_OP_NO_TLSv1_1 | SSL_OP_NO_COMPRESSION | SSL_OP_NO_RENEGOTIATION;
    if (cfg.min_tls_version >= TLS1_3_VERSION)
        opts |= SSL_OP_NO_TLSv1_2;
    if (cfg.disable_0rtt)
    {
        opts |= SSL_OP_NO_ANTI_REPLAY;
        SSL_CTX_set_max_early_data(native, 0);
        SSL_CTX_set_recv_max_early_data(native, 0);
    }
    SSL_CTX_set_options(native, opts);

    // Rule 5: Strong TLS 1.3 AEAD cipher suites & ECDHE groups
    if (!cfg.ciphersuites.empty())
    {
        if (SSL_CTX_set_ciphersuites(native, cfg.ciphersuites.c_str()) != 1)
            throw std::runtime_error("SSL_CTX_set_ciphersuites failed");
    }
    if (!cfg.groups.empty())
    {
        if (SSL_CTX_set1_groups_list(native, cfg.groups.c_str()) != 1)
            throw std::runtime_error("SSL_CTX_set1_groups_list failed");
    }

    if (!cfg.ca_pem.empty())
    {
        ctx.add_certificate_authority(
            asio::buffer(cfg.ca_pem.data(), cfg.ca_pem.size()));
    }
    if (!cfg.cert_pem.empty())
    {
        ctx.use_certificate_chain(
            asio::buffer(cfg.cert_pem.data(), cfg.cert_pem.size()));
    }
    if (!cfg.key_pem.empty())
    {
        ctx.use_private_key(
            asio::buffer(cfg.key_pem.data(), cfg.key_pem.size()),
            asio::ssl::context::pem);
    }
}

inline std::shared_ptr<asio::ssl::context> CreateServerTls13Context(const Tls13Config& cfg)
{
    auto ctx = std::make_shared<asio::ssl::context>(asio::ssl::context::tls_server);
    ApplyCommonTls13ContextRules(*ctx, cfg);

    const bool need_client_cert =
        cfg.require_mutual_tls || (cfg.domain == SecurityDomain::ServerToServerP2P);
    if (need_client_cert)
    {
        ctx->set_verify_mode(
            asio::ssl::verify_peer | asio::ssl::verify_fail_if_no_peer_cert);
    }
    else
    {
        ctx->set_verify_mode(asio::ssl::verify_none);
    }
    return ctx;
}

inline std::shared_ptr<asio::ssl::context> CreateClientTls13Context(const Tls13Config& cfg)
{
    auto ctx = std::make_shared<asio::ssl::context>(asio::ssl::context::tls_client);
    ApplyCommonTls13ContextRules(*ctx, cfg);

    // Rule 1 & Rule 3: Production client connections MUST NEVER use verify_none
    if (cfg.test_only_allow_verify_none)
    {
        ctx->set_verify_mode(asio::ssl::verify_none);
    }
    else
    {
        ctx->set_verify_mode(
            asio::ssl::verify_peer | asio::ssl::verify_fail_if_no_peer_cert);
    }
    return ctx;
}

inline void ConfigureClientStreamHostnameVerification(
    asio::ssl::stream<asio::ip::tcp::socket>& stream,
    const std::string& expected_hostname)
{
    if (expected_hostname.empty())
        return;
    SSL* ssl = stream.native_handle();
    SSL_set_tlsext_host_name(ssl, expected_hostname.c_str());
    SSL_set_hostflags(ssl, X509_CHECK_FLAG_NO_PARTIAL_WILDCARDS);
    if (SSL_set1_host(ssl, expected_hostname.c_str()) != 1)
        throw std::runtime_error("SSL_set1_host failed");
}

// ------------------------------------------------------------------------------
// 5. Metin2 Wire Packet Framer (Stream-Safe, Record-Boundary Independent)
// ------------------------------------------------------------------------------
enum class PacketLengthKind : uint8_t
{
    Fixed = 0,
    Dynamic16 = 1,   // uint16_t size at byte offset 1 (standard Metin2 dynamic packet)
    Dynamic32 = 2    // uint32_t size at byte offset 1 (ENABLE_LARGE_DYNAMIC_PACKETS)
};

struct PacketHeaderSpec
{
    PacketLengthKind kind = PacketLengthKind::Fixed;
    uint32_t min_size = 0;
};

class Mt2PacketSchemaRegistry
{
public:
    void RegisterFixed(uint8_t header, uint32_t exact_size)
    {
        specs_[header] = PacketHeaderSpec{PacketLengthKind::Fixed, exact_size};
        valid_[header] = true;
    }

    void RegisterDynamic16(uint8_t header, uint32_t min_size = 3)
    {
        specs_[header] = PacketHeaderSpec{PacketLengthKind::Dynamic16, std::max<uint32_t>(3, min_size)};
        valid_[header] = true;
    }

    void RegisterDynamic32(uint8_t header, uint32_t min_size = 5)
    {
        specs_[header] = PacketHeaderSpec{PacketLengthKind::Dynamic32, std::max<uint32_t>(5, min_size)};
        valid_[header] = true;
    }

    bool Lookup(uint8_t header, PacketHeaderSpec& out) const noexcept
    {
        if (!valid_[header])
            return false;
        out = specs_[header];
        return true;
    }

private:
    PacketHeaderSpec specs_[256]{};
    bool valid_[256]{};
};

enum class FrameStatus : uint8_t
{
    NeedMoreData = 0,
    PacketReady = 1,
    InvalidHeader = 2,
    InvalidLength = 3,
    OversizedPacket = 4,
    BufferOverflow = 5
};

class Mt2PacketFramer
{
public:
    Mt2PacketFramer(const Mt2PacketSchemaRegistry* schema, size_t max_packet_size, size_t max_buffer_size)
        : schema_(schema), max_packet_size_(max_packet_size), max_buffer_size_(max_buffer_size)
    {
    }

    bool AppendBytes(const uint8_t* data, size_t len)
    {
        if (len == 0)
            return true;
        if (UnreadBytes() + len > max_buffer_size_)
            return false;
        CompactIfNeeded();
        buf_.insert(buf_.end(), data, data + len);
        return true;
    }

    size_t UnreadBytes() const noexcept
    {
        return buf_.size() - read_pos_;
    }

    FrameStatus TryExtractPacket(std::vector<uint8_t>& out_packet)
    {
        const size_t avail = UnreadBytes();
        if (avail == 0)
            return FrameStatus::NeedMoreData;

        const uint8_t* ptr = buf_.data() + read_pos_;
        const uint8_t header = ptr[0];

        PacketHeaderSpec spec{};
        if (!schema_ || !schema_->Lookup(header, spec))
            return FrameStatus::InvalidHeader;

        uint32_t packet_len = 0;
        if (spec.kind == PacketLengthKind::Fixed)
        {
            packet_len = spec.min_size;
            if (packet_len == 0)
                return FrameStatus::InvalidLength;
        }
        else if (spec.kind == PacketLengthKind::Dynamic16)
        {
            if (avail < 3)
                return FrameStatus::NeedMoreData;
            uint16_t dyn16 = 0;
            std::memcpy(&dyn16, ptr + 1, sizeof(uint16_t));
            packet_len = dyn16;
            if (packet_len < spec.min_size)
                return FrameStatus::InvalidLength;
        }
        else if (spec.kind == PacketLengthKind::Dynamic32)
        {
            if (avail < 5)
                return FrameStatus::NeedMoreData;
            uint32_t dyn32 = 0;
            std::memcpy(&dyn32, ptr + 1, sizeof(uint32_t));
            packet_len = dyn32;
            if (packet_len < spec.min_size)
                return FrameStatus::InvalidLength;
        }

        if (packet_len > max_packet_size_)
            return FrameStatus::OversizedPacket;

        if (avail < packet_len)
            return FrameStatus::NeedMoreData;

        out_packet.assign(ptr, ptr + packet_len);
        read_pos_ += packet_len;
        if (read_pos_ == buf_.size())
        {
            buf_.clear();
            read_pos_ = 0;
        }
        return FrameStatus::PacketReady;
    }

    bool PeekBytes(size_t count, void* dst) const noexcept
    {
        if (UnreadBytes() < count)
            return false;
        std::memcpy(dst, buf_.data() + read_pos_, count);
        return true;
    }

    bool ConsumeBytes(size_t count, void* dst = nullptr) noexcept
    {
        if (UnreadBytes() < count)
            return false;
        if (dst)
            std::memcpy(dst, buf_.data() + read_pos_, count);
        read_pos_ += count;
        if (read_pos_ == buf_.size())
        {
            buf_.clear();
            read_pos_ = 0;
        }
        return true;
    }

    void Clear() noexcept
    {
        buf_.clear();
        read_pos_ = 0;
    }

private:
    void CompactIfNeeded()
    {
        if (read_pos_ > 0 && (read_pos_ >= 16384 || read_pos_ * 2 >= buf_.size()))
        {
            buf_.erase(buf_.begin(), buf_.begin() + static_cast<std::ptrdiff_t>(read_pos_));
            read_pos_ = 0;
        }
    }

    const Mt2PacketSchemaRegistry* schema_ = nullptr;
    size_t max_packet_size_ = 65536;
    size_t max_buffer_size_ = 262144;
    std::vector<uint8_t> buf_;
    size_t read_pos_ = 0;
};

// ------------------------------------------------------------------------------
// 6. Connection Flood, Per-IP Rate Limit & Single-Use LoginByKey Guard (FAZ 5)
// ------------------------------------------------------------------------------
class ConnectionFloodGuard
{
public:
    explicit ConnectionFloodGuard(const TransportLimits& limits = {})
        : limits_(limits)
    {
    }

    bool TryAcquireConnection(const std::string& ip)
    {
        std::lock_guard<std::mutex> lock(mu_);
        if (total_active_ >= limits_.max_total_connections)
        {
            ++rejected_total_cap_;
            return false;
        }
        auto& st = ip_states_[ip];
        const auto now = std::chrono::steady_clock::now();
        while (!st.handshake_timestamps.empty() &&
               (now - st.handshake_timestamps.front()) > limits_.handshake_rate_window)
        {
            st.handshake_timestamps.pop_front();
        }
        if (st.active_connections >= limits_.max_connections_per_ip)
        {
            ++rejected_ip_cap_;
            return false;
        }
        if (st.handshake_timestamps.size() >= limits_.max_handshakes_per_ip_window)
        {
            ++rejected_rate_limit_;
            return false;
        }
        ++st.active_connections;
        st.handshake_timestamps.push_back(now);
        ++total_active_;
        return true;
    }

    void ReleaseConnection(const std::string& ip)
    {
        std::lock_guard<std::mutex> lock(mu_);
        auto it = ip_states_.find(ip);
        if (it != ip_states_.end() && it->second.active_connections > 0)
            --it->second.active_connections;
        if (total_active_ > 0)
            --total_active_;
    }

    // Anti-replay single-use LoginByKey store
    void IssueLoginKey(uint32_t login_key, const std::string& login, const uint32_t client_key[4])
    {
        std::lock_guard<std::mutex> lock(mu_);
        LoginKeyEntry entry;
        entry.login = login;
        std::memcpy(entry.client_key, client_key, sizeof(entry.client_key));
        entry.consumed = false;
        login_keys_[login_key] = std::move(entry);
    }

    bool ConsumeLoginKeySingleUse(uint32_t login_key, const std::string& login, const uint32_t client_key[4])
    {
        std::lock_guard<std::mutex> lock(mu_);
        auto it = login_keys_.find(login_key);
        if (it == login_keys_.end())
            return false;
        if (it->second.consumed)
        {
            ++rejected_replay_;
            return false;
        }
        if (it->second.login != login ||
            std::memcmp(it->second.client_key, client_key, sizeof(it->second.client_key)) != 0)
        {
            return false;
        }
        it->second.consumed = true;
        login_keys_.erase(it); // Strictly single-use
        return true;
    }

    size_t TotalActive() const
    {
        std::lock_guard<std::mutex> lock(mu_);
        return total_active_;
    }

    size_t RejectedTotalCap() const { std::lock_guard<std::mutex> l(mu_); return rejected_total_cap_; }
    size_t RejectedIpCap() const { std::lock_guard<std::mutex> l(mu_); return rejected_ip_cap_; }
    size_t RejectedRateLimit() const { std::lock_guard<std::mutex> l(mu_); return rejected_rate_limit_; }
    size_t RejectedReplay() const { std::lock_guard<std::mutex> l(mu_); return rejected_replay_; }

private:
    struct IpState
    {
        size_t active_connections = 0;
        std::deque<std::chrono::steady_clock::time_point> handshake_timestamps;
    };

    struct LoginKeyEntry
    {
        std::string login;
        uint32_t client_key[4]{};
        bool consumed = false;
    };

    TransportLimits limits_;
    mutable std::mutex mu_;
    size_t total_active_ = 0;
    size_t rejected_total_cap_ = 0;
    size_t rejected_ip_cap_ = 0;
    size_t rejected_rate_limit_ = 0;
    size_t rejected_replay_ = 0;
    std::unordered_map<std::string, IpState> ip_states_;
    std::unordered_map<uint32_t, LoginKeyEntry> login_keys_;
};

// ------------------------------------------------------------------------------
// 7. Standalone Asio Plain TCP Session (FAZ 2 — asio_tcp_loopback)
// ------------------------------------------------------------------------------
class AsioTcpSession : public std::enable_shared_from_this<AsioTcpSession>
{
public:
    using PacketCallback = std::function<void(std::shared_ptr<AsioTcpSession>, const uint8_t*, size_t)>;
    using DisconnectCallback = std::function<void(std::shared_ptr<AsioTcpSession>, const asio::error_code&)>;

    AsioTcpSession(
        asio::io_context& io,
        asio::ip::tcp::socket socket,
        const TransportLimits& limits = {})
        : io_(io),
          strand_(asio::make_strand(io.get_executor())),
          socket_(std::move(socket)),
          idle_timer_(io),
          limits_(limits),
          read_chunk_(limits.read_chunk_size)
    {
        asio::error_code ec;
        socket_.set_option(asio::ip::tcp::no_delay(true), ec);
    }

    void SetCallbacks(PacketCallback on_read, DisconnectCallback on_disconnect)
    {
        on_read_ = std::move(on_read);
        on_disconnect_ = std::move(on_disconnect);
    }

    void Start()
    {
        auto self = shared_from_this();
        asio::post(strand_, [this, self]() {
            if (closed_)
                return;
            ArmIdleTimer();
            DoRead();
        });
    }

    bool EnqueueSend(const void* data, size_t len)
    {
        if (len == 0 || closed_.load(std::memory_order_relaxed))
            return false;

        std::vector<uint8_t> copy(
             static_cast<const uint8_t*>(data),
            static_cast<const uint8_t*>(data) + len);

        size_t cur_queued = queued_send_bytes_.fetch_add(len, std::memory_order_relaxed) + len;
        if (cur_queued > limits_.max_send_queue_bytes)
        {
            queued_send_bytes_.fetch_sub(len, std::memory_order_relaxed);
            backpressure_drops_.fetch_add(1, std::memory_order_relaxed);
            Close(asio::error::no_buffer_space);
            return false;
        }

        auto self = shared_from_this();
        asio::post(strand_, [this, self, buf = std::move(copy)]() mutable {
            if (closed_)
                return;
            const bool write_in_progress = !send_queue_.empty();
            send_queue_.push_back(std::move(buf));
            if (!write_in_progress)
                DoWrite();
        });
        return true;
    }

    void Close(asio::error_code reason = {})
    {
        bool expected = false;
        if (!closed_.compare_exchange_strong(expected, true))
            return;

        auto self = shared_from_this();
        asio::post(strand_, [this, self, reason]() {
            idle_timer_.cancel();
            asio::error_code ec;
            socket_.cancel(ec);
            socket_.shutdown(asio::ip::tcp::socket::shutdown_both, ec);
            socket_.close(ec);
            send_queue_.clear();
            queued_send_bytes_.store(0, std::memory_order_relaxed);
            if (on_disconnect_)
                on_disconnect_(self, reason);
        });
    }

    bool IsClosed() const noexcept { return closed_.load(std::memory_order_relaxed); }
    size_t BackpressureDrops() const noexcept { return backpressure_drops_.load(std::memory_order_relaxed); }
    size_t TotalBytesRead() const noexcept { return total_bytes_read_.load(std::memory_order_relaxed); }
    size_t TotalBytesWritten() const noexcept { return total_bytes_written_.load(std::memory_order_relaxed); }

private:
    void ArmIdleTimer()
    {
        if (limits_.idle_timeout.count() <= 0)
            return;
        idle_timer_.expires_after(limits_.idle_timeout);
        auto self = shared_from_this();
        idle_timer_.async_wait(asio::bind_executor(strand_, [this, self](const asio::error_code& ec) {
            if (ec || closed_)
                return;
            Close(asio::error::timed_out);
        }));
    }

    void DoRead()
    {
        auto self = shared_from_this();
        socket_.async_read_some(
            asio::buffer(read_chunk_),
            asio::bind_executor(strand_, [this, self](const asio::error_code& ec, size_t n) {
                if (closed_)
                    return;
                if (ec)
                {
                    Close(ec);
                    return;
                }
                total_bytes_read_.fetch_add(n, std::memory_order_relaxed);
                ArmIdleTimer();
                if (on_read_)
                    on_read_(self, read_chunk_.data(), n);
                if (!closed_)
                    DoRead();
            }));
    }

    void DoWrite()
    {
        auto self = shared_from_this();
        asio::async_write(
            socket_,
            asio::buffer(send_queue_.front()),
            asio::bind_executor(strand_, [this, self](const asio::error_code& ec, size_t n) {
                if (closed_)
                    return;
                if (ec)
                {
                    Close(ec);
                    return;
                }
                total_bytes_written_.fetch_add(n, std::memory_order_relaxed);
                const size_t popped_sz = send_queue_.front().size();
                send_queue_.pop_front();
                queued_send_bytes_.fetch_sub(popped_sz, std::memory_order_relaxed);
                if (!send_queue_.empty())
                    DoWrite();
            }));
    }

    asio::io_context& io_;
    asio::strand<asio::io_context::executor_type> strand_;
    asio::ip::tcp::socket socket_;
    asio::steady_timer idle_timer_;
    TransportLimits limits_;
    std::vector<uint8_t> read_chunk_;
    std::deque<std::vector<uint8_t>> send_queue_;
    std::atomic<size_t> queued_send_bytes_{0};
    std::atomic<size_t> backpressure_drops_{0};
    std::atomic<size_t> total_bytes_read_{0};
    std::atomic<size_t> total_bytes_written_{0};
    std::atomic<bool> closed_{false};
    PacketCallback on_read_;
    DisconnectCallback on_disconnect_;
};

// ------------------------------------------------------------------------------
// 8. Standalone Asio + OpenSSL TLS 1.3 Session (FAZ 3, FAZ 4, FAZ 5)
// ------------------------------------------------------------------------------
class AsioTls13Session : public std::enable_shared_from_this<AsioTls13Session>
{
public:
    using StreamType = asio::ssl::stream<asio::ip::tcp::socket>;
    using HandshakeCallback = std::function<void(std::shared_ptr<AsioTls13Session>, const asio::error_code&)>;
    using PacketCallback = std::function<void(std::shared_ptr<AsioTls13Session>, const std::vector<uint8_t>&)>;
    using RawDataCallback = std::function<void(std::shared_ptr<AsioTls13Session>, const uint8_t*, size_t)>;
    using DisconnectCallback = std::function<void(std::shared_ptr<AsioTls13Session>, const asio::error_code&, FrameStatus)>;

    AsioTls13Session(
        asio::io_context& io,
        asio::ip::tcp::socket socket,
        std::shared_ptr<asio::ssl::context> ssl_ctx,
        const Tls13Config& tls_cfg,
        const TransportLimits& limits = {},
        const Mt2PacketSchemaRegistry* schema = nullptr)
        : io_(io),
          strand_(asio::make_strand(io.get_executor())),
          ssl_ctx_(std::move(ssl_ctx)),
          stream_(std::move(socket), *ssl_ctx_),
          handshake_timer_(io),
          idle_timer_(io),
          tls_cfg_(tls_cfg),
          limits_(limits),
          framer_(schema, limits.max_packet_size, limits.max_recv_buffer_bytes),
          read_chunk_(limits.read_chunk_size)
    {
        asio::error_code ec;
        stream_.lowest_layer().set_option(asio::ip::tcp::no_delay(true), ec);
        remote_ip_ = ExtractRemoteIp();
    }

    void SetCallbacks(
        HandshakeCallback on_handshake,
        PacketCallback on_packet,
        DisconnectCallback on_disconnect,
        RawDataCallback on_raw_data = {})
    {
        on_handshake_ = std::move(on_handshake);
        on_packet_ = std::move(on_packet);
        on_disconnect_ = std::move(on_disconnect);
        on_raw_data_ = std::move(on_raw_data);
    }

    void StartServerHandshake()
    {
        auto self = shared_from_this();
        asio::post(strand_, [this, self]() {
            if (closed_)
                return;
            ArmHandshakeTimer();
            stream_.async_handshake(
                asio::ssl::stream_base::server,
                asio::bind_executor(strand_, [this, self](const asio::error_code& ec) {
                    OnHandshakeComplete(ec);
                }));
        });
    }

    void StartClientHandshake()
    {
        auto self = shared_from_this();
        asio::post(strand_, [this, self]() {
            if (closed_)
                return;
            if (!tls_cfg_.test_only_allow_verify_none && !tls_cfg_.expected_hostname.empty())
            {
                ConfigureClientStreamHostnameVerification(stream_, tls_cfg_.expected_hostname);
            }
            ArmHandshakeTimer();
            stream_.async_handshake(
                asio::ssl::stream_base::client,
                asio::bind_executor(strand_, [this, self](const asio::error_code& ec) {
                    OnHandshakeComplete(ec);
                }));
        });
    }

    bool EnqueueSend(const void* data, size_t len)
    {
        if (len == 0 || closed_.load(std::memory_order_relaxed))
            return false;

        std::vector<uint8_t> copy(
            static_cast<const uint8_t*>(data),
            static_cast<const uint8_t*>(data) + len);

        size_t cur_queued = queued_send_bytes_.fetch_add(len, std::memory_order_relaxed) + len;
        if (cur_queued > limits_.max_send_queue_bytes)
        {
            queued_send_bytes_.fetch_sub(len, std::memory_order_relaxed);
            backpressure_drops_.fetch_add(1, std::memory_order_relaxed);
            Close(asio::error::no_buffer_space);
            return false;
        }

        auto self = shared_from_this();
        asio::post(strand_, [this, self, buf = std::move(copy)]() mutable {
            if (closed_)
                return;
            const bool write_in_progress = !send_queue_.empty();
            send_queue_.push_back(std::move(buf));
            if (handshake_done_ && !write_in_progress)
                DoWrite();
        });
        return true;
    }

    void Close(asio::error_code reason = {}, FrameStatus frame_err = FrameStatus::PacketReady)
    {
        bool expected = false;
        if (!closed_.compare_exchange_strong(expected, true))
            return;

        last_frame_error_.store(frame_err, std::memory_order_relaxed);
        auto self = shared_from_this();
        asio::post(strand_, [this, self, reason, frame_err]() {
            handshake_timer_.cancel();
            idle_timer_.cancel();
            asio::error_code ec;
            stream_.lowest_layer().cancel(ec);
            stream_.lowest_layer().shutdown(asio::ip::tcp::socket::shutdown_both, ec);
            stream_.lowest_layer().close(ec);
            send_queue_.clear();
            queued_send_bytes_.store(0, std::memory_order_relaxed);
            if (on_disconnect_)
                on_disconnect_(self, reason, frame_err);
        });
    }

    // Non-blocking poll-mode helpers for main-thread consumption (DESC / CNetworkStream)
    size_t PopInboundPackets(std::deque<std::vector<uint8_t>>& out)
    {
        std::lock_guard<std::mutex> lock(inbound_mu_);
        size_t count = inbound_packets_.size();
        while (!inbound_packets_.empty())
        {
            out.push_back(std::move(inbound_packets_.front()));
            inbound_packets_.pop_front();
        }
        return count;
    }

    size_t AvailableRawBytes() const
    {
        std::lock_guard<std::mutex> lock(inbound_mu_);
        return inbound_raw_.size() - inbound_raw_pos_;
    }

    bool PeekRawBytes(size_t len, void* dst) const
    {
        std::lock_guard<std::mutex> lock(inbound_mu_);
        if (inbound_raw_.size() - inbound_raw_pos_ < len)
            return false;
        std::memcpy(dst, inbound_raw_.data() + inbound_raw_pos_, len);
        return true;
    }

    bool ConsumeRawBytes(size_t len, void* dst = nullptr)
    {
        std::lock_guard<std::mutex> lock(inbound_mu_);
        if (inbound_raw_.size() - inbound_raw_pos_ < len)
            return false;
        if (dst)
            std::memcpy(dst, inbound_raw_.data() + inbound_raw_pos_, len);
        inbound_raw_pos_ += len;
        if (inbound_raw_pos_ == inbound_raw_.size())
        {
            inbound_raw_.clear();
            inbound_raw_pos_ = 0;
        }
        else if (inbound_raw_pos_ >= 16384 && inbound_raw_pos_ * 2 >= inbound_raw_.size())
        {
            inbound_raw_.erase(
                inbound_raw_.begin(),
                inbound_raw_.begin() + static_cast<std::ptrdiff_t>(inbound_raw_pos_));
            inbound_raw_pos_ = 0;
        }
        return true;
    }

    bool IsHandshakeComplete() const noexcept { return handshake_done_.load(std::memory_order_relaxed); }
    bool IsClosed() const noexcept { return closed_.load(std::memory_order_relaxed); }
    bool IsOpen() const noexcept { return !IsClosed(); }
    bool SendRaw(const void* data, size_t len) { return EnqueueSend(data, len); }
    size_t AvailableRecvBytes() const { return AvailableRawBytes(); }
    size_t DrainRecvBytes(void* dst, size_t max_len)
    {
        const size_t avail = AvailableRawBytes();
        const size_t to_read = std::min(avail, max_len);
        if (to_read == 0)
            return 0;
        ConsumeRawBytes(to_read, dst);
        return to_read;
    }
    std::string NegotiatedTlsVersion() const
    {
        std::lock_guard<std::mutex> lock(meta_mu_);
        return negotiated_version_;
    }
    std::string NegotiatedCipherSuite() const
    {
        std::lock_guard<std::mutex> lock(meta_mu_);
        return negotiated_cipher_;
    }
    bool EarlyDataAccepted() const noexcept { return early_data_accepted_.load(std::memory_order_relaxed); }
    FrameStatus LastFrameError() const noexcept { return last_frame_error_.load(std::memory_order_relaxed); }
    size_t BackpressureDrops() const noexcept { return backpressure_drops_.load(std::memory_order_relaxed); }
    size_t TotalBytesRead() const noexcept { return total_bytes_read_.load(std::memory_order_relaxed); }
    size_t TotalBytesWritten() const noexcept { return total_bytes_written_.load(std::memory_order_relaxed); }
    const std::string& RemoteIp() const noexcept { return remote_ip_; }

private:
    std::string ExtractRemoteIp()
    {
        asio::error_code ec;
        auto ep = stream_.lowest_layer().remote_endpoint(ec);
        if (ec)
            return "127.0.0.1";
        return ep.address().to_string();
    }

    void ArmHandshakeTimer()
    {
        if (limits_.handshake_timeout.count() <= 0)
            return;
        handshake_timer_.expires_after(limits_.handshake_timeout);
        auto self = shared_from_this();
        handshake_timer_.async_wait(asio::bind_executor(strand_, [this, self](const asio::error_code& ec) {
            if (ec || closed_ || handshake_done_)
                return;
            Close(asio::error::timed_out);
        }));
    }

    void ArmIdleTimer()
    {
        if (limits_.idle_timeout.count() <= 0)
            return;
        idle_timer_.expires_after(limits_.idle_timeout);
        auto self = shared_from_this();
        idle_timer_.async_wait(asio::bind_executor(strand_, [this, self](const asio::error_code& ec) {
            if (ec || closed_)
                return;
            Close(asio::error::timed_out);
        }));
    }

    void OnHandshakeComplete(const asio::error_code& ec)
    {
        handshake_timer_.cancel();
        if (closed_)
            return;

        if (ec)
        {
            // Rule 8: Failed TLS handshake NEVER falls back to plaintext TCP
            if (on_handshake_)
                on_handshake_(shared_from_this(), ec);
            Close(ec);
            return;
        }

        SSL* ssl = stream_.native_handle();
        const int proto_ver = SSL_version(ssl);
        if (proto_ver != TLS1_3_VERSION && tls_cfg_.min_tls_version == TLS1_3_VERSION)
        {
            auto ver_ec = asio::error::make_error_code(asio::error::no_protocol_option);
            if (on_handshake_)
                on_handshake_(shared_from_this(), ver_ec);
            Close(ver_ec);
            return;
        }

        {
            std::lock_guard<std::mutex> lock(meta_mu_);
            const char* v_str = SSL_get_version(ssl);
            const char* c_str = SSL_get_cipher_name(ssl);
            negotiated_version_ = v_str ? v_str : "";
            negotiated_cipher_ = c_str ? c_str : "";
        }
        early_data_accepted_.store(
            SSL_get_early_data_status(ssl) == SSL_EARLY_DATA_ACCEPTED,
            std::memory_order_relaxed);

        handshake_done_.store(true, std::memory_order_release);
        if (on_handshake_)
            on_handshake_(shared_from_this(), ec);

        if (!closed_)
        {
            ArmIdleTimer();
            if (!send_queue_.empty())
                DoWrite();
            DoRead();
        }
    }

    void DoRead()
    {
        auto self = shared_from_this();
        stream_.async_read_some(
            asio::buffer(read_chunk_),
            asio::bind_executor(strand_, [this, self](const asio::error_code& ec, size_t n) {
                if (closed_)
                    return;
                if (ec)
                {
                    Close(ec);
                    return;
                }
                total_bytes_read_.fetch_add(n, std::memory_order_relaxed);
                ArmIdleTimer();

                if (on_raw_data_)
                    on_raw_data_(self, read_chunk_.data(), n);

                // Always store into raw buffer if no schema is provided, or frame via Mt2PacketFramer
                if (!framer_.AppendBytes(read_chunk_.data(), n))
                {
                    Close(asio::error::no_buffer_space, FrameStatus::BufferOverflow);
                    return;
                }

                {
                    std::lock_guard<std::mutex> lock(inbound_mu_);
                    if (inbound_raw_.size() - inbound_raw_pos_ + n <= limits_.max_recv_buffer_bytes)
                    {
                        inbound_raw_.insert(inbound_raw_.end(), read_chunk_.data(), read_chunk_.data() + n);
                    }
                }

                if (on_packet_)
                {
                    std::vector<uint8_t> pkt;
                    while (true)
                    {
                        FrameStatus st = framer_.TryExtractPacket(pkt);
                        if (st == FrameStatus::NeedMoreData)
                            break;
                        if (st != FrameStatus::PacketReady)
                        {
                            Close(asio::error::invalid_argument, st);
                            return;
                        }
                        {
                            std::lock_guard<std::mutex> lock(inbound_mu_);
                            inbound_packets_.push_back(pkt);
                        }
                        on_packet_(self, pkt);
                        if (closed_)
                            return;
                    }
                }

                if (!closed_)
                    DoRead();
            }));
    }

    void DoWrite()
    {
        auto self = shared_from_this();
        asio::async_write(
            stream_,
            asio::buffer(send_queue_.front()),
            asio::bind_executor(strand_, [this, self](const asio::error_code& ec, size_t n) {
                if (closed_)
                    return;
                if (ec)
                {
                    Close(ec);
                    return;
                }
                total_bytes_written_.fetch_add(n, std::memory_order_relaxed);
                const size_t popped_sz = send_queue_.front().size();
                send_queue_.pop_front();
                queued_send_bytes_.fetch_sub(popped_sz, std::memory_order_relaxed);
                if (!send_queue_.empty())
                    DoWrite();
            }));
    }

    asio::io_context& io_;
    asio::strand<asio::io_context::executor_type> strand_;
    std::shared_ptr<asio::ssl::context> ssl_ctx_;
    StreamType stream_;
    asio::steady_timer handshake_timer_;
    asio::steady_timer idle_timer_;
    Tls13Config tls_cfg_;
    TransportLimits limits_;
    Mt2PacketFramer framer_;
    std::vector<uint8_t> read_chunk_;
    std::deque<std::vector<uint8_t>> send_queue_;
    std::atomic<size_t> queued_send_bytes_{0};
    std::atomic<size_t> backpressure_drops_{0};
    std::atomic<size_t> total_bytes_read_{0};
    std::atomic<size_t> total_bytes_written_{0};
    std::atomic<bool> handshake_done_{false};
    std::atomic<bool> closed_{false};
    std::atomic<bool> early_data_accepted_{false};
    std::atomic<FrameStatus> last_frame_error_{FrameStatus::PacketReady};
    std::string remote_ip_;

    mutable std::mutex meta_mu_;
    std::string negotiated_version_;
    std::string negotiated_cipher_;

    mutable std::mutex inbound_mu_;
    std::deque<std::vector<uint8_t>> inbound_packets_;
    std::vector<uint8_t> inbound_raw_;
    size_t inbound_raw_pos_ = 0;

    HandshakeCallback on_handshake_;
    PacketCallback on_packet_;
    DisconnectCallback on_disconnect_;
    RawDataCallback on_raw_data_;
};

// ------------------------------------------------------------------------------
// 9. TLS 1.3 Server Acceptor with Atomic Certificate Rotation & Flood Guard
// ------------------------------------------------------------------------------
class AsioTls13Server
{
public:
    using SessionHandler = std::function<void(std::shared_ptr<AsioTls13Session>)>;

    AsioTls13Server(
        asio::io_context& io,
        uint16_t port,
        const Tls13Config& tls_cfg,
        const TransportLimits& limits = {},
        const Mt2PacketSchemaRegistry* schema = nullptr)
        : io_(io),
          acceptor_(io, asio::ip::tcp::endpoint(asio::ip::make_address("127.0.0.1"), port)),
          tls_cfg_(tls_cfg),
          limits_(limits),
          schema_(schema),
          flood_guard_(limits),
          ssl_ctx_(CreateServerTls13Context(tls_cfg))
    {
    }

    uint16_t BoundPort() const
    {
        return acceptor_.local_endpoint().port();
    }

    // Rule 6: Atomic live certificate rotation/reload without restarting server
    void ReloadCertificates(const std::string& new_cert_pem, const std::string& new_key_pem, const std::string& new_ca_pem = "")
    {
        std::lock_guard<std::mutex> lock(ctx_mu_);
        tls_cfg_.cert_pem = new_cert_pem;
        tls_cfg_.key_pem = new_key_pem;
        if (!new_ca_pem.empty())
            tls_cfg_.ca_pem = new_ca_pem;
        ssl_ctx_ = CreateServerTls13Context(tls_cfg_);
    }

    void StartAccept(SessionHandler on_session)
    {
        on_session_ = std::move(on_session);
        DoAccept();
    }

    void Stop()
    {
        stopped_.store(true, std::memory_order_relaxed);
        asio::error_code ec;
        acceptor_.close(ec);
    }

    ConnectionFloodGuard& FloodGuard() noexcept { return flood_guard_; }
    const ConnectionFloodGuard& FloodGuard() const noexcept { return flood_guard_; }

private:
    void DoAccept()
    {
        if (stopped_.load(std::memory_order_relaxed))
            return;

        acceptor_.async_accept([this](const asio::error_code& ec, asio::ip::tcp::socket sock) {
            if (stopped_.load(std::memory_order_relaxed))
                return;
            if (!ec)
            {
                asio::error_code ep_ec;
                auto ep = sock.remote_endpoint(ep_ec);
                std::string ip = ep_ec ? "127.0.0.1" : ep.address().to_string();

                if (!flood_guard_.TryAcquireConnection(ip))
                {
                    asio::error_code close_ec;
                    sock.shutdown(asio::ip::tcp::socket::shutdown_both, close_ec);
                    sock.close(close_ec);
                }
                else
                {
                    std::shared_ptr<asio::ssl::context> cur_ctx;
                    Tls13Config cur_cfg;
                    {
                        std::lock_guard<std::mutex> lock(ctx_mu_);
                        cur_ctx = ssl_ctx_;
                        cur_cfg = tls_cfg_;
                    }
                    auto session = std::make_shared<AsioTls13Session>(
                        io_, std::move(sock), std::move(cur_ctx), cur_cfg, limits_, schema_);
                    if (on_session_)
                        on_session_(session);
                }
            }
            if (!stopped_.load(std::memory_order_relaxed))
                DoAccept();
        });
    }

    asio::io_context& io_;
    asio::ip::tcp::acceptor acceptor_;
    Tls13Config tls_cfg_;
    TransportLimits limits_;
    const Mt2PacketSchemaRegistry* schema_ = nullptr;
    ConnectionFloodGuard flood_guard_;

    std::mutex ctx_mu_;
    std::shared_ptr<asio::ssl::context> ssl_ctx_;
    SessionHandler on_session_;
    std::atomic<bool> stopped_{false};
};

// ------------------------------------------------------------------------------
// 10. Mobile & Cross-Platform Client Connector with Lifecycle Hooks (FAZ 8)
// ------------------------------------------------------------------------------
class CrossPlatformTls13Client
{
public:
    CrossPlatformTls13Client(
        asio::io_context& io,
        const Tls13Config& tls_cfg,
        const TransportLimits& limits = {},
        const Mt2PacketSchemaRegistry* schema = nullptr,
        const MobilePlatformConfig& mobile_cfg = {})
        : io_(io),
          resolver_(io),
          tls_cfg_(tls_cfg),
          limits_(limits),
          schema_(schema),
          mobile_cfg_(mobile_cfg)
    {
        if (tls_cfg_.ca_pem.empty() && mobile_cfg_.load_platform_trust_store_pem)
            tls_cfg_.ca_pem = mobile_cfg_.load_platform_trust_store_pem();
        ssl_ctx_ = CreateClientTls13Context(tls_cfg_);
    }

    void AsyncConnect(
        const std::string& host,
        uint16_t port,
        AsioTls13Session::HandshakeCallback on_handshake,
        AsioTls13Session::PacketCallback on_packet,
        AsioTls13Session::DisconnectCallback on_disconnect,
        AsioTls13Session::RawDataCallback on_raw_data = {})
    {
        last_host_ = host;
        last_port_ = port;
        on_handshake_ = std::move(on_handshake);
        on_packet_ = std::move(on_packet);
        on_disconnect_ = std::move(on_disconnect);
        on_raw_data_ = std::move(on_raw_data);

        resolver_.async_resolve(
            host,
            std::to_string(port),
            [this](const asio::error_code& ec, asio::ip::tcp::resolver::results_type results) {
                if (ec || results.empty())
                {
                    if (on_handshake_)
                        on_handshake_(nullptr, ec ? ec : asio::error::host_not_found);
                    return;
                }
                ConnectEndpoint(*results.begin(), on_handshake_, on_packet_, on_disconnect_, on_raw_data_);
            });
    }

    // Direct endpoint connect helper (fast deterministic IPv4/IPv6 connect)
    std::shared_ptr<AsioTls13Session> ConnectEndpoint(
        const asio::ip::tcp::endpoint& ep,
        AsioTls13Session::HandshakeCallback on_handshake,
        AsioTls13Session::PacketCallback on_packet,
        AsioTls13Session::DisconnectCallback on_disconnect,
        AsioTls13Session::RawDataCallback on_raw_data = {})
    {
        last_ep_ = ep;
        on_handshake_ = std::move(on_handshake);
        on_packet_ = std::move(on_packet);
        on_disconnect_ = std::move(on_disconnect);
        on_raw_data_ = std::move(on_raw_data);

        auto sock_ptr = std::make_shared<asio::ip::tcp::socket>(io_);
        sock_ptr->async_connect(ep, [this, sock_ptr](const asio::error_code& ec) {
            if (ec)
            {
                if (on_handshake_)
                    on_handshake_(nullptr, ec);
                return;
            }
            if (mobile_cfg_.tcp_keepalive)
            {
                asio::error_code ka_ec;
                sock_ptr->set_option(asio::socket_base::keep_alive(true), ka_ec);
            }
            session_ = std::make_shared<AsioTls13Session>(
                io_, std::move(*sock_ptr), ssl_ctx_, tls_cfg_, limits_, schema_);
            session_->SetCallbacks(on_handshake_, on_packet_, on_disconnect_, on_raw_data_);
            session_->StartClientHandshake();
        });
        return session_;
    }

    // Mobile Lifecycle Hooks (Android ARM64 / iOS ARM64)
    void OnAppSuspend()
    {
        suspended_.store(true, std::memory_order_relaxed);
        if (session_ && !session_->IsClosed())
            session_->Close(asio::error::operation_aborted);
    }

    std::shared_ptr<AsioTls13Session> OnAppResume()
    {
        suspended_.store(false, std::memory_order_relaxed);
        if (last_ep_.port() != 0)
        {
            return ConnectEndpoint(last_ep_, on_handshake_, on_packet_, on_disconnect_, on_raw_data_);
        }
        return nullptr;
    }

    std::shared_ptr<AsioTls13Session> OnNetworkReachabilityChanged(const asio::ip::tcp::endpoint& new_ep)
    {
        if (session_ && !session_->IsClosed())
            session_->Close(asio::error::network_reset);
        return ConnectEndpoint(new_ep, on_handshake_, on_packet_, on_disconnect_, on_raw_data_);
    }

    std::shared_ptr<AsioTls13Session> CurrentSession() const noexcept { return session_; }
    bool IsSuspended() const noexcept { return suspended_.load(std::memory_order_relaxed); }

private:
    asio::io_context& io_;
    asio::ip::tcp::resolver resolver_;
    Tls13Config tls_cfg_;
    TransportLimits limits_;
    const Mt2PacketSchemaRegistry* schema_ = nullptr;
    MobilePlatformConfig mobile_cfg_;
    std::shared_ptr<asio::ssl::context> ssl_ctx_;
    std::shared_ptr<AsioTls13Session> session_;
    std::string last_host_;
    uint16_t last_port_ = 0;
    asio::ip::tcp::endpoint last_ep_;
    std::atomic<bool> suspended_{false};

    AsioTls13Session::HandshakeCallback on_handshake_;
    AsioTls13Session::PacketCallback on_packet_;
    AsioTls13Session::DisconnectCallback on_disconnect_;
    AsioTls13Session::RawDataCallback on_raw_data_;
};

} // namespace mt2::net

#if defined(_WIN32)
#pragma pop_macro("max")
#pragma pop_macro("min")
#endif
