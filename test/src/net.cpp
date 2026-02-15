#include <catch2/catch_test_macros.hpp>
#include <sg/net.h>

#include <ranges>

TEST_CASE("sg::net check interfaces() returns a loopback address", "[sg::net]") {
    auto loopback =
        sg::net::interfaces() | std::views::filter([](const sg::net::interface_details& inter) {
            return (inter.address == "127.0.0.1" && inter.is_internal &&
                    inter.family == sg::net::address_family::IPv4) ||
                   (inter.address == "::1" && inter.is_internal &&
                    inter.family == sg::net::address_family::IPv6);
        });
    REQUIRE(!std::ranges::empty(loopback));
}

TEST_CASE("sg::net check resolve()", "[sg::net]") {
    // check loopback
    REQUIRE((sg::net::resolve("localhost").front() == "127.0.0.1" ||
             sg::net::resolve("localhost").front() == "::1"));

    // check error handling
    REQUIRE_THROWS_AS(sg::net::resolve("localhozxcst"),
                      sg::exceptions::exception<sg::exceptions::errors::net>);

    // Check exact error code
    try {
        sg::net::resolve("localhozxcst");
    } catch (const sg::exceptions::net<sg::exceptions::errors::net::host_not_found>&) {}
}