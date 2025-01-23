#include <sg/net.h>

#include <catch2/catch_test_macros.hpp>
#include <ranges>

TEST_CASE("SG::common net namespace") {
    SECTION("checking sg::net::interfaces() returns a loopback address") {
        auto loopback = sg::net::interfaces() |
                        std::views::filter([](const sg::net::interface_details& inter) {
                            return (inter.address == "127.0.0.1" && inter.is_internal &&
                                    inter.family == sg::net::address_family::IPv4) ||
                                   (inter.address == "::1" && inter.is_internal &&
                                    inter.family == sg::net::address_family::IPv6);
                        });
        REQUIRE(!std::ranges::empty(loopback));
    }
}
