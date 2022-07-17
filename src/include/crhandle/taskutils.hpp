#ifndef TASKUTILS_HPP
#define TASKUTILS_HPP

#include "crhandle/taskhandle.hpp"

#include <array>
#include <optional>
#include <tuple>
#include <variant>

namespace cr {

namespace internal {

template <typename P = void>
struct CurrentHandleRetriever
{
   stdcr::coroutine_handle<P> handle;

   bool await_ready() const noexcept { return false; }
   template <typename T = P>
   bool await_suspend(stdcr::coroutine_handle<T> h) noexcept
   {
      static_assert(std::is_same_v<T, P> || std::is_same_v<void, P>);
      handle = h;
      return false;
   }
   stdcr::coroutine_handle<P> await_resume() const noexcept { return handle; }
};

template <typename F, typename T, size_t... Is>
auto TupleToArray(std::index_sequence<Is...>, T && tuple, F && transform)
{
   return std::array{transform(std::in_place_index<Is>, std::move(std::get<Is>(tuple)))...};
}

template <typename... Ts>
std::tuple<Ts...> ToNonOptional(std::tuple<std::optional<Ts>...> && t)
{
   return std::apply(
      [](auto &&... opt) {
         assert((... && opt.has_value()));
         return std::tuple{*std::move(opt)...};
      },
      std::move(t));
}

template <typename... Ts>
bool AllValuesSet(const std::tuple<std::optional<Ts>...> & t) noexcept
{
   return std::apply(
      [](const auto &... opt) {
         return (... && opt.has_value());
      },
      t);
}

template <typename T>
struct NonVoid
{
   using Type = T;
};

template <>
struct NonVoid<void>
{
   using Type = std::monostate;
};

} // namespace internal

template <typename P = void>
cr::Awaiter auto CurrentHandle() noexcept
{
   return internal::CurrentHandleRetriever<P>{};
}

template <typename T>
using NonVoid = typename internal::NonVoid<T>::Type;

struct AnyOfFn
{
   template <Executor E, TaskResult... Rs>
   using HandleType = TaskHandle<std::variant<NonVoid<Rs>...>, E>;

   template <Executor E, TaskResult... Rs>
   HandleType<E, Rs...> operator()(TaskHandle<Rs, E>... ts) const
   {
      std::optional<std::variant<NonVoid<Rs>...>> ret;
      stdcr::coroutine_handle<> continuation = nullptr;

      auto TaskWrapper = [&]<size_t I, typename R>(std::in_place_index_t<I> i,
                                                   TaskHandle<R, E> task) -> TaskHandle<void, E> {
         if (ret.has_value())
            co_return;

         if constexpr (std::is_same_v<R, void>) {
            co_await std::move(task);
            ret.emplace(i, NonVoid<void>{});
         } else {
            R tmp = co_await std::move(task);
            ret.emplace(i, std::move(tmp));
         }
         if (continuation)
            continuation.resume();
      };

      auto tasks = internal::TupleToArray(std::make_index_sequence<sizeof...(Rs)>{},
                                          std::forward_as_tuple(std::move(ts)...),
                                          TaskWrapper);

      auto thisHandle = co_await CurrentHandle<typename HandleType<E, Rs...>::promise_type>();
      const auto & thisPromise = thisHandle.promise();

      for (auto & h : tasks)
         h.Run(thisPromise.Executor());

      if (!ret.has_value()) {
         continuation = thisHandle;
         co_await stdcr::suspend_always{};
      }

      co_return *ret;
   }
};

inline constexpr AnyOfFn AnyOf;

struct AllOfFn
{
   template <Executor E, TaskResult... Rs>
   using HandleType = TaskHandle<std::tuple<NonVoid<Rs>...>, E>;

   template <Executor E, TaskResult... Rs>
   HandleType<E, Rs...> operator()(TaskHandle<Rs, E>... ts) const
   {
      std::tuple<std::optional<NonVoid<Rs>>...> ret;
      stdcr::coroutine_handle<> continuation = nullptr;

      auto TaskWrapper = [&]<size_t I, typename R>(std::in_place_index_t<I>,
                                                   TaskHandle<R, E> task) -> TaskHandle<void, E> {
         if constexpr (std::is_same_v<R, void>) {
            co_await std::move(task);
            std::get<I>(ret).emplace(NonVoid<void>{});
         } else {
            R tmp = co_await std::move(task);
            std::get<I>(ret).emplace(std::move(tmp));
         }
         if (continuation && internal::AllValuesSet(ret))
            continuation.resume();
      };

      auto tasks = internal::TupleToArray(std::make_index_sequence<sizeof...(Rs)>{},
                                          std::forward_as_tuple(std::move(ts)...),
                                          TaskWrapper);

      auto thisHandle = co_await CurrentHandle<typename HandleType<E, Rs...>::promise_type>();
      const auto & thisPromise = thisHandle.promise();

      for (auto & h : tasks)
         h.Run(thisPromise.Executor());

      if (!internal::AllValuesSet(ret)) {
         continuation = thisHandle;
         co_await stdcr::suspend_always{};
      }

      co_return internal::ToNonOptional(std::move(ret));
   }
};

inline constexpr AllOfFn AllOf;

} // namespace cr

#endif
