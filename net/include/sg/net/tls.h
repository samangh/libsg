#pragma once

#ifdef LIBSG_NET_TLS

#include <sg/export/net.h>

#include <boost/asio/ssl/context.hpp>
#include <boost/asio/ssl/stream.hpp>

#include <memory>
#include <string>

/* This header is what pulls OpenSSL in */

namespace sg::net {

/** Which end of the TLS connection this is */
enum class tls_role { client, server };

/** @brief Settings for TLS TCP transport. Cheap to copy and can back multiple connections.
 *
 * Prefer @ref tls_server_config() / @ref tls_client_config() unless you need to set up the
 * @c boost::asio::ssl::context yourself.
 */
struct tls_config {
    /** Holds the certificates, keys and trust store. Must not be null. */
    std::shared_ptr<boost::asio::ssl::context> context;

    tls_role role{tls_role::server};

    /** Hostname to be checked against the server's certificate. Leave this empty if the peer is not
     * being authenticated by name */
    std::string hostname{};

    unsigned handshake_timeout_msec{1000}; // 0 = no timeout

    /** timeout for a graceful shutdown.*/
    unsigned shutdown_timeout_msec{1000}; //0 = no timeout.
};

/** @brief Config for the listening end, from a PEM certificate chain and its private key.
 *
 * Client certificates are neither requested nor checked. For mutual TLS, set a verify mode and a
 * trust store on @c tls_config::context afterwards.
 */
SG_NET_EXPORT tls_config tls_server_config(const std::string& cert_chain_file,
                                           const std::string& private_key_file);

/** @brief Config for a client, authenticating the peer against the system trust store.
 *
 * Pass the name you expect to find on the certificate, which is not always the address you dialled.
 */
SG_NET_EXPORT tls_config tls_client_config(std::string hostname);

} // namespace sg::net

#endif