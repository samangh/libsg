#pragma once

#include <sg/export/common.h>

#include <iostream>
#include <string>
#include <vector>

namespace sg {

class SG_COMMON_EXPORT version {
  public:
    version();
    version(std::string);

    std::vector<unsigned int> versions;

    operator std::string() const;
    bool operator<(const version &rhs) const;
    bool operator<=(const version &rhs) const;
    bool operator>(const version &rhs) const;
    bool operator>=(const version &rhs) const;
    bool operator==(const version &rhs) const;
    bool operator!=(const version &rhs) const;

    friend std::ostream &operator<<(std::ostream &os, const version &t);
};

std::ostream &operator<<(std::ostream &os, const version &t);
;

} // namespace sg
