#pragma once

#include "channel.h"

namespace sg::data {

template <typename T> class view {
    const IChannel<T>& m_ptr;

  public:
    view(const IChannel<T>& src) :m_ptr(src){ }

    std::string name() const noexcept { return m_ptr.name(); }
    size_t count() const noexcept { return m_ptr.size(); }
    const T *data() const noexcept { return m_ptr.const_data(); }
};

} // namespace sg::data
