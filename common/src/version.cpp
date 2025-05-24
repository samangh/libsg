#include "sg/version.h"
#include "fmt/ranges.h"

namespace sg {

version::version() = default;
version::operator std::string() const { return fmt::format("{}", fmt::join(versions, ".")); }

version::version(std::string input) {
    const std::string delimiter = ".";

    while (input.find(delimiter) != std::string::npos) {
        auto pos = input.find(delimiter);
        auto extract = input.substr(0, pos);
        input.erase(0, pos + delimiter.length());

        versions.push_back(std::stoul(extract));
    }

    // Stuff after last .
    if (!input.empty())
        versions.push_back(std::stoul(input));
}

bool version::operator<(const version& rhs) const { return versions < rhs.versions; }
bool version::operator<=(const version& rhs) const { return versions <= rhs.versions; }
bool version::operator>(const version& rhs) const { return versions > rhs.versions; }
bool version::operator>=(const version& rhs) const { return versions >= rhs.versions; }
bool version::operator==(const version& rhs) const { return versions == rhs.versions; }
bool version::operator!=(const version& rhs) const { return versions != rhs.versions; }

std::ostream& operator<<(std::ostream& os, const version& t) {
    os << static_cast<std::string>(t);
    return os;
}

} // namespace sg
