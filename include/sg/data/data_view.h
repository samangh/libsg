#pragma once

#include "data_source.h"

namespace sg::data {

template <typename T> class data_view {
    const ISource<T> *m_ptr;

  public:
    data_view(ISource<T> *src) { this->m_ptr = src; }

    std::string name() const noexcept { return m_ptr->name(); }
    size_t count() const noexcept { return m_ptr->size(); }
    const T *data() const noexcept { return m_ptr->data(); }
};

} // namespace sg::data
