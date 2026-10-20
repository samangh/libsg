#pragma once

#include "sg/meta.h"
#include "sg/debug.h"

#include <fmt/format.h>
#include <map>
#include <type_traits>
#include <utility>

namespace sg::internal::enumeration {

/* Bidirectional enum<->value lookup.
 *
 * A pair of std::map is all the two accessors below need. We could use arelational container
 * such as boost::bimap, but that is overkill (bimap is ~150k lines!) */
template <typename TEnum, typename TValue> struct enum_val_maps {
    std::map<TEnum, TValue> to_value;
    std::map<TValue, TEnum> to_enum;
};

/* Builds both lookups from the std::map filled in by populate_enum_value_map().
 *
 * Throws std::logic_error if two enumerators share a value, as the reverse lookup would then be
 * ambiguous. */
template <typename TEnum, typename TValue>
    requires(std::is_enum_v<TEnum>)
[[nodiscard]] enum_val_maps<TEnum, TValue> generate_enum_val_maps() {
    /* get std::map via argument-dependent lookup */
    std::map<TEnum, TValue> map_;
    populate_enum_value_map(map_);

    enum_val_maps<TEnum, TValue> maps;
    for (const auto& [en, val] : map_)
        if (!maps.to_enum.emplace(val, en).second)
            SG_THROW(std::logic_error, fmt::format("duplicate enum value specified for {}",
                                                   sg::meta::type_name<TEnum>()));
    maps.to_value = std::move(map_);

    return maps;
}

/* Shared by both accessors, so that the maps are built once per <TEnum, TValue> pair.
 *
 * A failed construction is not cached: the next call retries, and so throws again. */
template <typename TEnum, typename TValue>
    requires(std::is_enum_v<TEnum>)
[[nodiscard]] const enum_val_maps<TEnum, TValue>& enum_val_lookup() {
    static const auto maps = generate_enum_val_maps<TEnum, TValue>();
    return maps;
}

} // namespace sg::internal::enumeration
