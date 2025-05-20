#pragma once

#include "sg/debug.h"

#include <fmt/format.h>

#include <boost/bimap.hpp>

#include <map>
#include <type_traits>

namespace sg::internal::enumeration {

/* generates boost bimap from standard std::map
 *
 * used by sg::enumeration::enum_to_val and sg::enumeration::enum_from_val */
template <typename TEnum, typename TValue>
    requires(std::is_enum_v<TEnum>)
[[nodiscard]] auto generate_enum_val_bimap() {
    // don't define collection type, boost::bimap will assume a set
    typedef boost::bimap<TEnum, TValue> TReturn;

    /* get std::map via argument-dependent lookup */
    std::map<TEnum, TValue> map_;
    populate_enum_value_map(map_);

    TReturn bimap;
    for (const auto& [en, val] : map_) {
        if (bimap.right.find(val) != bimap.right.end())
            throw std::logic_error(
                fmt::format("duplicate enum value specified for {}", sg::type_name<TEnum>()));
        bimap.insert(typename TReturn::value_type(en, val));
    }

    return bimap;
}

} // namespace sg::internal::enumeration
