#pragma once

/* Helpers for the two implementations of sg::net::interfaces().
 *
 * This is the only place that pulls in the platform socket headers, so that neither
 * implementation file needs a conditional for the Windows/POSIX split. On Windows <winsock2.h>
 * must precede every other Windows header, <iphlpapi.h> included, so include this header before
 * anything else.
 *
 * Note:
 *   - MSYS2's `msys` environment, and Cygwin, are POSIX (they do not set _WIN32);
 *   - MSYS2's mingw64/ucrt64/clang64 environments are native Windows and do.
 */

#ifdef _WIN32
    /* IP_ADAPTER_UNICAST_ADDRESS_LH, and so OnLinkPrefixLength, needs Vista or later.*/
    #if _WIN32_WINNT < 0x0600
        #error "need Vista or later (so _WIN32_WINNT >= 0x0600)"
    #endif

    #ifndef NOMINMAX
        #define NOMINMAX
    #endif
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif

    #include <winsock2.h>
    // clang-format off
    #include <ws2tcpip.h>
    #include <iphlpapi.h>
    // clang-format on
#else
    #include <arpa/inet.h>
    #include <netinet/in.h>
    #include <sys/socket.h>
#endif

#include "sg/net/net.h"

#include <array>
#include <bit>
#include <cstddef>
#include <cstring>
#include <span>
#include <string>

namespace {

/* keeps the helper signatures below readable */
using sg::net::address_family;
using sg::net::mac_address;

constexpr unsigned address_size(address_family family) {
    return family == address_family::IPv4 ? 4 : 16;
}

/* Where the address sits inside the sockaddr that carries it. */
size_t address_offset(address_family family) {
    return family == address_family::IPv4 ? offsetof(sockaddr_in, sin_addr)
                                          : offsetof(sockaddr_in6, sin6_addr);
}

address_family to_family(int af) {
    return af == AF_INET6 ? address_family::IPv6 : address_family::IPv4;
}

int to_af(address_family family) {
    return family == address_family::IPv4 ? AF_INET : AF_INET6;
}

/* The address bytes held inside a sockaddr, in network order. */
const std::byte *address_bytes(const sockaddr *sa, address_family family) {
    return reinterpret_cast<const std::byte *>(sa) + address_offset(family);
}

/* IPv6 scope (zone) id, which is the interface index. Zero for IPv4 and for addresses that need
 * no zone. */
unsigned scope_id_of(const sockaddr *sa) {
    if (sa->sa_family != AF_INET6)
        return 0;

    return reinterpret_cast<const sockaddr_in6 *>(sa)->sin6_scope_id;
}

/* Renders a numeric address.
 *
 * inet_ntop rather than getnameinfo: getnameinfo would need Winsock to have been initialised on
 * Windows, which neither implementation file guarantees, and its zone suffix differs between
 * platforms (an interface name on glibc, an index on Windows). inet_ntop needs no initialisation
 * and lets us append the scope id ourselves, identically everywhere. */
std::string address_to_string(const sockaddr *sa, address_family family,
                                     unsigned scope_id) {
    char buf[INET6_ADDRSTRLEN];
    if (::inet_ntop(to_af(family), address_bytes(sa, family), buf, sizeof(buf)) == nullptr)
        throw sg::exceptions::net::other("could not format address");

    /* without its zone, a link-local address is ambiguous */
    if (scope_id != 0)
        return std::string{buf} + '%' + std::to_string(scope_id);

    return buf;
}

/* CIDR prefix length, i.e. the number of bits set in the mask. */
unsigned prefix_length_of(std::span<const std::byte> mask) {
    unsigned bits = 0;
    for (auto byte : mask)
        bits += static_cast<unsigned>(std::popcount(std::to_integer<unsigned char>(byte)));

    return bits;
}

/* The two forms interface_details reports a netmask in. Both platforms build it from mask bytes
 * through here, so the string and the prefix length cannot drift apart. */
struct netmask_t {
    std::string text;
    unsigned prefix_length{};
};

netmask_t to_netmask(address_family family, const std::array<std::byte, 16> &bytes) {
    auto const mask = std::span{bytes}.first(address_size(family));

    char buf[INET6_ADDRSTRLEN];
    if (::inet_ntop(to_af(family), mask.data(), buf, sizeof(buf)) == nullptr)
        throw sg::exceptions::net::other("could not format netmask");

    return {buf, prefix_length_of(mask)};
}

/* Leaves the address all-zero if it is not an Ethernet-style one, which is how an interface
 * without a hardware address is reported. */
mac_address to_mac(const void *data, size_t size) {
    mac_address mac;
    if (size == mac.bytes.size())
        std::memcpy(mac.bytes.data(), data, mac.bytes.size());

    return mac;
}

} // namespace
