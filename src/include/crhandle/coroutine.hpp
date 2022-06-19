#ifndef COROUTINE_HPP
#define COROUTINE_HPP

#include <version>

#ifdef _LIBCPP_VERSION

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
