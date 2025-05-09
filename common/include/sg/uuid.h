#include <array>
#include <string>

#include <boost/uuid/uuid.hpp>

namespace sg::uuids {

//typedef boost::uuids::uuid uuid;

class uuid {
    boost::uuids::uuid m_uuid;
  public:
    uuid();
    uuid(const uuid& UUID);;
    uuid(std::array<uint8_t, 16> bytes);;

    bool operator==(uuid const& rhs) const noexcept = default;
    bool operator!=(uuid const& rhs) const noexcept = default;
    bool operator<(uuid const& rhs) const noexcept = default;
    bool operator>(uuid const& rhs) const noexcept = default;
    bool operator<=(uuid const& rhs) const noexcept = default;
    bool operator>=(uuid const& rhs) const noexcept = default;
    std::strong_ordering operator<=>(uuid const& rhs) const noexcept = default;

    void swap(uuid& rhs ) noexcept;

    std::string to_sting() const;
    std::array<uint8_t, 16> data() const {
        std::array<uint8_t, 16> arr;
        std::memcpy(arr.data(), m_uuid.data, 16);
        return arr;
    }
};



}
