#pragma once

#include <sg/export/common.h>

#include <string>
#include <vector>

namespace sg::net {

enum class address_family { IPv4, IPv6 };

struct interface_details {
    address_family family;
    std::string physical_address;
    std::string address;
    std::string netmask;
    bool is_internal;
};

/**
 * @brief returns vector of all system interfaces
 * @return vector of all system interfaces
 */
SG_COMMON_EXPORT std::vector<sg::net::interface_details> interfaces();

}  // namespace sg::net
