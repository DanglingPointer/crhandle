#ifndef TEST_MANUALDISPATCHER_HPP
#define TEST_MANUALDISPATCHER_HPP

#include <cassert>
#include <deque>
#include <functional>
#include <utility>

template <typename F>
struct AlwaysCopyable : F
{
private:
   AlwaysCopyable(F & f)
      : F(std::move(f))
   {}
   AlwaysCopyable(AlwaysCopyable & c)
      : AlwaysCopyable(static_cast<F &>(c))
   {}

public:
   AlwaysCopyable(F && f)
      : F(std::move(f))
   {}
   AlwaysCopyable(AlwaysCopyable && cc)
      : AlwaysCopyable(static_cast<F &&>(cc))
   {}
   AlwaysCopyable(const AlwaysCopyable & c)
      : AlwaysCopyable(const_cast<AlwaysCopyable &>(c))
   {}
};

struct ManualDispatcher
{
   using Task = std::function<void()>;

   struct Executor
   {
      ManualDispatcher * master = nullptr;

      template <typename F>
      void Execute(F && task)
      {
         assert(master);
         master->queue.push_back(AlwaysCopyable(std::move(task)));
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

   void ProcessAll()
   {
      while (ProcessOneTask())
         ;
   }

   std::deque<Task> queue;
};

#endif
