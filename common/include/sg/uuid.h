#include <sg/export/common.h>

#include <boost/uuid/uuid.hpp>

#include <array>
#include <cstring>
#include <string>

namespace sg::uuids {

//typedef boost::uuids::uuid uuid;

class SG_COMMON_EXPORT uuid {
    boost::uuids::uuid m_uuid;
  public:
    uuid();
    uuid(const uuid& UUID);;
    explicit uuid(std::array<uint8_t, 16> bytes);;

    bool operator==(uuid const& rhs) const noexcept = default;
    std::strong_ordering operator<=>(uuid const& rhs) const noexcept = default;

    friend SG_COMMON_EXPORT void swap(uuid& rhs, uuid& lhs) noexcept;

    [[nodiscard]] std::string to_string() const;
    [[nodiscard]] std::array<uint8_t, 16> data() const;
};

SG_COMMON_EXPORT void swap(uuid& rhs, uuid& lhs) noexcept;
SG_COMMON_EXPORT std::ostream &operator<<(std::ostream &os, const uuid &t);

} // namespace sg::uuids
