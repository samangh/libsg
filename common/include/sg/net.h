#pragma once

#include <sg/export/common.h>

#include <string>
#include <vector>
#include <cstdint>

namespace sg::net {

enum class address_family { IPv4, IPv6 };
typedef uint16_t port_t;

struct interface_details {
    address_family family;
    std::string physical_address;
    std::string address;
    std::string netmask;
    bool is_internal;
};

struct end_point{
    end_point() = default;
    end_point(std::string ip,  port_t port): ip(ip), port(port) {}

    std::string ip;
    port_t port;
};
/**
 * @brief returns vector of all system interfaces
 * @return vector of all system interfaces
 */
SG_COMMON_EXPORT std::vector<sg::net::interface_details> interfaces();

}  // namespace sg::net
