#undef NDEBUG
#include <gtest/gtest.h>

#include "counter.hpp"
#include "crhandle/detachedhandle.hpp"
#include "crhandle/taskhandle.hpp"
#include "crhandle/taskutils.hpp"
#include "dispatcher.hpp"

#include <optional>

namespace {

struct TaskUtilsFixture : public ::testing::Test
{
   template <typename S>
   struct Awaitable
   {
      S & state;
      bool await_ready() { return false; }
      void await_suspend(stdcr::coroutine_handle<> h) { state.handle = h; }
      void await_resume() {}
   };
};

TEST_F(TaskUtilsFixture, handle_retriever_converts_to_void_handle)
{
   stdcr::coroutine_handle<> handle;

   static auto Foo = [](stdcr::coroutine_handle<> & out) -> cr::TaskHandle<void> {
      out = co_await cr::CurrentHandle();
   };
   auto task = Foo(handle);
   task.Run();
   EXPECT_TRUE(handle.done());
}

TEST_F(TaskUtilsFixture, anyof_delivers_first_result_and_ignores_others)
{
   struct State
   {
      stdcr::coroutine_handle<> handle = nullptr;
   } state1, state2;

   static auto IntegerTask = [](State & s) -> cr::TaskHandle<int> {
      co_await Awaitable<State>{s};
      co_return 42;
   };
   static auto StringTask = [](State & s) -> cr::TaskHandle<std::string> {
      co_await Awaitable<State>{s};
      co_return "Hello World";
   };

   std::optional<int> intResult;
   std::optional<std::string> stringResult;

   auto OuterTask = [&]() -> cr::DetachedHandle {
      std::variant<int, std::string> result =
         co_await cr::AnyOf(IntegerTask(state1), StringTask(state2));

      switch (result.index()) {
      case 0:
         intResult.emplace(std::get<0>(result));
         break;
      case 1:
         stringResult.emplace(std::get<1>(result));
         break;
      }
   };

   OuterTask();
   EXPECT_TRUE(state1.handle);
   EXPECT_TRUE(state2.handle);
   EXPECT_FALSE(intResult.has_value());
   EXPECT_FALSE(stringResult.has_value());

   state2.handle.resume();
   EXPECT_FALSE(intResult.has_value());
   EXPECT_TRUE(stringResult.has_value());
   EXPECT_STREQ("Hello World", stringResult.value().c_str());

   state1.handle.resume();
   EXPECT_FALSE(intResult.has_value());
   EXPECT_TRUE(stringResult.has_value());
   EXPECT_STREQ("Hello World", stringResult.value().c_str());
}

TEST_F(TaskUtilsFixture, anyof_handles_void_tasks)
{
   struct State
   {
      stdcr::coroutine_handle<> handle = nullptr;
   } state1, state2;

   static auto VoidTask = [](State & s) -> cr::TaskHandle<void> {
      co_await Awaitable<State>{s};
   };

   size_t index = std::numeric_limits<size_t>::max();

   auto OuterTask = [&]() -> cr::DetachedHandle {
      auto result = co_await cr::AnyOf(VoidTask(state1), VoidTask(state2));
      index = result.index();
   };

   OuterTask();
   EXPECT_TRUE(state1.handle);
   EXPECT_TRUE(state2.handle);
   EXPECT_EQ(std::numeric_limits<size_t>::max(), index);

   state1.handle.resume();
   EXPECT_EQ(0u, index);

   state2.handle.resume();
   EXPECT_EQ(0u, index);
}

TEST_F(TaskUtilsFixture, anyof_handles_immediate_task_and_short_circuits)
{
   struct State
   {
      stdcr::coroutine_handle<> handle = nullptr;
   } state1, state2;

   static auto IntegerTask = []() -> cr::TaskHandle<int> {
      co_return 42;
   };
   static auto StringTask = [](State & s) -> cr::TaskHandle<std::string> {
      co_await Awaitable<State>{s};
      co_return "Hello World";
   };

   std::optional<std::string> stringResult1;
   std::optional<int> intResult;
   std::optional<std::string> stringResult2;

   auto OuterTask = [&]() -> cr::DetachedHandle {
      std::variant<std::string, int, std::string> result =
         co_await cr::AnyOf(StringTask(state1), IntegerTask(), StringTask(state2));

      switch (result.index()) {
      case 0:
         stringResult1.emplace(std::get<0>(result));
         break;
      case 1:
         intResult.emplace(std::get<1>(result));
         break;
      case 2:
         stringResult2.emplace(std::get<2>(result));
         break;
      }
   };

   OuterTask();
   EXPECT_TRUE(state1.handle);
   EXPECT_FALSE(state2.handle);

   EXPECT_FALSE(stringResult1.has_value());
   EXPECT_FALSE(stringResult2.has_value());
   EXPECT_TRUE(intResult.has_value());
   EXPECT_EQ(42, intResult.value());

   state1.handle.resume();
   EXPECT_FALSE(state2.handle);
   EXPECT_FALSE(stringResult1.has_value());
   EXPECT_FALSE(stringResult2.has_value());
}

TEST_F(TaskUtilsFixture, anyof_uses_provided_executor_instance)
{
   ManualDispatcher dispatcher;

   struct State
   {
      stdcr::coroutine_handle<> handle = nullptr;
   } state1, state2;
   size_t index = std::numeric_limits<size_t>::max();

   static auto VoidTask = [](State & s) -> cr::TaskHandle<void, ManualDispatcher::Executor> {
      co_await Awaitable<State>{s};
   };
   static auto OuterTask = [](State & state1,
                              State & state2,
                              size_t & index) -> cr::TaskHandle<void, ManualDispatcher::Executor> {
      auto result = co_await cr::AnyOf(VoidTask(state1), VoidTask(state2));
      index = result.index();
   };

   auto handle = OuterTask(state1, state2, index);
   handle.Run(ManualDispatcher::Executor{&dispatcher});
   EXPECT_FALSE(state1.handle);
   EXPECT_FALSE(state2.handle);

   dispatcher.ProcessAll();
   EXPECT_TRUE(state1.handle);
   EXPECT_TRUE(state2.handle);
   EXPECT_EQ(std::numeric_limits<size_t>::max(), index);

   state2.handle.resume();
   EXPECT_EQ(std::numeric_limits<size_t>::max(), index);

   dispatcher.ProcessAll();
   EXPECT_EQ(1u, index);

   state1.handle.resume();
   dispatcher.ProcessAll();
   EXPECT_EQ(1u, index);
}

TEST_F(TaskUtilsFixture, anyof_cancelation_doesnt_leak_memory)
{
   ManualDispatcher dispatcher;

   struct State
   {
      stdcr::coroutine_handle<> handle = nullptr;
      bool done = false;
   } state1, state2;

   int count = 0;

   size_t index = std::numeric_limits<size_t>::max();

   static auto VoidTask = [](State & s,
                             Counter counter) -> cr::TaskHandle<void, ManualDispatcher::Executor> {
      (void)counter;
      co_await Awaitable<State>{s};
      s.done = true;
   };
   static auto OuterTask = [](State & state1,
                              State & state2,
                              Counter counter,
                              size_t & index) -> cr::TaskHandle<void, ManualDispatcher::Executor> {
      auto result = co_await cr::AnyOf(VoidTask(state1, counter), VoidTask(state2, counter));
      index = result.index();
   };

   auto handle = OuterTask(state1, state2, Counter(count), index);
   handle.Run(ManualDispatcher::Executor{&dispatcher});
   dispatcher.ProcessAll();
   EXPECT_TRUE(state1.handle);
   EXPECT_TRUE(state2.handle);
   EXPECT_EQ(std::numeric_limits<size_t>::max(), index);
   EXPECT_LE(3, count);

   handle = {};
   state1.handle.resume();
   state2.handle.resume();
   dispatcher.ProcessAll();
   EXPECT_TRUE(state1.done);
   EXPECT_TRUE(state2.done);
   EXPECT_EQ(std::numeric_limits<size_t>::max(), index);
   EXPECT_EQ(0, count);
}

TEST_F(TaskUtilsFixture, allof_delivers_all_results)
{
   struct State
   {
      stdcr::coroutine_handle<> handle = nullptr;
   } state1, state2;

   static auto IntegerTask = [](State & s) -> cr::TaskHandle<int> {
      co_await Awaitable<State>{s};
      co_return 42;
   };
   static auto StringTask = [](State & s) -> cr::TaskHandle<std::string> {
      co_await Awaitable<State>{s};
      co_return "Hello World";
   };
   static auto ImmediateTask = []() -> cr::TaskHandle<double> {
      co_return 3.14;
   };

   std::optional<std::tuple<int, std::string, double>> result;

   auto OuterTask = [&]() -> cr::DetachedHandle {
      result.emplace(co_await cr::AllOf(IntegerTask(state1), StringTask(state2), ImmediateTask()));
   };

   OuterTask();
   EXPECT_TRUE(state1.handle);
   EXPECT_TRUE(state2.handle);
   EXPECT_FALSE(result);

   state2.handle.resume();
   EXPECT_FALSE(result);

   state1.handle.resume();
   EXPECT_TRUE(result);
   EXPECT_STREQ("Hello World", std::get<std::string>(*result).c_str());
   EXPECT_EQ(42, std::get<int>(*result));
   EXPECT_EQ(3.14, std::get<double>(*result));
}

TEST_F(TaskUtilsFixture, allof_handles_void_tasks)
{
   struct State
   {
      stdcr::coroutine_handle<> handle = nullptr;
      bool done = false;
   } state1, state2;

   std::optional<std::tuple<std::monostate, std::monostate>> result;

   static auto VoidTask = [](State & s) -> cr::TaskHandle<void> {
      co_await Awaitable<State>{s};
      s.done = true;
   };

   auto OuterTask = [&]() -> cr::DetachedHandle {
      auto ret = co_await cr::AllOf(VoidTask(state1), VoidTask(state2));
      result.emplace(std::move(ret));
   };

   OuterTask();
   EXPECT_TRUE(state1.handle);
   EXPECT_TRUE(state2.handle);
   EXPECT_FALSE(state1.done);
   EXPECT_FALSE(state2.done);
   EXPECT_FALSE(result);

   state1.handle.resume();
   EXPECT_TRUE(state1.done);
   EXPECT_FALSE(state2.done);
   EXPECT_FALSE(result);

   state2.handle.resume();
   EXPECT_TRUE(state1.done);
   EXPECT_TRUE(state2.done);
   EXPECT_TRUE(result);
}

TEST_F(TaskUtilsFixture, allof_handles_immediate_tasks)
{
   static auto IntegerTask = []() -> cr::TaskHandle<int> {
      co_return 42;
   };
   static auto StringTask = []() -> cr::TaskHandle<std::string> {
      co_return "Hello World";
   };

   std::optional<std::tuple<std::string, int, std::string>> result;

   auto OuterTask = [&]() -> cr::DetachedHandle {
      auto ret = co_await cr::AllOf(StringTask(), IntegerTask(), StringTask());
      result.emplace(std::move(ret));
   };

   OuterTask();
   EXPECT_TRUE(result);
   EXPECT_STREQ("Hello World", std::get<0>(*result).c_str());
   EXPECT_EQ(42, std::get<1>(*result));
   EXPECT_STREQ("Hello World", std::get<2>(*result).c_str());
}

TEST_F(TaskUtilsFixture, allof_uses_provided_executor_instance)
{
   ManualDispatcher dispatcher;

   struct State
   {
      stdcr::coroutine_handle<> handle = nullptr;
      bool done = false;
   } state1, state2;

   std::optional<std::tuple<std::monostate, std::monostate>> result;

   static auto VoidTask = [](State & s) -> cr::TaskHandle<void, ManualDispatcher::Executor> {
      co_await Awaitable<State>{s};
      s.done = true;
   };
   static auto OuterTask = [](State & state1,
                              State & state2,
                              auto & result) -> cr::TaskHandle<void, ManualDispatcher::Executor> {
      auto ret = co_await cr::AllOf(VoidTask(state1), VoidTask(state2));
      result.emplace(std::move(ret));
   };

   auto handle = OuterTask(state1, state2, result);
   handle.Run(ManualDispatcher::Executor{&dispatcher});
   EXPECT_FALSE(state1.handle);
   EXPECT_FALSE(state2.handle);

   dispatcher.ProcessAll();
   EXPECT_TRUE(state1.handle);
   EXPECT_TRUE(state2.handle);
   EXPECT_FALSE(state1.done);
   EXPECT_FALSE(state2.done);
   EXPECT_FALSE(result);

   state2.handle.resume();
   dispatcher.ProcessAll();
   EXPECT_FALSE(state1.done);
   EXPECT_TRUE(state2.done);
   EXPECT_FALSE(result);

   state1.handle.resume();
   dispatcher.ProcessAll();
   EXPECT_TRUE(state1.done);
   EXPECT_TRUE(state2.done);
   EXPECT_TRUE(result);
}

TEST_F(TaskUtilsFixture, allof_cancelation_doesnt_leak_memory)
{
   ManualDispatcher dispatcher;

   struct State
   {
      stdcr::coroutine_handle<> handle = nullptr;
      bool done = false;
   } state1, state2;

   int count = 0;

   std::optional<std::tuple<std::monostate, std::monostate>> result;

   static auto VoidTask = [](State & s,
                             Counter counter) -> cr::TaskHandle<void, ManualDispatcher::Executor> {
      (void)counter;
      co_await Awaitable<State>{s};
      s.done = true;
   };
   static auto OuterTask = [](State & state1,
                              State & state2,
                              Counter counter,
                              auto & result) -> cr::TaskHandle<void, ManualDispatcher::Executor> {
      auto ret = co_await cr::AllOf(VoidTask(state1, counter), VoidTask(state2, counter));
      result.emplace(std::move(ret));
   };

   auto handle = OuterTask(state1, state2, Counter(count), result);
   handle.Run(ManualDispatcher::Executor{&dispatcher});
   dispatcher.ProcessAll();
   EXPECT_TRUE(state1.handle);
   EXPECT_TRUE(state2.handle);
   EXPECT_FALSE(result);
   EXPECT_LE(3, count);

   handle = {};
   state1.handle.resume();
   state2.handle.resume();
   dispatcher.ProcessAll();
   EXPECT_TRUE(state1.done);
   EXPECT_TRUE(state2.done);
   EXPECT_FALSE(result);
   EXPECT_EQ(0, count);
}

} // namespace
