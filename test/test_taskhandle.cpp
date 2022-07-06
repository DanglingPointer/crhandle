#undef NDEBUG
#include <gtest/gtest.h>

#include "crhandle/detachedhandle.hpp"
#include "crhandle/taskhandle.hpp"

#include <deque>
#include <optional>

namespace {

struct Counter
{
   int & count;

   Counter(int & counter)
      : count(counter)
   {
      ++count;
   }
   Counter(const Counter & other)
      : count(other.count)
   {
      ++count;
   }
   ~Counter() { --count; }
};

struct DetachedTaskFixture : public ::testing::Test
{
   struct State
   {
      bool beforeSuspend = false;
      bool afterSuspend = false;
      stdcr::coroutine_handle<> handle = nullptr;
      int count = 0;
   };

   struct Awaitable
   {
      State & state;
      bool await_ready() { return false; }
      void await_suspend(stdcr::coroutine_handle<> h) { state.handle = h; }
      void await_resume() {}
   };

   cr::DetachedHandle StartDetachedOperation(State & state)
   {
      Counter c(state.count);
      state.beforeSuspend = true;
      co_await Awaitable{state};
      state.afterSuspend = true;
   }
};

TEST_F(DetachedTaskFixture, detached_task_runs_eagerly)
{
   State state;

   [[maybe_unused]] auto handle = StartDetachedOperation(state);

   EXPECT_TRUE(state.beforeSuspend);
   EXPECT_FALSE(state.afterSuspend);
   EXPECT_TRUE(state.handle);
   EXPECT_EQ(1, state.count);

   state.handle.resume();
   EXPECT_TRUE(state.afterSuspend);
   EXPECT_EQ(0, state.count);
}

TEST_F(DetachedTaskFixture, detached_task_runs_without_handle)
{
   State state;

   StartDetachedOperation(state);

   EXPECT_TRUE(state.beforeSuspend);
   EXPECT_FALSE(state.afterSuspend);
   EXPECT_TRUE(state.handle);
   EXPECT_EQ(1, state.count);

   state.handle.resume();
   EXPECT_TRUE(state.afterSuspend);
   EXPECT_EQ(0, state.count);
}

struct TaskHandleFixture : public ::testing::Test
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

TEST_F(TaskHandleFixture, task_runs_if_handle_is_alive)
{
   struct State
   {
      bool beforeSuspend = false;
      bool afterSuspend = false;
      stdcr::coroutine_handle<> handle = nullptr;
      int count = 0;
   } state;

   auto StartVoidOperation = [](State & s) -> cr::TaskHandle<void> {
      Counter c(s.count);
      s.beforeSuspend = true;
      co_await Awaitable<State>{s};
      s.afterSuspend = true;
   };

   cr::TaskHandle<void> task = StartVoidOperation(state);
   EXPECT_TRUE(task);
   EXPECT_FALSE(state.beforeSuspend);
   EXPECT_FALSE(state.afterSuspend);
   EXPECT_FALSE(state.handle);
   EXPECT_EQ(0, state.count);

   task.Run();
   EXPECT_TRUE(task);
   EXPECT_TRUE(state.beforeSuspend);
   EXPECT_FALSE(state.afterSuspend);
   EXPECT_TRUE(state.handle);
   EXPECT_EQ(1, state.count);

   state.handle.resume();
   EXPECT_FALSE(task);
   EXPECT_TRUE(state.afterSuspend);
   EXPECT_EQ(0, state.count);
}

TEST_F(TaskHandleFixture, task_is_canceled_when_handle_dies)
{
   struct State
   {
      bool beforeSuspend = false;
      bool afterSuspend = false;
      stdcr::coroutine_handle<> handle = nullptr;
      int count = 0;
   } state;

   auto StartVoidOperation = [](State & s) -> cr::TaskHandle<void> {
      Counter c(s.count);
      s.beforeSuspend = true;
      co_await Awaitable<State>{s};
      s.afterSuspend = true;
   };

   cr::TaskHandle<void> task = StartVoidOperation(state);
   task.Run();

   task = {};
   EXPECT_FALSE(task);
   EXPECT_TRUE(state.beforeSuspend);
   EXPECT_FALSE(state.afterSuspend);
   EXPECT_TRUE(state.handle);
   EXPECT_EQ(1, state.count);

   state.handle.resume();
   EXPECT_FALSE(state.afterSuspend);
   EXPECT_EQ(0, state.count);
}

TEST_F(TaskHandleFixture, task_resumes_outer_task)
{
   struct State
   {
      bool beforeInnerSuspend = false;
      bool afterInnerSuspend = false;
      bool beforeOuterSuspend = false;
      bool afterOuterSuspend = false;
      stdcr::coroutine_handle<> handle = nullptr;
      int count = 0;
   } state;

   auto StartInnerVoidOperation = [](State & s) -> cr::TaskHandle<void> {
      Counter c(s.count);
      s.beforeInnerSuspend = true;
      co_await Awaitable<State>{s};
      s.afterInnerSuspend = true;
   };

   auto StartOuterVoidOperation = [=](State & s) -> cr::TaskHandle<void> {
      Counter c(s.count);
      s.beforeOuterSuspend = true;
      co_await StartInnerVoidOperation(s);
      s.afterOuterSuspend = true;
   };

   auto task = StartOuterVoidOperation(state);
   task.Run();
   EXPECT_TRUE(state.beforeOuterSuspend);
   EXPECT_TRUE(state.beforeInnerSuspend);
   EXPECT_FALSE(state.afterOuterSuspend);
   EXPECT_FALSE(state.afterInnerSuspend);
   EXPECT_TRUE(state.handle);
   EXPECT_EQ(2, state.count);

   state.handle.resume();
   EXPECT_TRUE(state.afterOuterSuspend);
   EXPECT_TRUE(state.afterInnerSuspend);
   EXPECT_EQ(0, state.count);
}

TEST_F(TaskHandleFixture, canceled_tasks_dont_run_once_resumed)
{
   struct State
   {
      bool beforeInnerSuspend = false;
      bool afterInnerSuspend = false;
      bool beforeOuterSuspend = false;
      bool afterOuterSuspend = false;
      stdcr::coroutine_handle<> handle = nullptr;
      int count = 0;
   } state;

   auto StartInnerVoidOperation = [](State & s) -> cr::TaskHandle<void> {
      Counter c(s.count);
      s.beforeInnerSuspend = true;
      co_await Awaitable<State>{s};
      s.afterInnerSuspend = true;
   };

   auto StartOuterVoidOperation = [=](State & s) -> cr::TaskHandle<void> {
      Counter c(s.count);
      s.beforeOuterSuspend = true;
      co_await StartInnerVoidOperation(s);
      s.afterOuterSuspend = true;
   };

   auto task = StartOuterVoidOperation(state);
   task.Run();

   task = {};
   state.handle.resume();
   EXPECT_FALSE(state.afterOuterSuspend);
   EXPECT_FALSE(state.afterInnerSuspend);
   EXPECT_EQ(0, state.count);
}

TEST_F(TaskHandleFixture, task_returns_value_to_outer_task)
{
   struct State
   {
      stdcr::coroutine_handle<> handle = nullptr;
      std::string value;
      int count = 0;
   } state;

   auto StartInnerStringOperation = [](State & s) -> cr::TaskHandle<std::string> {
      Counter c(s.count);
      co_await Awaitable<State>{s};
      co_return "Hello World!";
   };

   auto StartOuterVoidOperation = [=](State & s) -> cr::TaskHandle<void> {
      Counter c(s.count);
      std::string result = co_await StartInnerStringOperation(s);
      s.value = std::move(result);
   };

   auto task = StartOuterVoidOperation(state);
   task.Run();
   EXPECT_TRUE(state.value.empty());
   EXPECT_TRUE(state.handle);
   EXPECT_EQ(2, state.count);

   state.handle.resume();
   EXPECT_STREQ("Hello World!", state.value.c_str());
   EXPECT_EQ(0, state.count);
}

TEST_F(TaskHandleFixture, canceled_task_doesnt_receive_value_from_inner_task)
{
   struct State
   {
      stdcr::coroutine_handle<> handle = nullptr;
      std::string value;
      int count = 0;
   } state;

   auto StartInnerStringOperation = [](State & s) -> cr::TaskHandle<std::string> {
      Counter c(s.count);
      co_await Awaitable<State>{s};
      co_return "Hello World!";
   };

   auto StartOuterVoidOperation = [=](State & s) -> cr::TaskHandle<void> {
      Counter c(s.count);
      std::string result = co_await StartInnerStringOperation(s);
      s.value = std::move(result);
   };

   auto task = StartOuterVoidOperation(state);
   task.Run();

   task = {};
   state.handle.resume();
   EXPECT_TRUE(state.value.empty());
   EXPECT_EQ(0, state.count);
}

TEST_F(TaskHandleFixture, three_nested_tasks_resume_each_other)
{
   struct State
   {
      stdcr::coroutine_handle<> handle = nullptr;
      std::string middleValue;
      int innerValue = 0;
   } state;

   auto StartInnerIntOperation = [](State & s) -> cr::TaskHandle<int> {
      co_await Awaitable<State>{s};
      co_return 42;
   };

   auto StartMiddleStringOperation = [=](State & s) -> cr::TaskHandle<std::string> {
      int result = co_await StartInnerIntOperation(s);
      s.innerValue = result;
      co_return std::to_string(result);
   };

   auto StartOuterVoidOperation = [=](State & s) -> cr::TaskHandle<void> {
      std::string result = co_await StartMiddleStringOperation(s);
      s.middleValue = result;
   };

   auto task = StartOuterVoidOperation(state);
   task.Run();
   EXPECT_TRUE(state.handle);
   EXPECT_EQ(0, state.innerValue);
   EXPECT_TRUE(state.middleValue.empty());

   state.handle.resume();
   EXPECT_EQ(42, state.innerValue);
   EXPECT_STREQ("42", state.middleValue.c_str());
}

TEST_F(TaskHandleFixture, three_nested_tasks_cancel_each_other)
{
   struct State
   {
      stdcr::coroutine_handle<> handle = nullptr;
      std::string middleResult;
      int innerResult = 0;
      const char * innerIntermediate = nullptr;
   } state;

   auto StartInnerIntOperation = [](State & s) -> cr::TaskHandle<int> {
      co_await Awaitable<State>{s};
      s.innerIntermediate = "Hello World";
      co_return 42;
   };

   auto StartMiddleStringOperation = [=](State & s) -> cr::TaskHandle<std::string> {
      int result = co_await StartInnerIntOperation(s);
      s.innerResult = result;
      co_return std::to_string(result);
   };

   auto StartOuterVoidOperation = [=](State & s) -> cr::TaskHandle<void> {
      std::string result = co_await StartMiddleStringOperation(s);
      s.middleResult = result;
   };

   auto task = StartOuterVoidOperation(state);
   task.Run();
   EXPECT_TRUE(state.handle);
   EXPECT_EQ(0, state.innerResult);
   EXPECT_EQ(nullptr, state.innerIntermediate);
   EXPECT_TRUE(state.middleResult.empty());

   task = {};
   state.handle.resume();
   EXPECT_EQ(0, state.innerResult);
   EXPECT_EQ(nullptr, state.innerIntermediate);
   EXPECT_TRUE(state.middleResult.empty());
}

struct ManualDispatcher
{
   static ManualDispatcher s_instance;

   using Task = std::function<void()>;

   struct Executor
   {
      ManualDispatcher * master = &s_instance;
      void Execute(Task && task)
      {
         assert(master);
         master->queue.push_back(std::move(task));
      }
   };

   bool ProcessOneTask()
   {
      if (queue.empty())
         return false;
      Task t = std::move(queue.front());
      queue.pop_front();
      t();
      return true;
   }

   std::deque<Task> queue;
};

ManualDispatcher ManualDispatcher::s_instance;

TEST_F(TaskHandleFixture, task_uses_provided_executor_and_passes_it_to_inner_task)
{
   using TaskType = cr::TaskHandle<void, ManualDispatcher::Executor>;

   struct State
   {
      bool beforeInnerSuspend = false;
      bool afterInnerSuspend = false;
      bool beforeOuterSuspend = false;
      bool afterOuterSuspend = false;
      stdcr::coroutine_handle<> handle = nullptr;
      int count = 0;
   } state;

   Counter counter{state.count};
   ManualDispatcher dispatcher;

   static auto StartInnerVoidOperation = [](State & s, Counter c) -> TaskType {
      Counter copy(c);
      s.beforeInnerSuspend = true;
      co_await Awaitable<State>{s};
      s.afterInnerSuspend = true;
   };

   static auto StartOuterVoidOperation = [](State & s, Counter c) -> TaskType {
      Counter copy(c);
      s.beforeOuterSuspend = true;
      co_await StartInnerVoidOperation(s, c);
      s.afterOuterSuspend = true;
   };

   auto task = StartOuterVoidOperation(state, counter);

   // co-awaiting on initial suspend
   EXPECT_FALSE(state.beforeOuterSuspend);
   EXPECT_FALSE(state.beforeInnerSuspend);
   EXPECT_FALSE(state.afterOuterSuspend);
   EXPECT_FALSE(state.afterInnerSuspend);
   EXPECT_FALSE(state.handle);
   EXPECT_EQ(2, state.count);
   EXPECT_EQ(0, dispatcher.queue.size());

   // task schedules itself on executor
   task.Run(ManualDispatcher::Executor{&dispatcher});
   EXPECT_FALSE(state.beforeOuterSuspend);
   EXPECT_FALSE(state.beforeInnerSuspend);
   EXPECT_FALSE(state.afterOuterSuspend);
   EXPECT_FALSE(state.afterInnerSuspend);
   EXPECT_FALSE(state.handle);
   EXPECT_EQ(2, state.count);
   EXPECT_EQ(1, dispatcher.queue.size());

   // co-awaiting on initial suspend of the inner coro
   dispatcher.ProcessOneTask();
   EXPECT_TRUE(state.beforeOuterSuspend);
   EXPECT_FALSE(state.beforeInnerSuspend);
   EXPECT_FALSE(state.afterOuterSuspend);
   EXPECT_FALSE(state.afterInnerSuspend);
   EXPECT_FALSE(state.handle);
   EXPECT_LE(4, state.count); // 4 for MSVC, 5 for clang & gcc
   EXPECT_EQ(1, dispatcher.queue.size());

   // co-awaiting on Awaitable<State>
   dispatcher.ProcessOneTask();
   EXPECT_TRUE(state.beforeOuterSuspend);
   EXPECT_TRUE(state.beforeInnerSuspend);
   EXPECT_FALSE(state.afterOuterSuspend);
   EXPECT_FALSE(state.afterInnerSuspend);
   EXPECT_TRUE(state.handle);
   EXPECT_FALSE(state.handle.done());
   EXPECT_LE(5, state.count); // 5 for MSVC, 6 for clang & gcc
   EXPECT_EQ(0, dispatcher.queue.size());

   // inner coro reached final suspend and schedules the rest of the outer coro
   state.handle.resume();
   EXPECT_TRUE(state.beforeOuterSuspend);
   EXPECT_TRUE(state.beforeInnerSuspend);
   EXPECT_FALSE(state.afterOuterSuspend);
   EXPECT_TRUE(state.afterInnerSuspend);
   EXPECT_TRUE(state.handle);
   EXPECT_TRUE(state.handle.done());
   EXPECT_LE(4, state.count); // 4 for MSVC, 5 for clang & gcc
   EXPECT_EQ(1, dispatcher.queue.size());

   // outer coro reached final suspend
   dispatcher.ProcessOneTask();
   EXPECT_TRUE(state.beforeOuterSuspend);
   EXPECT_TRUE(state.beforeInnerSuspend);
   EXPECT_TRUE(state.afterOuterSuspend);
   EXPECT_TRUE(state.afterInnerSuspend);
   EXPECT_TRUE(state.handle);
   EXPECT_EQ(2, state.count);
   EXPECT_EQ(0, dispatcher.queue.size());

   // outer coro is destroyed
   task = {};
   EXPECT_TRUE(state.beforeOuterSuspend);
   EXPECT_TRUE(state.beforeInnerSuspend);
   EXPECT_TRUE(state.afterOuterSuspend);
   EXPECT_TRUE(state.afterInnerSuspend);
   EXPECT_TRUE(state.handle);
   EXPECT_EQ(1, state.count);
   EXPECT_EQ(0, dispatcher.queue.size());
}

TEST_F(TaskHandleFixture, task_doesnt_run_when_canceled_before_initial_suspend)
{
   using TaskType = cr::TaskHandle<void, ManualDispatcher::Executor>;

   struct State
   {
      bool beforeSuspend = false;
      bool afterSuspend = false;
      stdcr::coroutine_handle<> handle = nullptr;
      int count = 0;
   } state;

   Counter counter{state.count};
   ManualDispatcher dispatcher;

   static auto StartVoidOperation = [](State & s, Counter c) -> TaskType {
      (void)c;
      s.beforeSuspend = true;
      co_await Awaitable<State>{s};
      s.afterSuspend = true;
   };

   auto task = StartVoidOperation(state, counter);

   // co-awaiting on initial suspend
   EXPECT_FALSE(state.beforeSuspend);
   EXPECT_FALSE(state.afterSuspend);
   EXPECT_FALSE(state.handle);
   EXPECT_EQ(2, state.count);
   EXPECT_EQ(0, dispatcher.queue.size());

   // task schedules itself on executor
   task.Run(ManualDispatcher::Executor{&dispatcher});
   EXPECT_FALSE(state.beforeSuspend);
   EXPECT_FALSE(state.afterSuspend);
   EXPECT_FALSE(state.handle);
   EXPECT_EQ(2, state.count);
   EXPECT_EQ(1, dispatcher.queue.size());

   // coro doesn't know it's canceled until it resumes
   task = {};
   EXPECT_FALSE(state.beforeSuspend);
   EXPECT_FALSE(state.afterSuspend);
   EXPECT_FALSE(state.handle);
   EXPECT_EQ(2, state.count);
   EXPECT_EQ(1, dispatcher.queue.size());

   // coro resumes and finds out it's been canceled
   dispatcher.ProcessOneTask();
   EXPECT_FALSE(state.beforeSuspend);
   EXPECT_FALSE(state.afterSuspend);
   EXPECT_FALSE(state.handle);
   EXPECT_EQ(1, state.count);
   EXPECT_EQ(0, dispatcher.queue.size());
}

TEST_F(TaskHandleFixture, detached_task_schedules_lazy_inner_task_on_default_constructed_executor)
{
   using LazyIntTask = cr::TaskHandle<int, ManualDispatcher::Executor>;

   struct State
   {
      bool beforeSuspend = false;
      bool afterSuspend = false;
      int value = 0;
      stdcr::coroutine_handle<> handle = nullptr;
   } state;

   static auto StartInnerIntOperation = [](State & s) -> LazyIntTask {
      s.beforeSuspend = true;
      co_await Awaitable<State>{s};
      s.afterSuspend = true;
      co_return 42;
   };

   static auto StartDetachedOperation = [](State & s) -> cr::DetachedHandle {
      int result = co_await StartInnerIntOperation(s);
      s.value = result;
   };

   StartDetachedOperation(state);
   EXPECT_FALSE(state.beforeSuspend);
   EXPECT_FALSE(state.afterSuspend);
   EXPECT_EQ(0, state.value);
   EXPECT_FALSE(state.handle);
   EXPECT_EQ(1, ManualDispatcher::s_instance.queue.size());

   ManualDispatcher::s_instance.ProcessOneTask();
   EXPECT_TRUE(state.beforeSuspend);
   EXPECT_FALSE(state.afterSuspend);
   EXPECT_EQ(0, state.value);
   EXPECT_TRUE(state.handle);
   EXPECT_FALSE(state.handle.done());
   EXPECT_EQ(0, ManualDispatcher::s_instance.queue.size());

   state.handle.resume();
   EXPECT_TRUE(state.beforeSuspend);
   EXPECT_TRUE(state.afterSuspend);
   EXPECT_EQ(0, state.value);
   EXPECT_TRUE(state.handle);
   EXPECT_TRUE(state.handle.done());
   EXPECT_EQ(1, ManualDispatcher::s_instance.queue.size());

   ManualDispatcher::s_instance.ProcessOneTask();
   EXPECT_TRUE(state.beforeSuspend);
   EXPECT_TRUE(state.afterSuspend);
   EXPECT_EQ(42, state.value);
   EXPECT_TRUE(state.handle);
   EXPECT_EQ(0, ManualDispatcher::s_instance.queue.size());
}

TEST_F(TaskHandleFixture, eager_task_resumes_its_continuation)
{
   int value = 0;

   static auto EagerIntTask = []() -> cr::TaskHandle<int> {
      co_return 42;
   };

   static auto OuterVoidTask = [](int & outValue) -> cr::DetachedHandle {
      outValue = co_await EagerIntTask();
   };

   OuterVoidTask(value);
   EXPECT_EQ(42, value);
}

} // namespace
