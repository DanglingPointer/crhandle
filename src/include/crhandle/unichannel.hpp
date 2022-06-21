#ifndef UNICHANNEL_HPP
#define UNICHANNEL_HPP

#include "crhandle/task.hpp"

#include <cassert>
#include <deque>
#include <memory>

namespace cr {

template <typename T, Executor E = InlineExecutor>
class Unichannel
   : public std::enable_shared_from_this<Unichannel<T, E>>
   , private E
{
public:
   using ChT = Unichannel<T, E>;

   static std::shared_ptr<ChT> Make(E executor = {})
   {
      return std::shared_ptr<ChT>(new Unichannel(std::move(executor)));
   }

   ~Unichannel()
   {
      assert(m_consumers.empty() || m_items.empty());
      for (auto handle : m_consumers)
         handle.resume();
   }

   cr::TaskHandle<T, E> Next() { co_return co_await SubmitConsumer(); }

   class Producer : private E
   {
   public:
      explicit Producer(const std::shared_ptr<ChT> & channel)
         : E(static_cast<E &>(*channel))
         , m_channel(channel)
      {}

      bool Send(T && item)
      {
         auto channel = m_channel.lock();
         if (!channel)
            return false;

         E::Execute([channel = std::move(channel), item = std::move(item)]() mutable {
            channel->SubmitItem(std::move(item));
         });
         return true;
      }

   private:
      std::weak_ptr<ChT> m_channel;
   };


private:
   explicit Unichannel(E executor)
      : E{executor}
   {}

   auto SubmitConsumer()
   {
      struct Awaiter
      {
         Unichannel & owner;

         bool await_ready() noexcept { return !owner.m_items.empty(); }
         void await_suspend(stdcr::coroutine_handle<> handle)
         {
            assert(owner.m_items.empty());
            owner.m_consumers.emplace_back(handle);
         }
         T await_resume()
         {
            if (owner.m_items.empty())
               throw CanceledException{};

            T temp = std::move(owner.m_items.front());
            owner.m_items.pop_front();
            return temp;
         }
      };
      return Awaiter{*this};
   }

   void SubmitItem(T && item)
   {
      m_items.emplace_back(std::move(item));

      if (m_consumers.empty())
         return;

      auto callback = m_consumers.front();
      m_consumers.pop_front();
      callback.resume();
   }

   std::deque<stdcr::coroutine_handle<>> m_consumers;
   std::deque<T> m_items;
};

} // namespace cr

#endif
