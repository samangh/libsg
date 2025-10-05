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
    bool operator!=(uuid const& rhs) const noexcept = default;
    bool operator<(uuid const& rhs) const noexcept = default;
    bool operator>(uuid const& rhs) const noexcept = default;
    bool operator<=(uuid const& rhs) const noexcept = default;
    bool operator>=(uuid const& rhs) const noexcept = default;
    std::strong_ordering operator<=>(uuid const& rhs) const noexcept = default;

    void swap(uuid& rhs ) noexcept;

    [[nodiscard]] std::string to_sting() const;
    [[nodiscard]] std::array<uint8_t, 16> data() const;
};



}
