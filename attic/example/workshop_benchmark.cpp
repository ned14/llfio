#include "afio_pch.hpp"

#ifdef _DEBUG
#define ITEMS 64
#else
#define ITEMS 65536
#endif
#define PARALLELISM 256  // up to max fds on your platform

namespace iostreams {
#include "workshop_naive.ipp"
}
namespace naive {
#include "workshop_naive_afio.ipp"
}
namespace atomic_updates {
#include "workshop_atomic_updates_afio.ipp"
}
namespace final {
#include "workshop_final_afio.ipp"
#include "../detail/SpookyV2.cpp"
}


namespace filesystem = BOOST_AFIO_V2_NAMESPACE::filesystem;
namespace chrono = BOOST_AFIO_V2_NAMESPACE::chrono;
using BOOST_AFIO_V2_NAMESPACE::ratio;

// From http://burtleburtle.net/bob/rand/smallprng.html
typedef unsigned int  u4;
typedef struct ranctx { u4 a; u4 b; u4 c; u4 d; } ranctx;

#define rot(x,k) (((x)<<(k))|((x)>>(32-(k))))
static u4 ranval(ranctx *x) {
    u4 e = x->a - rot(x->b, 27);
    x->a = x->b ^ rot(x->c, 17);
    x->b = x->c + x->d;
    x->c = x->d + e;
    x->d = e + x->a;
    return x->d;
}

static void raninit(ranctx *x, u4 seed) {
    u4 i;
    x->a = 0xf1ea5eed, x->b = x->c = x->d = seed;
    for (i = 0; i < 20; ++i) {
        (void) ranval(x);
    }
}

static std::vector<std::pair<ranctx, unsigned>> items;

void prepare()
{
  ranctx gen;
  raninit(&gen, 0x78adbcff);
  items.clear();
  items.reserve(ITEMS);
  for (size_t n = 0; n < ITEMS; n++)
  {
    unsigned t=ranval(&gen);
    items.push_back(std::make_pair(gen, t));
  }
}

template<class data_store> void benchmark(const char *filename, const char *desc, bool parallel_writes)
{
  typedef chrono::duration<double, ratio<1, 1>> secs_type;
  std::vector<std::tuple<size_t, double, double>> results;
  prepare();
  for(size_t n=1; n<=ITEMS; n<<=1)
  {
    std::cout << "Benchmarking " << desc << " insertion of " << n << " records ... ";
    if(filesystem::exists("store"))
      filesystem::remove_all("store");
    auto begin=chrono::high_resolution_clock::now();
    while(chrono::duration_cast<secs_type>(chrono::high_resolution_clock::now()-begin).count()<1);
    data_store store(data_store::writeable);
    size_t write_parallelism = parallel_writes ? PARALLELISM : 1;
    std::vector<typename data_store::write_result_type> ops(write_parallelism);
    begin=chrono::high_resolution_clock::now();
    for (size_t m = 0; m < n; m += write_parallelism)
    {
      int todo = (int)(n < write_parallelism ? n : write_parallelism);
#pragma omp parallel for
      for (int o = 0; o < todo; o++)
        ops[o] = store.write(std::to_string(m + (size_t)o));
#pragma omp parallel for
      for (int o = 0; o < todo; o++)
      {
        *ops[o].get() << items[m + (size_t)o].second;
        ops[o].get()->flush();
      }
      // Some dumping of the writes should have happened by now
#pragma omp parallel for
      for (int o = 0; o < todo; o++)
        ops[o]=typename data_store::write_result_type();
    }
    auto end=chrono::high_resolution_clock::now();
    auto diff=chrono::duration_cast<secs_type>(end-begin).count();
    std::cout << (n/diff) << " items/sec" << std::endl;
    results.push_back(std::make_tuple(n, n/diff, 0));
  }
  auto resultsit = results.begin();
  for (size_t n = 1; n <= ITEMS; n <<= 1)
  {
    std::cout << "Benchmarking " << desc << " lookup of " << n << " records ... ";
    data_store store;
    std::vector<typename data_store::lookup_result_type> ops(PARALLELISM);
    auto begin = chrono::high_resolution_clock::now();
    for (size_t m = 0; m < n; m += PARALLELISM)
    {
      int todo = n < PARALLELISM ? n : PARALLELISM;
#pragma omp parallel for
      for (int o = 0; o < todo; o++)
        ops[o] = store.lookup(std::to_string(m + (size_t) o));
      // Some readahead should have happened by now
#pragma omp parallel for
      for (int o = 0; o < todo; o++)
      {
        auto s(ops[o].get());
        unsigned t;
        *s >> t;
        if (t != items[m+(size_t) o].second)
          std::cerr << "ERROR: Item " << m << " has incorrect contents!" << std::endl;
        ops[o]=typename data_store::lookup_result_type();
      }
    }
    auto end = chrono::high_resolution_clock::now();
    auto diff = chrono::duration_cast<secs_type>(end - begin).count();
    std::cout << (n / diff) << " items/sec" << std::endl;
    std::get<2>(*resultsit++) = n / diff;
  }
  std::ofstream csvh(filename);
  csvh << "Items,Inserts/sec,Lookups/sec" << std::endl;
  for(auto &i : results)
    csvh << std::get<0>(i) << "," << std::get<1>(i) << "," << std::get<2>(i) << std::endl;
}

int main(void)
{
  typedef chrono::duration<double, ratio<1, 1>> secs_type;
  auto begin=chrono::high_resolution_clock::now();
  while(chrono::duration_cast<secs_type>(chrono::high_resolution_clock::now()-begin).count()<3);
  
  //benchmark<iostreams::data_store>("iostreams.csv", "STL iostreams", true);
  //benchmark<naive::data_store>("afio_naive.csv", "AFIO naive", true);
  //benchmark<atomic_updates::data_store>("afio_atomic.csv", "AFIO atomic update", true);
  benchmark<final::data_store>("afio_final.csv", "AFIO single file", true);
  return 0;
}