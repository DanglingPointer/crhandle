#undef NDEBUG
#include <gtest/gtest.h>

#include "crhandle/unichannel.hpp"
#include "dispatcher.hpp"

namespace {

struct UnichannelFixture : public ::testing::Test
{
   using ImmediateChannel = cr::Unichannel<std::unique_ptr<int>>;
   using StepwiseChannel = cr::Unichannel<std::unique_ptr<int>, ManualDispatcher::Executor>;

   ManualDispatcher::Executor GetExecutor() { return ManualDispatcher::Executor{&dispatcher}; }

   ManualDispatcher dispatcher;
};

TEST_F(UnichannelFixture, unichannel_immediate_send_then_receive)
{
   auto ch = ImmediateChannel::Make();
   ImmediateChannel::Producer prod(ch);
   bool done = false;

   EXPECT_TRUE(prod.Send(std::make_unique<int>(42)));

   [&]() -> cr::DetachedHandle {
      auto result = co_await ch->Next();
      EXPECT_EQ(42, *result);
      done = true;
   }();

   EXPECT_TRUE(done);
}

TEST_F(UnichannelFixture, unichannel_stepwise_send_then_receive)
{
   auto ch = StepwiseChannel::Make(GetExecutor());
   StepwiseChannel::Producer prod(ch);
   bool done = false;

   EXPECT_TRUE(prod.Send(std::make_unique<int>(42)));
   EXPECT_TRUE(dispatcher.ProcessOneTask());
   EXPECT_FALSE(dispatcher.ProcessOneTask());

   static auto ReceiveOne = [](StepwiseChannel * ch,
                               bool & done) -> cr::TaskHandle<void, ManualDispatcher::Executor> {
      auto result = co_await ch->Next();
      EXPECT_EQ(42, *result);
      done = true;
   };

   auto task = ReceiveOne(ch.get(), done);
   task.Run(GetExecutor());
   EXPECT_TRUE(dispatcher.ProcessOneTask());
   EXPECT_TRUE(dispatcher.ProcessOneTask());
   EXPECT_FALSE(done);

   EXPECT_TRUE(dispatcher.ProcessOneTask());
   EXPECT_FALSE(dispatcher.ProcessOneTask());
   EXPECT_TRUE(done);

   EXPECT_FALSE(task);
}

TEST_F(UnichannelFixture, unichannel_immediate_receive_then_send)
{
   auto ch = ImmediateChannel::Make();
   ImmediateChannel::Producer prod(ch);
   bool done = false;

   [&]() -> cr::DetachedHandle {
      auto result = co_await ch->Next();
      EXPECT_EQ(42, *result);
      done = true;
   }();

   prod.Send(std::make_unique<int>(42));

   EXPECT_TRUE(done);
}

TEST_F(UnichannelFixture, unichannel_stepwise_receive_then_send)
{
   auto ch = StepwiseChannel::Make(GetExecutor());
   StepwiseChannel::Producer prod(ch);
   bool done = false;

   static auto ReceiveOne = [](StepwiseChannel * ch,
                               bool & done) -> cr::TaskHandle<void, ManualDispatcher::Executor> {
      auto result = co_await ch->Next();
      EXPECT_EQ(42, *result);
      done = true;
   };

   auto task = ReceiveOne(ch.get(), done);
   task.Run(GetExecutor());
   EXPECT_TRUE(dispatcher.ProcessOneTask());
   EXPECT_TRUE(dispatcher.ProcessOneTask());
   EXPECT_FALSE(dispatcher.ProcessOneTask());
   EXPECT_FALSE(done);

   prod.Send(std::make_unique<int>(42));
   EXPECT_TRUE(dispatcher.ProcessOneTask());
   EXPECT_TRUE(dispatcher.ProcessOneTask());
   EXPECT_FALSE(dispatcher.ProcessOneTask());
   EXPECT_TRUE(done);

   EXPECT_FALSE(task);
}

TEST_F(UnichannelFixture, unichannel_immediate_send_without_receiving)
{
   auto ch = ImmediateChannel::Make();
   ImmediateChannel::Producer prod(ch);

   EXPECT_TRUE(prod.Send(std::make_unique<int>(42)));

   ch.reset();

   EXPECT_FALSE(prod.Send(std::make_unique<int>(43)));
}

TEST_F(UnichannelFixture, unichannel_stepwise_send_without_receiving)
{
   auto ch = StepwiseChannel::Make(GetExecutor());
   StepwiseChannel::Producer prod(ch);

   EXPECT_TRUE(prod.Send(std::make_unique<int>(42)));
   EXPECT_TRUE(dispatcher.ProcessOneTask());
   EXPECT_FALSE(dispatcher.ProcessOneTask());

   ch.reset();
   EXPECT_FALSE(dispatcher.ProcessOneTask());

   EXPECT_FALSE(prod.Send(std::make_unique<int>(43)));
   EXPECT_FALSE(dispatcher.ProcessOneTask());
}

TEST_F(UnichannelFixture, unichannel_immediate_receive_without_sending)
{
   auto ch = ImmediateChannel::Make();
   ImmediateChannel::Producer prod(ch);
   bool done = false;

   [&]() -> cr::DetachedHandle {
      EXPECT_THROW({ auto result = co_await ch->Next(); }, cr::CanceledException);
      done = true;
   }();

   ch.reset();
   EXPECT_TRUE(done);
}

TEST_F(UnichannelFixture, unichannel_stepwise_receive_without_sending)
{
   auto ch = StepwiseChannel::Make(GetExecutor());
   StepwiseChannel::Producer prod(ch);
   bool done = false;

   static auto ReceiveOne = [](StepwiseChannel * ch,
                               bool & done) -> cr::TaskHandle<void, ManualDispatcher::Executor> {
      EXPECT_THROW({ auto result = co_await ch->Next(); }, cr::CanceledException);
      done = true;
   };

   auto task = ReceiveOne(ch.get(), done);
   task.Run(GetExecutor());
   EXPECT_TRUE(dispatcher.ProcessOneTask());
   EXPECT_TRUE(dispatcher.ProcessOneTask());
   EXPECT_FALSE(dispatcher.ProcessOneTask());
   EXPECT_FALSE(done);

   ch.reset();
   EXPECT_TRUE(dispatcher.ProcessOneTask());
   EXPECT_FALSE(dispatcher.ProcessOneTask());
   EXPECT_TRUE(done);

   EXPECT_FALSE(task);
}

TEST_F(UnichannelFixture, unichannel_immediate_cancels_task_when_dies)
{
   auto ch = ImmediateChannel::Make();
   ImmediateChannel::Producer prod(ch);

   static auto ReceiveOne = [](ImmediateChannel * ch) -> cr::TaskHandle<void, cr::InlineExecutor> {
      auto result = co_await ch->Next();
   };

   auto task = ReceiveOne(ch.get());
   EXPECT_TRUE(task);

   task.Run();
   EXPECT_TRUE(task);

   ch.reset();
   EXPECT_FALSE(task);
}

TEST_F(UnichannelFixture, unichannel_stepwise_cancels_task_when_dies)
{
   auto ch = StepwiseChannel::Make(GetExecutor());
   StepwiseChannel::Producer prod(ch);

   static auto ReceiveOne =
      [](StepwiseChannel * ch) -> cr::TaskHandle<void, ManualDispatcher::Executor> {
      auto result = co_await ch->Next();
   };

   auto task = ReceiveOne(ch.get());
   EXPECT_TRUE(task);

   task.Run(GetExecutor());
   EXPECT_TRUE(dispatcher.ProcessOneTask());
   EXPECT_TRUE(dispatcher.ProcessOneTask());
   EXPECT_FALSE(dispatcher.ProcessOneTask());
   EXPECT_TRUE(task);

   ch.reset();
   EXPECT_TRUE(dispatcher.ProcessOneTask());
   EXPECT_FALSE(dispatcher.ProcessOneTask());
   EXPECT_FALSE(task);
}

TEST_F(UnichannelFixture, unichannel_immediate_preserves_send_order)
{
   auto ch = ImmediateChannel::Make();
   ImmediateChannel::Producer prod(ch);
   bool done = false;

   EXPECT_TRUE(prod.Send(std::make_unique<int>(42)));
   EXPECT_TRUE(prod.Send(std::make_unique<int>(43)));
   EXPECT_TRUE(prod.Send(std::make_unique<int>(44)));

   [ch = ch.get(), &done]() -> cr::DetachedHandle {
      {
         auto result = co_await ch->Next();
         EXPECT_EQ(42, *result);
      }
      {
         auto result = co_await ch->Next();
         EXPECT_EQ(43, *result);
      }
      {
         auto result = co_await ch->Next();
         EXPECT_EQ(44, *result);
      }
      EXPECT_THROW(
         {
            auto result1 = co_await ch->Next();
            auto result2 = co_await ch->Next();
         },
         cr::CanceledException);
      done = true;
   }();

   ch.reset();
   EXPECT_TRUE(done);
}

TEST_F(UnichannelFixture, unichannel_stepwise_preserves_send_order)
{
   auto ch = StepwiseChannel::Make(GetExecutor());
   StepwiseChannel::Producer prod(ch);
   bool done = false;

   EXPECT_TRUE(prod.Send(std::make_unique<int>(42)));
   EXPECT_TRUE(prod.Send(std::make_unique<int>(43)));
   EXPECT_TRUE(prod.Send(std::make_unique<int>(44)));

   static auto ReceiveFive = [](StepwiseChannel * ch,
                                bool & done) -> cr::TaskHandle<void, ManualDispatcher::Executor> {
      {
         auto result = co_await ch->Next();
         EXPECT_EQ(42, *result);
      }
      {
         auto result = co_await ch->Next();
         EXPECT_EQ(43, *result);
      }
      {
         auto result = co_await ch->Next();
         EXPECT_EQ(44, *result);
      }
      EXPECT_THROW(
         {
            auto result1 = co_await ch->Next();
            auto result2 = co_await ch->Next();
         },
         cr::CanceledException);
      done = true;
   };

   auto task = ReceiveFive(ch.get(), done);
   task.Run(GetExecutor());
   while (dispatcher.ProcessOneTask())
      ;
   EXPECT_FALSE(done);

   ch.reset();
   while (dispatcher.ProcessOneTask())
      ;
   EXPECT_TRUE(done);
   EXPECT_FALSE(task);
}

TEST_F(UnichannelFixture, unichannel_immediate_honours_subscription_order)
{
   auto ch = ImmediateChannel::Make();
   ImmediateChannel::Producer prod(ch);
   bool done1 = false;
   bool done2 = false;

   auto StartFirst = [ch = ch.get(), &done1]() -> cr::DetachedHandle {
      {
         auto result = co_await ch->Next();
         EXPECT_EQ(42, *result);
      }
      EXPECT_THROW(
         {
            auto result1 = co_await ch->Next();
            auto result2 = co_await ch->Next();
         },
         cr::CanceledException);
      done1 = true;
   };
   StartFirst();

   auto StartSecond = [ch = ch.get(), &done2]() -> cr::DetachedHandle {
      {
         auto result = co_await ch->Next();
         EXPECT_EQ(43, *result);
      }
      EXPECT_THROW(
         {
            auto result1 = co_await ch->Next();
            auto result2 = co_await ch->Next();
         },
         cr::CanceledException);
      done2 = true;
   };
   StartSecond();

   EXPECT_TRUE(prod.Send(std::make_unique<int>(42)));
   EXPECT_TRUE(prod.Send(std::make_unique<int>(43)));

   ch.reset();
   EXPECT_TRUE(done1);
   EXPECT_TRUE(done2);
}

TEST_F(UnichannelFixture, unichannel_stepwise_honours_subscription_order)
{
   auto ch = StepwiseChannel::Make(GetExecutor());
   StepwiseChannel::Producer prod(ch);
   bool done1 = false;
   bool done2 = false;

   static auto ReceiveOne = [](int expected,
                               StepwiseChannel * ch,
                               bool & done) -> cr::TaskHandle<void, ManualDispatcher::Executor> {
      {
         auto result = co_await ch->Next();
         EXPECT_EQ(expected, *result);
      }
      done = true;
      auto result1 = co_await ch->Next();
      auto result2 = co_await ch->Next();
   };

   auto task1 = ReceiveOne(42, ch.get(), done1);
   task1.Run(GetExecutor());

   auto task2 = ReceiveOne(43, ch.get(), done2);
   task2.Run(GetExecutor());

   EXPECT_TRUE(prod.Send(std::make_unique<int>(42)));
   EXPECT_TRUE(prod.Send(std::make_unique<int>(43)));

   while (dispatcher.ProcessOneTask())
      ;

   EXPECT_TRUE(done1);
   EXPECT_TRUE(done2);
   EXPECT_TRUE(task1);
   EXPECT_TRUE(task2);

   ch.reset();
   while (dispatcher.ProcessOneTask())
      ;

   EXPECT_FALSE(task1);
   EXPECT_FALSE(task2);
}

TEST_F(UnichannelFixture, unichannel_immediate_ignores_canceled_consumers)
{
   auto ch = ImmediateChannel::Make();
   ImmediateChannel::Producer prod(ch);

   int received1 = 0;
   int received2 = 0;

   static auto ReceiveOne = [](ImmediateChannel * ch, int & result) -> cr::TaskHandle<void> {
      result = *co_await ch->Next();
   };

   auto task1 = ReceiveOne(ch.get(), received1);
   task1.Run();

   auto task2 = ReceiveOne(ch.get(), received2);
   task2.Run();

   task1 = {};

   EXPECT_TRUE(prod.Send(std::make_unique<int>(42)));

   EXPECT_EQ(0, received1);
   EXPECT_EQ(42, received2);

   EXPECT_FALSE(task1);
   EXPECT_FALSE(task2);
}

TEST_F(UnichannelFixture, unichannel_stepwise_ignores_canceled_consumers)
{
   auto ch = StepwiseChannel::Make(GetExecutor());
   StepwiseChannel::Producer prod(ch);

   int received1 = 0;
   int received2 = 0;

   static auto ReceiveOne = [](StepwiseChannel * ch,
                               int & result) -> cr::TaskHandle<void, ManualDispatcher::Executor> {
      result = *co_await ch->Next();
   };

   auto task1 = ReceiveOne(ch.get(), received1);
   task1.Run(GetExecutor());

   auto task2 = ReceiveOne(ch.get(), received2);
   task2.Run(GetExecutor());

   while (dispatcher.ProcessOneTask())
      ;

   task1 = {};

   EXPECT_TRUE(prod.Send(std::make_unique<int>(42)));

   while (dispatcher.ProcessOneTask())
      ;

   EXPECT_EQ(0, received1);
   EXPECT_EQ(42, received2);

   EXPECT_FALSE(task1);
   EXPECT_FALSE(task2);
}

TEST_F(UnichannelFixture, unichannel_immediate_all_canceled_consumers)
{
   auto ch = ImmediateChannel::Make();
   ImmediateChannel::Producer prod(ch);

   int received1 = 0;
   int received2 = 0;

   static auto ReceiveOne = [](ImmediateChannel * ch, int & result) -> cr::TaskHandle<void> {
      result = *co_await ch->Next();
   };

   auto task1 = ReceiveOne(ch.get(), received1);
   task1.Run();

   auto task2 = ReceiveOne(ch.get(), received2);
   task2.Run();

   task1 = {};
   task2 = {};

   EXPECT_TRUE(prod.Send(std::make_unique<int>(42)));

   EXPECT_EQ(0, received1);
   EXPECT_EQ(0, received2);

   EXPECT_FALSE(task1);
   EXPECT_FALSE(task2);

   int received3 = 0;
   auto task3 = ReceiveOne(ch.get(), received3);
   task3.Run();

   EXPECT_EQ(42, received3);
   EXPECT_FALSE(task3);
}

TEST_F(UnichannelFixture, unichannel_stepwise_all_canceled_consumers)
{
   auto ch = StepwiseChannel::Make(GetExecutor());
   StepwiseChannel::Producer prod(ch);

   int received1 = 0;
   int received2 = 0;

   static auto ReceiveOne = [](StepwiseChannel * ch,
                               int & result) -> cr::TaskHandle<void, ManualDispatcher::Executor> {
      result = *co_await ch->Next();
   };

   auto task1 = ReceiveOne(ch.get(), received1);
   task1.Run(GetExecutor());

   auto task2 = ReceiveOne(ch.get(), received2);
   task2.Run(GetExecutor());

   while (dispatcher.ProcessOneTask())
      ;

   task1 = {};
   task2 = {};

   EXPECT_TRUE(prod.Send(std::make_unique<int>(42)));

   while (dispatcher.ProcessOneTask())
      ;

   EXPECT_EQ(0, received1);
   EXPECT_EQ(0, received2);

   EXPECT_FALSE(task1);
   EXPECT_FALSE(task2);

   int received3 = 0;
   auto task3 = ReceiveOne(ch.get(), received3);
   task3.Run(GetExecutor());
   while (dispatcher.ProcessOneTask())
      ;

   EXPECT_EQ(42, received3);
   EXPECT_FALSE(task3);
}

} // namespace
