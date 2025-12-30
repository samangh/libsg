#include "sg/version.h"
#include "fmt/ranges.h"

namespace {
unsigned int convert_or_throw(const std::string& input) {
    size_t charactersConverted;
    const auto result = std::stoul(input, &charactersConverted);

    if (charactersConverted!=input.length())
        throw std::runtime_error("could not convert input string to a version");

    return result;
}

}

namespace sg {

version::version() = default;
version::version(std::initializer_list<unsigned int> input) : versions(input) {}
version::operator std::string() const { return fmt::format("{}", fmt::join(versions, ".")); }

version::version(std::string input) {
    const std::string delimiter = ".";

    for (auto pos = input.find(delimiter); pos != std::string::npos; pos = input.find(delimiter)) {
        auto extract = input.substr(0, pos);
        input.erase(0, pos + delimiter.length());

        versions.push_back(convert_or_throw(extract));
    }

    // Stuff after last .
    if (!input.empty())
        versions.push_back(convert_or_throw(input));
}

std::ostream& operator<<(std::ostream& os, const version& t) {
    os << static_cast<std::string>(t);
    return os;
}

} // namespace sg
