/* **************************************************************************
 * Derived from libuv's uv_interface_addresses(), which is under an MIT
 * license. The code is rewritten, but its structure and platform handling
 * follow libuv's. See LICENCES/libuv.md
 * **************************************************************************
 *
 * Copyright (c) 2015-present libuv project contributors.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

/* Built wherever _WIN32 is defined, which is every native Windows toolchain:
 *
 *  - MSVC
 *  - the mingw-w64 environments (MSYS2's mingw64/ucrt64/clang64)
 *
 * Note that MSYS2's `msys` environment and Cygwin are POSIX emulation (and don't set _WIN32),
 * so they take interfaces_posix.cpp instead. */

#ifndef _WIN32
    #error "This file should only be used in _WIN32"
#endif

#include "include/interfaces_common.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <optional>
#include <string>
#include <vector>

namespace {

std::string to_utf8(const wchar_t *wide) {
    if (wide == nullptr || *wide == L'\0')
        return {};

    /* the returned size counts the null terminator, which std::string provides itself */
    auto const size = ::WideCharToMultiByte(CP_UTF8, 0, wide, -1, nullptr, 0, nullptr, nullptr);
    if (size <= 1)
        return {};

    std::string out(static_cast<size_t>(size) - 1, '\0');
    if (::WideCharToMultiByte(CP_UTF8, 0, wide, -1, out.data(), size, nullptr, nullptr) == 0)
        return {};

    return out;
}

constexpr unsigned max_prefix_length(address_family family) {
    return 8 * address_size(family);
}

/* Windows reports a prefix length rather than a mask, so the bytes have to be synthesised. */
std::array<std::byte, 16> netmask_from_prefix(unsigned prefix) {
    std::array<std::byte, 16> bytes{};
    for (size_t bit = 0; bit < std::min<size_t>(prefix, 8 * bytes.size()); ++bit)
        bytes[bit / 8] |= static_cast<std::byte>(0x80u >> (bit % 8));

    return bytes;
}

/* GetAdaptersAddresses() reports how much room it wanted when the buffer was too small, and the
 * answer can grow between calls as adapters appear, so keep asking until it fits. Returns an
 * empty buffer when the host has no adapters at all, which is a legitimate state. */
std::vector<std::byte> adapter_addresses() {
    constexpr ULONG flags =
        GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST | GAA_FLAG_SKIP_DNS_SERVER;
    constexpr ULONG max_size = 64 * 1024 * 1024;

    std::vector<std::byte> buffer;
    ULONG size = 16 * 1024;

    for (;;) {
        buffer.resize(size);

        auto const err =
            ::GetAdaptersAddresses(AF_UNSPEC, flags, nullptr,
                                   reinterpret_cast<IP_ADAPTER_ADDRESSES *>(buffer.data()), &size);

        if (err == NO_ERROR)
            return buffer;

        if (err == ERROR_NO_DATA)
            return {};

        if (err != ERROR_BUFFER_OVERFLOW || size > max_size)
            throw sg::exceptions::net::other("GetAdaptersAddresses() failed with error " +
                                         std::to_string(err));
    }
}

} // namespace

namespace sg::net {

std::vector<interface_details> interfaces() {
    auto const buffer = adapter_addresses();
    if (buffer.empty())
        return {};

    std::vector<interface_details> result;
    for (auto const *adapter = reinterpret_cast<const IP_ADAPTER_ADDRESSES *>(buffer.data());
         adapter != nullptr; adapter = adapter->Next) {
        if (adapter->OperStatus != IfOperStatusUp)
            continue;

        auto const name = to_utf8(adapter->FriendlyName);
        auto const mac = to_mac(adapter->PhysicalAddress, adapter->PhysicalAddressLength);
        auto const is_loopback = adapter->IfType == IF_TYPE_SOFTWARE_LOOPBACK;

        for (auto const *unicast = adapter->FirstUnicastAddress; unicast != nullptr;
             unicast = unicast->Next) {
            auto const *sa = unicast->Address.lpSockaddr;
            if (sa->sa_family != AF_INET && sa->sa_family != AF_INET6)
                continue;

            auto const family = to_family(sa->sa_family);
            auto const scope_id = scope_id_of(sa);

            /* The field only exists in the _LH (Vista and later) layout, which the SDK may have
             * typedefed away to the XP one; libuv casts for the same reason. */
            unsigned const prefix =
                reinterpret_cast<const IP_ADAPTER_UNICAST_ADDRESS_LH *>(unicast)
                    ->OnLinkPrefixLength;

            /* 255 is the documented sentinel for "unknown", and anything above the family's
             * maximum is illegal. Report no netmask rather than fabricating a /32. */
            netmask_t mask;
            if (prefix <= max_prefix_length(family))
                mask = to_netmask(family, netmask_from_prefix(prefix));

            result.push_back({.name = name,
                              .family = family,
                              .address = address_to_string(sa, family, scope_id),
                              .netmask = std::move(mask.text),
                              .prefix_length = mask.prefix_length,
                              .scope_id = scope_id,
                              .physical_address = mac,
                              .is_loopback = is_loopback});
        }
    }

    return result;
}

} // namespace sg::net
