#pragma once

#include <sg/buffer.h>
#include <string>
#include <map>

namespace sg::data {

class ISourceBase {
public:
    virtual std::string name() const noexcept  =0;
    virtual void name(const std::string&) noexcept  =0;
    virtual size_t size() const noexcept =0;
};

template <typename T>
class ISource : public ISourceBase {
public:
    virtual T* data() const noexcept = 0;
};

}
