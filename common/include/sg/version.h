#pragma once

#include <sg/export/common.h>

#include <iostream>
#include <string>
#include <vector>

namespace sg {

class SG_COMMON_EXPORT version {
  public:
    version();
    explicit version(std::initializer_list<unsigned int>);
    explicit version(std::string);

    constexpr bool operator==(const version& rhs) const = default;
    constexpr std::strong_ordering operator<=>(version const& rhs) const =default;
    friend std::ostream& operator<<(std::ostream& os, const version& t);

    explicit operator std::string() const;

    std::vector<unsigned int> versions{};
};

std::ostream &operator<<(std::ostream &os, const version &t);


} // namespace sg
