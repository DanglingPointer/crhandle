#ifndef TEST_COUNTER_HPP
#define TEST_COUNTER_HPP

struct Counter
{
   Counter(int & counter)
      : count(counter)
   {
      ++count;
   }
   Counter(const Counter & other)
      : Counter(other.count)
   {}
   ~Counter() { --count; }

private:
   int & count;
};

#endif
