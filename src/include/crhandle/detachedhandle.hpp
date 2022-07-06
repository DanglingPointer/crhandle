#ifndef DETACHEDHANDLE_HPP
#define DETACHEDHANDLE_HPP

#include "crhandle/taskhandle.hpp"

namespace cr {

struct DetachedPromise;

struct DetachedHandle
{
   using promise_type = DetachedPromise;
};

struct DetachedPromise
{
   DetachedHandle get_return_object() const noexcept { return {}; }
   stdcr::suspend_never initial_suspend() const noexcept { return {}; }
   stdcr::suspend_never final_suspend() const noexcept { return {}; }
   void unhandled_exception() const { throw; }
   void return_void() noexcept {}
   template <Awaiter A>
   decltype(auto) await_transform(A && a)
   {
      return std::forward<A>(a);
   }
   template <TaskResult T, Executor E>
   auto await_transform(TaskHandle<T, E> && handle)
   {
      return handle.Run();
   }
};

} // namespace cr

#endif
