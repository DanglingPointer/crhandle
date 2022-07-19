#ifndef COROUTINE_HPP
#define COROUTINE_HPP

#include <version>

#if defined(_LIBCPP_VERSION) && _LIBCPP_VERSION < 13000

#include <experimental/coroutine>

namespace stdcr {
using std::experimental::coroutine_handle;
using std::experimental::suspend_always;
using std::experimental::suspend_never;
} // namespace stdcr

#else

#include <coroutine>

namespace stdcr {
using std::coroutine_handle;
using std::suspend_always;
using std::suspend_never;
} // namespace stdcr

#endif

#endif
