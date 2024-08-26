//#pragma once

//#include "memory.h"

//#include <functional>
//#include <memory>

//namespace sg {

///**
// * @brief A std::funnction tyype object, where the function will only be run if the
// * provided weak_ptr is valid.
// *
// * This is handly when passing a std::fucntion as a callback, and we want the
// * callback to not be called if the owning object is out of scope.
// *
// * @tparam PtrT     type of oject held by the weakptr
// * @tparam ReturnT  return type of the function
// * @tparam ArgsT    types of function arguments
// */
//template <typename PtrT, typename ReturnT, typename... ArgsT>
//class weak_function {
//   public:
//    typedef std::weak_ptr<PtrT> ptr_type;
//    typedef std::function<ReturnT(ArgsT...)> func_type;

//    weak_function(const std::nullptr_t &){};  // Allow cast from nullptr, i.e. a weak_function that
//                                              // does nothing
//    weak_function(ptr_type weakPtr, func_type func) : m_item(weakPtr), m_func(std::move(func)){};

//    template <typename... U>
//    ReturnT operator()(U &&...args) {
//        if (m_item.lock() && m_func)
//            return m_func(std::forward<ArgsT>(args)...);
//        return ReturnT();
//    };
//    virtual ~weak_function() = default;

//   private:
//    ptr_type m_item;
//    func_type m_func;
//};

//template <typename R, typename... ArgTypes>
//auto create_weak_function(sg::enable_lifetime_indicator *lifeimeIndicator,
//                          std::function<R(ArgTypes...)> func) {
//    return sg::weak_function(lifeimeIndicator->get_lifetime_indicator(), std::move(func));
//}
//}  // namespace sg
