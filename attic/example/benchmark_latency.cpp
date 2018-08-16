#include "afio_pch.hpp"
#include <thread>

#define ITERATIONS 10000
#define CONCURRENCY 32

// Optional
//#define MULTIPLIER 1000000 // output number of microseconds instead of seconds
#define MULTIPLIER 3900000000ULL // output number of CPU clocks instead of seconds

typedef decltype(boost::afio::chrono::high_resolution_clock::now()) time_point;
size_t id_offset;
static time_point points[100000];
static time_point::duration overhead, timesliceoverhead, sleepoverhead;
static std::pair<bool, std::shared_ptr<boost::afio::handle>> _callback(size_t id, boost::afio::future<> op)
{
  using namespace boost::afio;
  points[id-id_offset]=chrono::high_resolution_clock::now();
  return std::make_pair(true, op.get_handle());
};

int main(void)
{
  using namespace boost::afio;
  auto dispatcher=make_dispatcher().get();
  typedef chrono::duration<double, ratio<1, 1>> secs_type;
  {
    size_t total1=0, total2=0, total3=0;
    auto begin=chrono::high_resolution_clock::now();
    while(chrono::duration_cast<secs_type>(chrono::high_resolution_clock::now()-begin).count()<1)
    {
      auto now2=chrono::high_resolution_clock::now();
      this_thread::yield();
      auto now3=chrono::high_resolution_clock::now();
      timesliceoverhead+=now3-now2;
      total1++;
    }
    while(chrono::duration_cast<secs_type>(chrono::high_resolution_clock::now()-begin).count()<2)
    {
      auto now2=chrono::high_resolution_clock::now();
      this_thread::sleep_for(chrono::nanoseconds(1));
      auto now3=chrono::high_resolution_clock::now();
      sleepoverhead+=now3-now2;
      total3++;
    }
    while(chrono::duration_cast<secs_type>(chrono::high_resolution_clock::now()-begin).count()<3)
    {
      auto now1=chrono::high_resolution_clock::now();
      auto now2=chrono::high_resolution_clock::now();
      overhead+=now2-now1;
      total2++;
    }
    overhead=time_point::duration(overhead.count()/total2);
    sleepoverhead=time_point::duration(sleepoverhead.count()/total3);
    timesliceoverhead=time_point::duration(timesliceoverhead.count()/total1);
    std::cout << "Timing overhead is calculated to be " << chrono::duration_cast<secs_type>(overhead).count() << " seconds." << std::endl;
    std::cout << "OS context switch overhead is calculated to be " << chrono::duration_cast<secs_type>(timesliceoverhead).count() << " seconds." << std::endl;
    std::cout << "OS sleep overhead is calculated to be " << chrono::duration_cast<secs_type>(sleepoverhead).count() << " seconds." << std::endl;
  }  
  
  std::pair<async_op_flags, dispatcher::completion_t *> callback(async_op_flags::none, _callback);
  std::ofstream csv("afio_latencies.csv");
  csv << "Timing overhead is calculated to be," << chrono::duration_cast<secs_type>(overhead).count()
#ifdef MULTIPLIER
    * MULTIPLIER
#endif
    << std::endl;
  csv << "OS context switch overhead is calculated to be," << chrono::duration_cast<secs_type>(timesliceoverhead).count()
#ifdef MULTIPLIER
    * MULTIPLIER
#endif
    << std::endl << std::endl;
  csv << "OS sleep overhead is calculated to be," << chrono::duration_cast<secs_type>(sleepoverhead).count()
#ifdef MULTIPLIER
    * MULTIPLIER
#endif
    << std::endl << std::endl;
  csv << "Concurrency,Handler Min,Handler Max,Handler Average,Handler Stddev,Complete Min,Complete Max,Complete Average,Complete Stddev" << std::endl;
  for(size_t concurrency=0; concurrency<CONCURRENCY; concurrency++)
  {
    future<> last[CONCURRENCY];
    time_point begin[CONCURRENCY], handled[CONCURRENCY], end[CONCURRENCY];
    atomic<bool> waiter;
    double handler[ITERATIONS], complete[ITERATIONS];
    std::vector<std::thread> threads;
    threads.reserve(CONCURRENCY);
    size_t iterations=(concurrency>=8) ? ITERATIONS/10 : ITERATIONS;
    std::cout << "Running " << iterations << " iterations of concurrency " << concurrency+1 << " ..." << std::endl;
    for(size_t n=0; n<iterations; n++)
    {
      threads.clear();
      waiter.store(true);
      atomic<size_t> threads_ready(0);
      for(size_t c=0; c<=concurrency; c++)
      {
        threads.push_back(std::thread([&, c]{
          ++threads_ready;
          while(waiter)
#ifdef BOOST_SMT_PAUSE
            BOOST_SMT_PAUSE
#endif
          ;
          begin[c]=chrono::high_resolution_clock::now();
          last[c]=dispatcher->completion(future<>(), callback);
          last[c].get();
          end[c]=chrono::high_resolution_clock::now();
          handled[c]=points[last[c].id()-id_offset];
        }));
      }
      while(threads_ready<=concurrency)
#ifdef BOOST_SMT_PAUSE
        BOOST_SMT_PAUSE
#endif
      ;
      waiter.store(false);
      for(auto &i: threads)
        i.join();
      for(size_t c=0; c<=concurrency; c++)
      {
        handler[n]=chrono::duration_cast<secs_type>(handled[c]-begin[c]-overhead).count();
        complete[n]=chrono::duration_cast<secs_type>(end[c]-handled[c]-overhead).count();
      }
    }
    for(size_t n=0; n<=concurrency; n++)
      if(last[n].id()>id_offset) id_offset=last[n].id();
    double minHandler=1<<30, maxHandler=0, totalHandler=0, minComplete=1<<30, maxComplete=0, totalComplete=0;
    for(size_t n=0; n<iterations; n++)
    {
      if(handler[n]<minHandler) minHandler=handler[n];
      if(handler[n]>maxHandler) maxHandler=handler[n];
      if(complete[n]<minHandler) minComplete=complete[n];
      if(complete[n]>maxHandler) maxComplete=complete[n];
      totalHandler+=handler[n];
      totalComplete+=complete[n];
    }
    totalHandler/=iterations;
    totalComplete/=iterations;
    double varianceHandler=0, varianceComplete=0;
    for(size_t n=0; n<iterations; n++)
    {
      varianceHandler+=pow(handler[n]-totalHandler, 2);
      varianceComplete+=pow(complete[n]-totalComplete, 2);
    }
    varianceHandler/=iterations;
    varianceComplete/=iterations;
    varianceHandler=sqrt(varianceHandler);
    varianceComplete=sqrt(varianceComplete);
#ifdef MULTIPLIER
    minHandler*=MULTIPLIER;
    maxHandler*=MULTIPLIER;
    totalHandler*=MULTIPLIER;
    varianceHandler*=MULTIPLIER;
    minComplete*=MULTIPLIER;
    maxComplete*=MULTIPLIER;
    totalComplete*=MULTIPLIER;
    varianceComplete*=MULTIPLIER;    
#endif
    csv << concurrency+1 << "," << minHandler << "," << maxHandler << "," << totalHandler << "," << varianceHandler << ","
        << minComplete << "," << maxComplete << "," << totalComplete << "," << varianceComplete << std::endl;
  }
  return 0;
}
