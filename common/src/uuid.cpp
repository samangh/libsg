#include "sg/uuid.h"

#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>

#include <cstring>

namespace {

static boost::uuids::uuid random_uuid() {
    static boost::uuids::random_generator generator;
    return generator();
}

} // namespace

namespace sg::uuids {

uuid::uuid() : m_uuid(random_uuid()) {}
uuid::uuid(const uuid& UUID) : m_uuid(UUID.m_uuid) {}
uuid::uuid(std::array<uint8_t, 16> bytes) { std::memcpy(m_uuid.data, bytes.data(), 16); }

void uuid::swap(uuid& rhs) noexcept { m_uuid.swap(rhs.m_uuid); }

std::string uuid::to_sting() const { return boost::uuids::to_string(m_uuid); }

std::array<uint8_t, 16> uuid::data() const {
    std::array<uint8_t, 16> arr;
    std::memcpy(arr.data(), m_uuid.data, 16);
    return arr;
}

} // namespace sg::uuids
