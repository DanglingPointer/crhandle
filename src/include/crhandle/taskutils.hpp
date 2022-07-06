#ifndef TASKUTILS_HPP
#define TASKUTILS_HPP

#include "crhandle/taskhandle.hpp"

#include <array>
#include <optional>
#include <tuple>

namespace cr {

namespace internal {

struct CurrentHandleRetriever
{
   stdcr::coroutine_handle<> handle;

   bool await_ready() const noexcept { return false; }
   bool await_suspend(stdcr::coroutine_handle<> h) noexcept
   {
      handle = h;
      return false;
   }
   stdcr::coroutine_handle<> await_resume() const noexcept { return handle; }
};

template <typename F, typename T, size_t... Is>
auto CreateArray(std::index_sequence<Is...>, T && tuple, F && transform)
{
   return std::array{transform(std::in_place_index<Is>, std::move(std::get<Is>(tuple)))...};
}

template <typename T>
struct TypeConverter
{
   using Type = T;
};

template <>
struct TypeConverter<void>
{
   using Type = std::monostate;
};

template <typename T>
struct NonVoid;

template <template <typename...> typename C, typename... Ts>
struct NonVoid<C<Ts...>>
{
   using Type = C<typename TypeConverter<Ts>::Type...>;
};

} // namespace internal


template <typename T>
using NonVoid = typename internal::NonVoid<T>::Type;

template <Executor E, TaskResult... Rs>
TaskHandle<NonVoid<std::variant<Rs...>>, E> AnyOf(TaskHandle<Rs, E>... ts)
{
   std::optional<NonVoid<std::variant<Rs...>>> ret;
   stdcr::coroutine_handle<> thisHandle = nullptr;

   auto TaskWrapper = [&]<size_t I, typename R>(std::in_place_index_t<I> i,
                                                TaskHandle<R, E> task) -> TaskHandle<void, E> {
      if (ret.has_value())
         co_return;

      if constexpr (std::is_same_v<R, void>) {
         co_await std::move(task);
         ret.emplace(i, internal::TypeConverter<void>::Type{});
      } else {
         R tmp = co_await std::move(task);
         ret.emplace(i, std::move(tmp));
      }
      if (thisHandle)
         thisHandle.resume();
   };

   auto handles = internal::CreateArray(std::make_index_sequence<sizeof...(Rs)>{},
                                        std::forward_as_tuple(std::move(ts)...),
                                        TaskWrapper);

   for (auto & h : handles)
      h.Run();

   if (!ret.has_value()) {
      thisHandle = co_await internal::CurrentHandleRetriever{};
      co_await stdcr::suspend_always{};
   }

   co_return *ret;
}

} // namespace cr

#endif
