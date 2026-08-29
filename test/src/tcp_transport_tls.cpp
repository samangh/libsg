#include "catch2/catch_test_macros.hpp"
#include "catch2/matchers/catch_matchers.hpp"
#include "catch2/matchers/catch_matchers_string.hpp"

#include "helpers.h"

#include <atomic>
#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <optional>
#include <sg/net/tcp_client.h>
#include <sg/net/tcp_server.h>
#include <sg/net/tls.h>
#include <sg/net/tcp_transport_tls.h>
#include <string>
#include <thread>

using namespace sg::net;

static end_point ep("127.0.0.1", 4466);

/* A self-signed certificate for "localhost", valid until 2126. Generated with:
 *
 *   openssl req -x509 -newkey rsa:2048 -keyout key.pem -out cert.pem -days 36500 -nodes \
 *       -subj "/CN=localhost" -addext "subjectAltName=DNS:localhost,IP:127.0.0.1"
 *
 * It is embedded rather than shipped as a file so the tests do not depend on a working directory,
 * and it is only ever trusted explicitly, by the test that wants it. */
static constexpr auto test_cert_pem = R"(-----BEGIN CERTIFICATE-----
MIIDJzCCAg+gAwIBAgIUOjAjon2vZ9ariM4cwXBEtPEg9CMwDQYJKoZIhvcNAQEL
BQAwFDESMBAGA1UEAwwJbG9jYWxob3N0MCAXDTI2MDgyMjA5MDQzMloYDzIxMjYw
NzI5MDkwNDMyWjAUMRIwEAYDVQQDDAlsb2NhbGhvc3QwggEiMA0GCSqGSIb3DQEB
AQUAA4IBDwAwggEKAoIBAQCin5qBWSn8GhHWel2JnFD6ngVCPmh60HOyygHCCglV
FQ9wBE5X2G7bDWgQ79NFMqy0AgisJcnQESf+1Cx1gAwApYnOdKc4BzGBuL1YsUIO
dnvnAOpCCL5ets/HqIVSQxwrz2qDnEUY8UG3KMV6wv1G7Gwtott2FRjO6rY27ctX
5bwrLIZChSED49krGlmPfxNhJJ1S48ID/HC84jxw4N2Kz4+kTM9uAbmX6w7rnlhV
5FgftqgOqb5AOR+XDNn6NhR0ttsAxLvCV+t9OvQ2cOAQMknAT3oVkJ6uEzh/KEtp
JmNRYWDRb+/e+jaLMOvtPViUA9xMgEmMM97DvpqR/4u5AgMBAAGjbzBtMB0GA1Ud
DgQWBBRNhvNTvH4UGcJHzwqBaZ2kMdQOJDAfBgNVHSMEGDAWgBRNhvNTvH4UGcJH
zwqBaZ2kMdQOJDAPBgNVHRMBAf8EBTADAQH/MBoGA1UdEQQTMBGCCWxvY2FsaG9z
dIcEfwAAATANBgkqhkiG9w0BAQsFAAOCAQEAf2NRsxkA58U/Ei0/AqMGkpijNCAA
eF9I2PVTRqlm28VmyHcbu473wfUOWOG1A8qVLYh4SgqS4UV/ftR2vey/GijSF7Of
nKGTlciEkezv4eY4S6N9uA6DN1rwACua/dj6Ss/MckKjsJO0pepEyt0u+pmyWUEc
/50ofyoZfrehB6HiGS9BCrhraEAVD9K66i+fgBO89k/nqD/J47knawMVX/t1Hq5O
m585st1ZplUQm7l1Gv1/jV7qS40AWK/vffanTU+sr3buInwOhNSwVmdhYyiSSiK9
vokjnIhoBT2OwgKAYwUzabMVWe7z+VrDzCKpOeffb791w6iVzuzA3z37Nw==
-----END CERTIFICATE-----
)";

static constexpr auto test_key_pem = R"(-----BEGIN PRIVATE KEY-----
MIIEvQIBADANBgkqhkiG9w0BAQEFAASCBKcwggSjAgEAAoIBAQCin5qBWSn8GhHW
el2JnFD6ngVCPmh60HOyygHCCglVFQ9wBE5X2G7bDWgQ79NFMqy0AgisJcnQESf+
1Cx1gAwApYnOdKc4BzGBuL1YsUIOdnvnAOpCCL5ets/HqIVSQxwrz2qDnEUY8UG3
KMV6wv1G7Gwtott2FRjO6rY27ctX5bwrLIZChSED49krGlmPfxNhJJ1S48ID/HC8
4jxw4N2Kz4+kTM9uAbmX6w7rnlhV5FgftqgOqb5AOR+XDNn6NhR0ttsAxLvCV+t9
OvQ2cOAQMknAT3oVkJ6uEzh/KEtpJmNRYWDRb+/e+jaLMOvtPViUA9xMgEmMM97D
vpqR/4u5AgMBAAECggEAEramC2tswudnFRS4rcywnt5PQZu33yYcoj7Pq/oGr+VA
BQZh04cC3q8wwf4vBB/8MqynPdQNYKWQwii9Qvosp2hlLr5Kvc5KGqDdHRcswEhP
Irp+uoGSEokb6OBSEzDIzZvNKH7zjpa4lrCY5PMKPT6Yhjne4jw0Qh9NP43mn+MP
S0ykzi07aiIwobCOk91pZ28bTJHs4mSWXx2hV7af8tUc+815GAKRV6vAZMHDa8hI
mWN46LO3O+u+nuiov977R2VyMR03UEaE7J8aHOisQJvPZWCEZ6S6X27+ZaCSHMuf
lPSvQyTSBHWGT2L0Gjr0sbBjwUHEkHjJqQFAKSNMfwKBgQDbMj5M/+Epvz+m1Ie3
PdUNWNcZz4Qkfp5C4UW+llkRaEc6rwqzsbs34e2hSn47wjCUC4RxSsyrbVbRz4LP
jg8pgVThbnsO21U6Azlf7mzHYJQLsKLHq9Wqi8yZPsQ/zI8B+uGMXWMBsxYKtsh/
ldEhUUHwY6vSC9ucswfYxdqqtwKBgQC97a4DH5LSACLFOXVHcxV4Cyu646b8voh3
XCVYJT7ggO7Rz/zP5trEx3OkKZ1usIgvFhPj91IaUjKEAI7juTwCg1aD6g6YgF1w
lcF3FnrVEvDH36UIW4lb1PgyXgJkmrdgxmzy5CtxH6tjYnGjyQT0ymJIL3PqNkfb
n/Mw/V3NDwKBgFTwn1vPPD2lMtE/Qmgruce2SYRi+d36gvF/wjscn98y/YcsFYWU
tevtzNvXthsKITD9VZFhXvZ/JEmhlBtB/XYj+/Rvj5guPlSAs1cNWXHZZwUwqaOe
Xun7yokH3ZyrdC9cPNLbzWX3M+9q7T8jmyrCBj9VIFwA5FVQuxWxdg75AoGAOF6h
peWqEs/dggGysDkU5yMRoI9OkXcPU7Wyk73CMqmxt+3uD9pplbvIs7FyO1cDpE3+
K0vNP2ij+4+a2TWx/OD0PYSrPlHi6bZYzDqMzE6pzfivp5JbazkDTRefyDIvOPbL
gS99QT7wBDhEmCLiaLDkiJ4k3h+sJiAL8r9QJrsCgYEAsl+rkp/V4PS576fbz8n3
ll2e0I5hHKIQbLE24Z+MreU5+WKkFRtqLx4LWSiipZFtE3p8esxUpJwMI5OM+mv9
t24IsiL3YB3nQgg0Xlgw5CmBhcz9GKT6mrMYB83uky9lI0TOIpo9zIZWiTQNo5I2
g+354vHHzqRJq5fypBd/m38=
-----END PRIVATE KEY-----
)";

/* Writes the embedded certificate and key out for the duration of a test, since
 * ssl::context only loads them from files. */
class cert_files {
  public:
    cert_files() {
        m_dir = std::filesystem::temp_directory_path() /
                ("sg_tls_test_" + std::to_string(::getpid()) + "_" +
                 std::to_string(m_counter.fetch_add(1)));
        std::filesystem::create_directories(m_dir);

        write(cert(), test_cert_pem);
        write(key(), test_key_pem);
    }

    ~cert_files() {
        std::error_code ec;
        std::filesystem::remove_all(m_dir, ec);
    }

    cert_files(const cert_files&) = delete;
    cert_files& operator=(const cert_files&) = delete;

    [[nodiscard]] std::string cert() const { return (m_dir / "cert.pem").string(); }
    [[nodiscard]] std::string key() const { return (m_dir / "key.pem").string(); }

    /* A server presenting our self-signed certificate. */
    [[nodiscard]] transport_factory server_factory() const {
        return tls_transport_factory(tls_server_config(cert(), key()));
    }

    /* A client that trusts our self-signed certificate, and nothing else about it relaxed: the
     * name on the certificate is still checked. */
    [[nodiscard]] tls_config trusting_client(std::string hostname = "localhost") const {
        auto config = tls_client_config(std::move(hostname));
        config.context->load_verify_file(cert());
        return config;
    }

  private:
    static void write(const std::string& path, const char* contents) {
        std::ofstream out(path, std::ios::binary | std::ios::trunc);
        out << contents;
        out.close();
        if (!out)
            throw std::runtime_error("could not write " + path);
    }

    static std::atomic<int> m_counter;
    std::filesystem::path m_dir;
};

std::atomic<int> cert_files::m_counter{0};

TEST_CASE("tls: data survives a round trip in both directions", "[sg::net::tls]") {
    scoped_deadline dl("tls round trip deadline");
    cert_files certs;

    std::mutex mutex;
    std::string server_received;
    std::atomic_int sessions_created{0};

    tcp_server server;
    server.start({ep},
                 {.OnSessionCreated = [&](tcp_server&, tcp_server::session_id_t) { ++sessions_created; },
                  .OnSessionDataAvailable =
                      [&](tcp_server& s, tcp_server::session_id_t id, const std::byte* data,
                          size_t size) {
                          {
                              std::lock_guard lock(mutex);
                              server_received.append(reinterpret_cast<const char*>(data), size);
                          }
                          s.write(id, "pong");
                      }},
                 {.make_transport = certs.server_factory()});

    std::string client_received;
    std::atomic_bool got_reply{false};

    tcp_client client;
    client.connect(
        ep,
        [&](tcp_session&, const std::byte* data, size_t size) {
            client_received.append(reinterpret_cast<const char*>(data), size);
            got_reply = true;
        },
        nullptr, {}, tls_transport_factory(certs.trusting_client()));

    REQUIRE(client.is_connected());

    client.session().write("ping");

    while (!got_reply)
        std::this_thread::sleep_for(std::chrono::milliseconds(5));

    REQUIRE(client_received == "pong");
    {
        std::lock_guard lock(mutex);
        REQUIRE(server_received == "ping");
    }

    /* Deliberately checked only after the exchange, not straight after connect(). In TLS 1.3 the
     * client sends the last handshake message, so its async_handshake completes while the server
     * is still waiting to read it: a connected client does not imply a registered session yet. */
    REQUIRE(sessions_created == 1);

    client.disconnect();
    server.stop_async();
    server.future_get_once();
}

TEST_CASE("tls: a graceful stop is reported as a clean disconnection", "[sg::net::tls]") {
    scoped_deadline dl("tls clean stop deadline");
    cert_files certs;

    tcp_server server;
    server.start({ep}, {}, {.make_transport = certs.server_factory()});

    std::atomic_bool disconnected{false};
    std::exception_ptr disconnect_ex;

    tcp_client client;
    client.connect(
        ep, nullptr,
        [&](tcp_session&, std::exception_ptr ex) {
            disconnect_ex = ex;
            disconnected = true;
        },
        {}, tls_transport_factory(certs.trusting_client()));

    REQUIRE(client.is_connected());

    const auto start = std::chrono::steady_clock::now();
    client.session().stop_async();
    client.session().wait_until_stopped();
    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - start);

    REQUIRE(disconnected);
    /* the close_notify exchange is not an error, whether or not it got through */
    REQUIRE(disconnect_ex == nullptr);

    /* The peer answers the close_notify, so this costs a round trip on loopback and nowhere near
     * tls_config::shutdown_timeout_msec. Guards against the shutdown silently always timing out,
     * which would be invisible except as a 1s pause on every graceful close. */
    CAPTURE(elapsed.count());
    REQUIRE(elapsed < std::chrono::milliseconds(300));

    server.stop_async();
    server.future_get_once();
}

/* How a raw TLS peer saw its end of the stream: a close_notify reads as eof, a bare TCP close as
 * ssl::error::stream_truncated. That distinction is the entire point of sending close_notify, and
 * it is the only thing that can tell a real graceful shutdown from one that merely looks quick. */
static boost::system::error_code peer_stream_end(const cert_files& certs,
                                                 bool force_stop,
                                                 std::chrono::milliseconds& elapsed) {
    boost::asio::io_context peer_context;
    boost::asio::ip::tcp::acceptor acceptor(
        peer_context, {boost::asio::ip::make_address(ep.ip), ep.port});

    auto server_config = tls_server_config(certs.cert(), certs.key());
    boost::asio::ip::tcp::socket peer(peer_context);
    boost::system::error_code read_ec;

    /* Reads until the stream ends and reports why. It never sends a close_notify of its own, so a
     * shutdown that waited for one would sit here until it timed out. */
    std::jthread peer_thread([&] {
        acceptor.accept(peer);
        boost::asio::ssl::stream<boost::asio::ip::tcp::socket&> stream(peer,
                                                                      *server_config.context);
        boost::system::error_code ec;
        stream.handshake(boost::asio::ssl::stream_base::server, ec);
        if (ec) {
            read_ec = ec;
            return;
        }

        char buffer[64];
        stream.read_some(boost::asio::buffer(buffer), read_ec);
    });

    auto config = certs.trusting_client();
    config.shutdown_timeout_msec = 5000; // deliberately long, so any wait would be obvious

    tcp_client client;
    client.connect(ep, nullptr, nullptr, {}, tls_transport_factory(config));
    REQUIRE(client.is_connected());

    const auto start = std::chrono::steady_clock::now();
    if (force_stop)
        client.session().stop_async_force();
    else
        client.session().stop_async();
    client.session().wait_until_stopped();
    elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - start);

    peer_thread.join();
    peer.close();

    return read_ec;
}

TEST_CASE("tls: a graceful stop sends close_notify, without waiting for a reply", "[sg::net::tls]") {
    scoped_deadline dl("tls graceful close_notify deadline");
    cert_files certs;

    std::chrono::milliseconds elapsed{};
    const auto ec = peer_stream_end(certs, false, elapsed);

    /* The peer saw a deliberate end of stream rather than a connection that just vanished. On POSIX
     * the close_notify surfaces as a clean EOF. On Windows the socket teardown reaches the peer as
     * an abortive close (connection_aborted/reset) before the buffered close_notify can be read as
     * EOF, so there the peer's error code cannot tell a graceful stop from a forced one -- the
     * timing check below is the portable guarantee that we did not wait on the peer. */
#ifndef _WIN32
    REQUIRE(ec == boost::asio::error::eof);
#else
    REQUIRE((ec == boost::asio::error::eof ||
             ec == boost::asio::error::connection_aborted ||
             ec == boost::asio::error::connection_reset));
#endif

    /* ...and we did not pay for it: the peer never answers, so anything approaching
     * shutdown_timeout_msec here means the shutdown went back to waiting on a reply. */
    CAPTURE(elapsed.count());
    REQUIRE(elapsed < std::chrono::milliseconds(500));
}

TEST_CASE("tls: a forced stop skips the close_notify", "[sg::net::tls]") {
    scoped_deadline dl("tls forced stop deadline");
    cert_files certs;

    std::chrono::milliseconds elapsed{};
    const auto ec = peer_stream_end(certs, true, elapsed);

    REQUIRE((ec == boost::asio::ssl::error::stream_truncated ||
             ec == boost::asio::error::connection_reset ||
             ec == boost::asio::error::connection_aborted));

    CAPTURE(elapsed.count());
    REQUIRE(elapsed < std::chrono::milliseconds(500));
}

TEST_CASE("tls: forcing a stop escalates a graceful stop already under way", "[sg::net::tls]") {
    scoped_deadline dl("tls stop escalation deadline");
    cert_files certs;

    boost::asio::io_context peer_context;
    boost::asio::ip::tcp::acceptor acceptor(
        peer_context, {boost::asio::ip::make_address(ep.ip), ep.port});

    auto server_config = tls_server_config(certs.cert(), certs.key());
    boost::asio::ip::tcp::socket peer(peer_context);

    /* A peer that completes the handshake and then goes silent -- it never reads and never sends a
     * close_notify back. */
    std::jthread peer_thread([&] {
        acceptor.accept(peer);
        boost::asio::ssl::stream<boost::asio::ip::tcp::socket&> stream(peer,
                                                                      *server_config.context);
        boost::system::error_code ec;
        stream.handshake(boost::asio::ssl::stream_base::server, ec);
    });

    auto config = certs.trusting_client();
    /* No shutdown timeout: if force could not cut a parked graceful stop short, this would hang. */
    config.shutdown_timeout_msec = 0;

    std::exception_ptr disconnect_ex;
    std::atomic_int disconnects{0};

    tcp_client client;
    client.connect(
        ep, nullptr,
        [&](tcp_session&, std::exception_ptr ex) { disconnect_ex = ex; ++disconnects; }, {},
        tls_transport_factory(config));
    REQUIRE(client.is_connected());

    /* Graceful stop, then immediately force it: force must take the session down, exactly once. */
    client.session().stop_async();
    client.session().stop_async_force();
    client.session().wait_until_stopped();

    REQUIRE(disconnects.load() == 1);
    REQUIRE(disconnect_ex == nullptr); // a deliberate stop is reported as clean

    peer_thread.join();
    peer.close();
}

TEST_CASE("tls: check google HTTTPS get rquest", "[sg::net::tls]") {

    tcp_client client;
    std::string str;
    std::binary_semaphore sem{0};

    tcp_session::Callbacks::OnDataAvailable on_data_available =
        [&](tcp_session&, const std::byte* data, size_t size) {
            str.append(reinterpret_cast<const char*>(data), size);
            sem.release();
        };

    client.connect({"google.com", 443}, on_data_available, nullptr, {},
                   sg::net::tls_transport_factory(sg::net::tls_client_config("google.com")));

    client.session().write("GET / HTTP/1.1\r\n"
                           "Host: google.com\r\n"
                           "User-Agent: sg-net-example\r\n"
                           "Connection: close\r\n"
                           "\r\n");

    sem.acquire();
    REQUIRE_THAT(str, Catch::Matchers::StartsWith("HTTP/1"));
}

TEST_CASE("tls: a certificate for the wrong name is refused", "[sg::net::tls]") {
    scoped_deadline dl("tls hostname verification deadline");
    cert_files certs;

    tcp_server server;
    server.start({ep}, {}, {.make_transport = certs.server_factory()});

    /* The certificate is trusted, but it is issued to "localhost", not to this. Without the
     * hostname check the handshake would sail through. */
    tcp_client client;
    REQUIRE_THROWS(client.connect(ep, nullptr, nullptr,
                                  {}, tls_transport_factory(certs.trusting_client("wrong.example"))));
    REQUIRE_FALSE(client.is_connected());

    server.stop_async();
    server.future_get_once();
}

TEST_CASE("tls: an untrusted certificate is refused", "[sg::net::tls]") {
    scoped_deadline dl("tls trust deadline");
    cert_files certs;

    tcp_server server;
    server.start({ep}, {}, {.make_transport = certs.server_factory()});

    /* tls_client_config() trusts only the system store, which has never heard of our
     * self-signed certificate, so this must fail closed. */
    tcp_client client;
    REQUIRE_THROWS(client.connect(ep, nullptr, nullptr, {}, tls_transport_factory(tls_client_config("localhost"))));
    REQUIRE_FALSE(client.is_connected());

    server.stop_async();
    server.future_get_once();
}

TEST_CASE("tls: a peer that never handshakes times out", "[sg::net::tls]") {
    scoped_deadline dl("tls handshake timeout deadline");
    cert_files certs;

    /* A plaintext server: it accepts the connection and then waits for data that means nothing to
     * it, so the client's handshake gets no reply at all. */
    tcp_server server;
    server.start({ep}, {});

    auto config = certs.trusting_client();
    config.handshake_timeout_msec = 500;

    const auto start = std::chrono::steady_clock::now();

    tcp_client client;
    REQUIRE_THROWS_AS(client.connect(ep, nullptr, nullptr, {}, tls_transport_factory(config)),
                      sg::exceptions::net::time_out);

    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - start);
    CAPTURE(elapsed.count());
    REQUIRE(elapsed >= std::chrono::milliseconds(400));
    REQUIRE(elapsed < std::chrono::seconds(5));

    REQUIRE_FALSE(client.is_connected());

    server.stop_async();
    server.future_get_once();
}

TEST_CASE("tls: a client that never handshakes is reported and then dropped", "[sg::net::tls]") {
    scoped_deadline dl("tls plaintext client deadline");
    cert_files certs;

    std::atomic_int sessions_created{0};
    std::atomic_int disconnections{0};
    std::exception_ptr disconnect_ex;

    auto config = tls_server_config(certs.cert(), certs.key());
    config.handshake_timeout_msec = 300;

    tcp_server server;
    server.start(
        {ep},
        {.OnSessionCreated = [&](tcp_server&, tcp_server::session_id_t) { ++sessions_created; },
         .OnDisconnected = [&](tcp_server&, tcp_server::session_id_t, std::exception_ptr ex) {
             disconnect_ex = ex;
             ++disconnections;
         }},
        {.make_transport = tls_transport_factory(config)});

    /* A plaintext client: it completes the TCP connect and then says nothing the server can make
     * sense of, so the handshake times out. */
    tcp_client client;
    client.connect(ep, nullptr, nullptr, {});

    /* long enough for the server's handshake to have timed out and given up */
    std::this_thread::sleep_for(std::chrono::milliseconds(900));

    /* The connection is reported as a session as soon as the socket is up, before the handshake,
     * which is what makes it visible to clients_count() and to the shutdown drain. It is then
     * reported as disconnected, carrying the reason it never got any further. */
    REQUIRE(sessions_created == 1);
    REQUIRE(disconnections == 1);
    REQUIRE(disconnect_ex != nullptr);
    REQUIRE_THROWS_AS(std::rethrow_exception(disconnect_ex), sg::exceptions::net::time_out);

    /* Reported both ways round, so nothing is left behind. */
    REQUIRE(server.clients_count() == 0);

    client.disconnect();
    server.stop_async();
    server.future_get_once();
}

TEST_CASE("tls: writes are refused until the handshake completes", "[sg::net::tls]") {
    scoped_deadline dl("tls handshake-gating deadline");
    cert_files certs;

    std::atomic_bool findable{false};
    std::atomic_bool connected_in_callback{true};
    std::atomic_bool write_threw{false};
    std::atomic<tcp_session::state_t> state_in_callback{tcp_session::state_t::running};

    tcp_server server;
    server.start(
        {ep},
        {.OnSessionCreated =
             [&](tcp_server& s, tcp_server::session_id_t id) {
                 /* Runs before this session's handshake. Reporting it this early is what makes it
                  * findable and trackable -- but it cannot carry data yet, so it is not connected
                  * and write() has to refuse. */
                 try {
                     auto session = s.session(id);
                     findable.store(session != nullptr);
                     state_in_callback.store(session->state());
                     connected_in_callback.store(session->is_connected());
                     session->write("too early");
                 } catch (...) {
                     write_threw.store(true);
                 }
             },
         .OnSessionDataAvailable =
             [&](tcp_server& s, tcp_server::session_id_t id, const std::byte*, size_t) {
                 s.write(id, "pong");
             }},
        {.make_transport = certs.server_factory()});

    std::string received;
    std::atomic_bool got_reply{false};

    tcp_client client;
    client.connect(
        ep,
        [&](tcp_session&, const std::byte* data, size_t size) {
            received.append(reinterpret_cast<const char*>(data), size);
            got_reply.store(true, std::memory_order::release);
        },
        nullptr, {}, tls_transport_factory(certs.trusting_client()));

    /* The handshake has landed by the time connect() returns, so the session that refused a write
     * mid-handshake is now usable and a normal round trip goes through. */
    REQUIRE(client.is_connected());
    client.session().write("ping");

    while (!got_reply.load(std::memory_order::acquire))
        std::this_thread::sleep_for(std::chrono::milliseconds(5));

    REQUIRE(received == "pong");

    REQUIRE(findable.load());
    REQUIRE(state_in_callback.load() == tcp_session::state_t::handshaking);
    REQUIRE_FALSE(connected_in_callback.load());
    REQUIRE(write_threw.load());

    client.disconnect();
    server.stop_async();
    server.future_get_once();
}

TEST_CASE("tls: full-duplex traffic survives a multi-threaded pool", "[sg::net::tls]") {
    /* Both directions share one SSL object that OpenSSL does not lock; the session is safe only
     * because it drives reader and writer from one strand. Run the server on several pool threads
     * and hammer both directions at once: if that invariant breaks, SSL_read/SSL_write race and the
     * stream corrupts or crashes. A clean, exact-byte round trip is the evidence it holds. */
    scoped_deadline dl("tls full-duplex stress deadline");
    cert_files certs;

    constexpr int msgs = 3000;
    const std::string payload(64, 'x');
    const size_t total = static_cast<size_t>(msgs) * payload.size();

    std::atomic<size_t> server_received{0};
    std::atomic<size_t> client_received{0};
    std::once_flag server_push;

    tcp_server server;
    server.start(
        {ep},
        {.OnSessionDataAvailable =
             [&](tcp_server& s, tcp_server::session_id_t id, const std::byte*, size_t size) {
                 server_received.fetch_add(size, std::memory_order::relaxed);
                 /* First bytes in => handshake landed, session running and writable. Push our own
                  * stream back while the reader drains the client's -- both directions hot at once. */
                 std::call_once(server_push, [&] {
                     for (int i = 0; i < msgs; ++i)
                         s.write(id, payload);
                 });
             }},
        {.no_threads = 4, .make_transport = certs.server_factory()});

    tcp_client client;
    client.connect(
        ep,
        [&](tcp_session&, const std::byte*, size_t size) {
            client_received.fetch_add(size, std::memory_order::relaxed);
        },
        nullptr, {}, tls_transport_factory(certs.trusting_client()));

    REQUIRE(client.is_connected());

    for (int i = 0; i < msgs; ++i)
        client.session().write(payload);

    while (server_received.load(std::memory_order::relaxed) < total ||
           client_received.load(std::memory_order::relaxed) < total)
        std::this_thread::sleep_for(std::chrono::milliseconds(2));

    /* Exactly the bytes sent, no more: nothing else writes, so overshoot would mean corruption. */
    REQUIRE(server_received.load() == total);
    REQUIRE(client_received.load() == total);

    client.disconnect();
    server.stop_async();
    server.future_get_once();
}

TEST_CASE("tls: OnSessionNegotiated fires after the handshake, before data", "[sg::net::tls]") {
    scoped_deadline dl("tls negotiated callback deadline");
    cert_files certs;

    std::atomic_int seq{0};
    std::atomic_int created_at{-1}, negotiated_at{-1}, data_at{-1};
    std::atomic_int negotiated_count{0};
    std::atomic_bool connected_in_cb{false};
    std::binary_semaphore data_seen{0};

    tcp_server server;
    server.start(
        {ep},
        {.OnSessionCreated = [&](tcp_server&, tcp_server::session_id_t) { created_at = seq++; },
         .OnSessionNegotiated =
             [&](tcp_server& s, tcp_server::session_id_t id) {
                 negotiated_at = seq++;
                 ++negotiated_count;
                 auto sess = s.session(id); // registered and running by now
                 connected_in_cb = (sess != nullptr && sess->is_connected());
             },
         .OnSessionDataAvailable =
             [&](tcp_server&, tcp_server::session_id_t, const std::byte*, size_t) {
                 if (int expected = -1; data_at.compare_exchange_strong(expected, seq++))
                     data_seen.release();
             }},
        {.make_transport = certs.server_factory()});

    tcp_client client;
    client.connect(ep, nullptr, nullptr, {}, tls_transport_factory(certs.trusting_client()));
    REQUIRE(client.is_connected());

    client.session().write("ping");
    data_seen.acquire();

    client.disconnect();
    server.stop_async();
    server.future_get_once();

    REQUIRE(negotiated_count.load() == 1);
    REQUIRE(connected_in_cb.load());
    REQUIRE(created_at.load() == 0);    // created, then
    REQUIRE(negotiated_at.load() == 1); // negotiated, then
    REQUIRE(data_at.load() == 2);       // data
}

TEST_CASE("tls: dont_read is rejected", "[sg::net::tls]") {
    cert_files certs;

    boost::asio::io_context context;
    boost::asio::ip::tcp::socket socket(context);

    tcp_session::Callbacks callbacks{
        .onDataAvailable = [](tcp_session&, const std::byte*, size_t) {}};

    REQUIRE_THROWS_AS(
        tcp_session::create(context, std::move(socket),
                            tls_transport_factory(tls_server_config(certs.cert(), certs.key())),
                            callbacks, {.dont_read = true}),
        std::invalid_argument);
}
