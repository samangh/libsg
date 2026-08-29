#include "sg/net/tls.h"

#include "sg/net/net.h"
#include "sg/debug.h"

#include "transport_timeout.h"

#include <boost/asio/ssl/host_name_verification.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/asio/write.hpp>

#ifdef _WIN32
    #include <openssl/ssl.h> // for SSL_CTX_load_verify_store()
#endif

#include <stdexcept>

namespace {

/* Everything below TLS 1.2 is broken in ways not worth being compatible with, and asio's `tls`
 * methods still allow them. */
boost::asio::ssl::context::options baseline_options() {
    return boost::asio::ssl::context::default_workarounds | //
           boost::asio::ssl::context::no_sslv2 |            //
           boost::asio::ssl::context::no_sslv3 |            //
           boost::asio::ssl::context::no_tlsv1 |            //
           boost::asio::ssl::context::no_tlsv1_1 |          //
           boost::asio::ssl::context::single_dh_use;
}

/* Points the client's verifier at the operating system's trust store.
 *
 * On POSIX that is set_default_verify_paths() -- OpenSSL's configured CA bundle / directory. On
 * Windows OpenSSL ships no default bundle and does not read the Windows certificate store, so that
 * call loads nothing and every public certificate fails to verify. Since OpenSSL 3.2 the built-in
 * "winstore" provider exposes the Windows store as a CA-store URI; load that instead. */
void load_system_trust_store(boost::asio::ssl::context& context) {
#ifdef _WIN32
    if (::SSL_CTX_load_verify_store(context.native_handle(), "org.openssl.winstore://") != 1)
        SG_THROW(std::runtime_error, "sg::net: could not load the Windows certificate store");
#else
    context.set_default_verify_paths();
#endif
}

} // namespace

namespace sg::net {

tls_config tls_server_config(const std::string& cert_chain_file,
                             const std::string& private_key_file) {
    auto context =
        std::make_shared<boost::asio::ssl::context>(boost::asio::ssl::context::tls_server);

    context->set_options(baseline_options());
    context->use_certificate_chain_file(cert_chain_file);
    context->use_private_key_file(private_key_file, boost::asio::ssl::context::pem);

    return {.context = std::move(context), .role = tls_role::server};
}

tls_config tls_client_config(std::string hostname) {
    auto context =
        std::make_shared<boost::asio::ssl::context>(boost::asio::ssl::context::tls_client);

    context->set_options(baseline_options());
    load_system_trust_store(*context);

    /* Without this the handshake would succeed against any certificate at all. The name is checked
     * separately, per connection, by tls_transport. */
    context->set_verify_mode(boost::asio::ssl::verify_peer);

    return {.context = std::move(context),
            .role = tls_role::client,
            .hostname = std::move(hostname)};
}

} // namespace sg::net
