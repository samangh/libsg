#pragma once

#include <sg/export/net.h>

#include <sg/exceptions.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

/* On Linux, SO_REUSEADDR is used so that a socket in TIME-WAIT state can be rebound after a
 * listening process is restarted. It does not allow two processes to listen on the exact same
 * (addr, port) combination. However, on Windows, it does, and SO_EXCLUSIVEADDRUSE is required to
 * reproduce the Linux behaviour.
 *
 * On macOS setting SO_REUSEADDR allows multiple threads to bind to the same port. But we don't
 * disable it by default, as otherwise a connection can't be used until it comes out TIME_WAIT
 * state.
 */
#if defined(_WIN32)
    #define LIBSG_NET_REUSEADDR_DEFAULT false
    #define LIBSG_NET_EXCLUSIVEADDRUSE_DEFAULT true
#elif defined(__APPLE__)
    #define LIBSG_NET_REUSEADDR_DEFAULT true
    #define LIBSG_NET_EXCLUSIVEADDRUSE_DEFAULT false
#else
    #define LIBSG_NET_REUSEADDR_DEFAULT true
    #define LIBSG_NET_EXCLUSIVEADDRUSE_DEFAULT false
#endif

namespace sg::exceptions::net {

SG_CREATE_EXCEPTION_ANY();
SG_CREATE_EXCEPTION(time_out, "operation timed out");
SG_CREATE_EXCEPTION(host_not_found, "host not found");
SG_CREATE_EXCEPTION(network_unreachable, "network unreachable");
SG_CREATE_EXCEPTION(session_not_found, "no such session");
SG_CREATE_EXCEPTION(buffer_full, "buffer is full");
SG_CREATE_EXCEPTION(other, "other error");

} // namespace sg::exceptions

namespace sg::net {

enum class address_family { IPv4, IPv6 };
typedef uint16_t port_t;

/** @brief A 48-bit hardware (MAC) address. */
struct SG_NET_EXPORT mac_address {
    std::array<std::byte, 6> bytes{};

    /** @brief True if every byte is zero, which is how Linux reports a loopback interface. */
    [[nodiscard]] bool is_zero() const noexcept;

    /** @brief Lower-case colon-separated form, e.g. "02:42:ac:11:00:02". */
    [[nodiscard]] std::string to_string() const;

    [[nodiscard]] bool operator==(const mac_address& rhs) const noexcept = default;
};

struct interface_details {
    /** Interface name. On POSIX this is the kernel name, e.g. "eth0". On Windows it is the
     *  adapter's friendly name, e.g. "Wi-Fi", which is localised, renameable by the user and not
     *  guaranteed to be unique, so do not treat it as a stable key. */
    std::string name;

    address_family family{address_family::IPv4};

    /** Numeric address. A link-local IPv6 address carries a "%<scope_id>" suffix, without which
     *  it is ambiguous. */
    std::string address;

    /** The netmask, e.g. "255.255.255.0". Empty if the OS did not report one. On Windows, where
     *  only a prefix length is available, it is derived from @ref prefix_length. */
    std::string netmask;

    /** Number of bits set in @ref netmask, i.e. the CIDR prefix length. Zero if the OS did not
     *  report a netmask. A netmask whose set bits are not contiguous is legal but vanishingly
     *  rare; for one of those @ref netmask is authoritative and this is just its bit count. */
    unsigned prefix_length{};

    /** IPv6 scope (zone) id, which is the interface index. Zero for IPv4, and for IPv6 addresses
     *  that need no zone. */
    unsigned scope_id{};

    /** The interface's hardware address, all-zero if it has none -- as loopback and
     *  point-to-point interfaces report, or fail to report, one. */
    mac_address physical_address;

    bool is_loopback{};
};

struct end_point{
    end_point() = default;
    end_point(std::string ip,  port_t port): ip(std::move(ip)), port(port) {}

    std::string ip;
    port_t port{};
};

struct keepalive_t {
    bool enable{true};            // Set true to enable keepalive, false to disable
    unsigned idle_seconds{60};    // seconds of no-activity before a keepalive packet is sent
    unsigned interval_seconds{5}; // Interval between each keep alive packet
    unsigned count{5};            // # of packets to send before terminating the connection
};

/**
 * @brief Returns one entry per address of every interface that is currently up and running.
 *
 * An interface with several addresses (typically one IPv4 and one or more IPv6) appears once per
 * address, with the same @ref interface_details::name each time.
 *
 * @throws sg::exceptions::net::other if the system interface list cannot be read
 */
SG_NET_EXPORT std::vector<sg::net::interface_details> interfaces();

/**
 *
 * @throws sg::exceptions::net on networking errors, other exceptions on non-networking erros
 * @param hostname
 * @return
 */
SG_NET_EXPORT std::vector<std::string> resolve(const std::string& hostname);

}  // namespace sg::net
