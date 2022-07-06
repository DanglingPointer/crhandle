#ifndef TASK_OWNER_HPP
#define TASK_OWNER_HPP

#include "crhandle/taskhandle.hpp"

#include <vector>

namespace cr {

template <Executor E = InlineExecutor>
class TaskOwner
{
public:
   explicit TaskOwner(E executor = {})
      : m_executor(executor)
   {}

   void StartRootTask(TaskHandle<void, E> && task)
   {
      RethrowExceptions();
      std::erase_if(m_tasks, [](const TaskHandle<void, E> & t) {
         return !t;
      });
      m_tasks.emplace_back(std::move(task));
      m_tasks.back().Run(m_executor);
   }

   [[nodiscard]] auto StartNestedTask(TaskHandle<void, E> && task)
   {
      struct SuspenderStarter
      {
         TaskOwner<E> & owner;
         TaskHandle<void, E> task;

         bool await_ready() noexcept { return false; }
         bool await_suspend(stdcr::coroutine_handle<>)
         {
            owner.StartRootTask(std::move(task));
            return false;
         }
         void await_resume() noexcept {}
      };
      return SuspenderStarter{*this, std::move(task)};
   }

   void RethrowExceptions()
   {
      for (auto & task : m_tasks)
         task.EnsureNoException();
   }

   E Executor() const { return m_executor; }

private:
   E m_executor;
   std::vector<TaskHandle<void, E>> m_tasks;
};

} // namespace cr

#endif
