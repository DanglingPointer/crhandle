#undef NDEBUG
#include <gtest/gtest.h>

#include "crhandle/task.hpp"
#include "crhandle/taskowner.hpp"

#include <optional>

namespace {

struct TaskOwnerFixture : public ::testing::Test
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

TEST_F(TaskOwnerFixture, task_owner_starts_a_task)
{
   struct State
   {
      bool beforeSuspend = false;
      bool afterSuspend = false;
      stdcr::coroutine_handle<> handle = nullptr;
   } state;

   static auto VoidTask = [](State & s) -> cr::TaskHandle<void> {
      s.beforeSuspend = true;
      co_await Awaitable<State>{s};
      s.afterSuspend = true;
   };

   cr::TaskOwner<> owner;
   owner.StartRootTask(VoidTask(state));

   EXPECT_TRUE(state.beforeSuspend);
   EXPECT_FALSE(state.afterSuspend);
   EXPECT_TRUE(state.handle);
   EXPECT_FALSE(state.handle.done());

   state.handle.resume();
   EXPECT_TRUE(state.beforeSuspend);
   EXPECT_TRUE(state.afterSuspend);
   EXPECT_TRUE(state.handle);
   EXPECT_TRUE(state.handle.done());
}

TEST_F(TaskOwnerFixture, task_owner_cancels_tasks_when_dies)
{
   struct State
   {
      bool beforeSuspend = false;
      bool afterSuspend = false;
      stdcr::coroutine_handle<> handle = nullptr;
   } state;

   static auto VoidTask = [](State & s) -> cr::TaskHandle<void> {
      s.beforeSuspend = true;
      co_await Awaitable<State>{s};
      s.afterSuspend = true;
   };

   std::optional<cr::TaskOwner<>> owner;
   owner.emplace();
   owner->StartRootTask(VoidTask(state));

   EXPECT_TRUE(state.beforeSuspend);
   EXPECT_FALSE(state.afterSuspend);
   EXPECT_TRUE(state.handle);
   EXPECT_FALSE(state.handle.done());

   owner.reset();
   state.handle.resume();
   EXPECT_TRUE(state.beforeSuspend);
   EXPECT_FALSE(state.afterSuspend);
   EXPECT_TRUE(state.handle);
}

TEST_F(TaskOwnerFixture, task_owner_starts_a_nested_task)
{
   struct State
   {
      bool beforeSuspend = false;
      bool afterSuspend = false;
      stdcr::coroutine_handle<> handle = nullptr;
   } stateOuter, stateInner;

   cr::TaskOwner<> owner;

   static auto InnerVoidTask = [](State & s) -> cr::TaskHandle<void> {
      s.beforeSuspend = true;
      co_await Awaitable<State>{s};
      s.afterSuspend = true;
   };

   auto OuterVoidTask = [&owner](State & sInner, State & sOuter) -> cr::TaskHandle<void> {
      co_await owner.StartNestedTask(InnerVoidTask(sInner));
      sOuter.beforeSuspend = true;
      co_await Awaitable<State>{sOuter};
      sOuter.afterSuspend = true;
   };

   owner.StartRootTask(OuterVoidTask(stateInner, stateOuter));

   EXPECT_TRUE(stateOuter.beforeSuspend);
   EXPECT_FALSE(stateOuter.afterSuspend);
   EXPECT_TRUE(stateOuter.handle);
   EXPECT_FALSE(stateOuter.handle.done());

   EXPECT_TRUE(stateInner.beforeSuspend);
   EXPECT_FALSE(stateInner.afterSuspend);
   EXPECT_TRUE(stateInner.handle);
   EXPECT_FALSE(stateInner.handle.done());

   stateOuter.handle.resume();

   EXPECT_TRUE(stateOuter.beforeSuspend);
   EXPECT_TRUE(stateOuter.afterSuspend);

   EXPECT_TRUE(stateInner.beforeSuspend);
   EXPECT_FALSE(stateInner.afterSuspend);
   EXPECT_TRUE(stateInner.handle);
   EXPECT_FALSE(stateInner.handle.done());

   stateInner.handle.resume();

   EXPECT_TRUE(stateInner.beforeSuspend);
   EXPECT_TRUE(stateInner.afterSuspend);
}

TEST_F(TaskOwnerFixture, task_owner_cancels_nested_task_when_dies)
{
   struct State
   {
      bool beforeSuspend = false;
      bool afterSuspend = false;
      stdcr::coroutine_handle<> handle = nullptr;
   } stateOuter, stateInner;

   std::optional<cr::TaskOwner<>> owner;
   owner.emplace();

   static auto InnerVoidTask = [](State & s) -> cr::TaskHandle<void> {
      s.beforeSuspend = true;
      co_await Awaitable<State>{s};
      s.afterSuspend = true;
   };

   auto OuterVoidTask = [&owner](State & sInner, State & sOuter) -> cr::TaskHandle<void> {
      co_await owner->StartNestedTask(InnerVoidTask(sInner));
      sOuter.beforeSuspend = true;
      co_await Awaitable<State>{sOuter};
      sOuter.afterSuspend = true;
   };

   owner->StartRootTask(OuterVoidTask(stateInner, stateOuter));

   EXPECT_TRUE(stateOuter.beforeSuspend);
   EXPECT_FALSE(stateOuter.afterSuspend);
   EXPECT_TRUE(stateOuter.handle);
   EXPECT_FALSE(stateOuter.handle.done());

   EXPECT_TRUE(stateInner.beforeSuspend);
   EXPECT_FALSE(stateInner.afterSuspend);
   EXPECT_TRUE(stateInner.handle);
   EXPECT_FALSE(stateInner.handle.done());

   owner.reset();
   stateOuter.handle.resume();

   EXPECT_TRUE(stateOuter.beforeSuspend);
   EXPECT_FALSE(stateOuter.afterSuspend);

   EXPECT_TRUE(stateInner.beforeSuspend);
   EXPECT_FALSE(stateInner.afterSuspend);
   EXPECT_TRUE(stateInner.handle);
   EXPECT_FALSE(stateInner.handle.done());

   stateInner.handle.resume();

   EXPECT_TRUE(stateInner.beforeSuspend);
   EXPECT_FALSE(stateInner.afterSuspend);
}

} // namespace
