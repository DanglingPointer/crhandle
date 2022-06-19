#include <gtest/gtest.h>
#include "crhandle/task.hpp"
#include "crhandle/taskutils.hpp"

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

} // namespace
