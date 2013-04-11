/* Unit tests for TripleGit
(C) 2013 Niall Douglas http://www.nedprod.com/
Created: Feb 2013
*/

#include <utility>
#include <sstream>
#include <iostream>
#include "../triplegit/include/triplegit.hpp"
#include "../triplegit/include/async_file_io.hpp"
#include "boost/graph/topological_sort.hpp"
#include "boost/graph/depth_first_search.hpp"
#include "boost/graph/dijkstra_shortest_paths.hpp"
#include "boost/graph/visitors.hpp"
#include "boost/graph/isomorphism.hpp"
#define CATCH_CONFIG_RUNNER
#include "catch.hpp"

enum files_e { dax_h, yow_h, boz_h, zow_h, foo_cpp, 
               foo_o, bar_cpp, bar_o, libfoobar_a,
               zig_cpp, zig_o, zag_cpp, zag_o, 
                 libzigzag_a, killerapp, N };
const char* name[] = { "dax.h", "yow.h", "boz.h", "zow.h", "foo.cpp",
                       "foo.o", "bar.cpp", "bar.o", "libfoobar.a",
                       "zig.cpp", "zig.o", "zag.cpp", "zag.o",
                       "libzigzag.a", "killerapp" };
typedef std::pair<int,int> Edge;
Edge used_by[] = {
	Edge(dax_h, foo_cpp), Edge(dax_h, bar_cpp), Edge(dax_h, yow_h),
	Edge(yow_h, bar_cpp), Edge(yow_h, zag_cpp),
	Edge(boz_h, bar_cpp), Edge(boz_h, zig_cpp), Edge(boz_h, zag_cpp),
	Edge(zow_h, foo_cpp), 
	Edge(foo_cpp, foo_o),
	Edge(foo_o, libfoobar_a),
	Edge(bar_cpp, bar_o),
	Edge(bar_o, libfoobar_a),
	Edge(libfoobar_a, libzigzag_a),
	Edge(zig_cpp, zig_o),
	Edge(zig_o, libzigzag_a),
	Edge(zag_cpp, zag_o),
	Edge(zag_o, libzigzag_a),
	Edge(libzigzag_a, killerapp)
};
const std::size_t nedges = sizeof(used_by)/sizeof(Edge);

//static triplegit::fs_store store(std::filesystem::current_path());
//static triplegit::collection_id testgraph(store, "unittests.testgraph");

int main (int argc, char * const argv[]) {
    int ret=Catch::Main( argc, argv );
	printf("Press Return to exit ...\n");
	getchar();
	return ret;
}

struct print_visitor : public boost::bfs_visitor<> {
  std::ostream &out;
  print_visitor(std::ostream &_out) : out(_out) { }
  template <class Vertex, class Graph>
  void discover_vertex(Vertex v, Graph&) {
    out << name[v] << " ";
  }
};


struct cycle_detector : public boost::dfs_visitor<>
{
  cycle_detector(bool& has_cycle) 
    : m_has_cycle(has_cycle) { }

  template <class Edge, class Graph>
  void back_edge(Edge, Graph&) { m_has_cycle = true; }
protected:
  bool& m_has_cycle;
};

template<class adjacency_list> void TestGraph(adjacency_list &g)
{
	using namespace boost;
	using namespace std;
	typedef adjacency_list Graph;
	typedef typename graph_traits<Graph>::vertex_descriptor Vertex;
	ostringstream out;

  // Determine ordering for a full recompilation
  // and the order with files that can be compiled in parallel
  {
    typedef list<Vertex> MakeOrder;
    typename MakeOrder::iterator i;
    MakeOrder make_order;

    topological_sort(g, std::front_inserter(make_order));
    out << "make ordering: ";
    for (i = make_order.begin();
         i != make_order.end(); ++i) 
      out << name[*i] << " ";
  
    out << endl << endl;
	cout << out.str();
	CHECK(out.str()=="make ordering: zow.h boz.h zig.cpp zig.o dax.h yow.h zag.cpp zag.o bar.cpp bar.o foo.cpp foo.o libfoobar.a libzigzag.a killerapp \n\n");
	out.clear();
	out.str("");

    // Parallel compilation ordering
    std::vector<int> time(N, 0);
    for (i = make_order.begin(); i != make_order.end(); ++i) {    
      // Walk through the in_edges an calculate the maximum time.
      if (in_degree (*i, g) > 0) {
        typename Graph::in_edge_iterator j, j_end;
        int maxdist=0;
        // Through the order from topological sort, we are sure that every 
        // time we are using here is already initialized.
        for (boost::tie(j, j_end) = in_edges(*i, g); j != j_end; ++j)
          maxdist=(std::max)(time[source(*j, g)], maxdist);
        time[*i]=maxdist+1;
      }
    }

    out << "parallel make ordering, " << endl
         << "vertices with same group number can be made in parallel" << endl;
    {
      typename graph_traits<Graph>::vertex_iterator i, iend;
      for (boost::tie(i,iend) = vertices(g); i != iend; ++i)
        out << "time_slot[" << name[*i] << "] = " << time[*i] << endl;
    }

  }
  out << endl;
	cout << out.str();
	const char *test1=R"(parallel make ordering, 
vertices with same group number can be made in parallel
time_slot[dax.h] = 0
time_slot[yow.h] = 1
time_slot[boz.h] = 0
time_slot[zow.h] = 0
time_slot[foo.cpp] = 1
time_slot[foo.o] = 2
time_slot[bar.cpp] = 2
time_slot[bar.o] = 3
time_slot[libfoobar.a] = 4
time_slot[zig.cpp] = 1
time_slot[zig.o] = 2
time_slot[zag.cpp] = 2
time_slot[zag.o] = 3
time_slot[libzigzag.a] = 5
time_slot[killerapp] = 6

)";
	CHECK(out.str()==test1);
	out.clear();
	out.str("");

  // if I change yow.h what files need to be re-made?
  {
    out << "A change to yow.h will cause what to be re-made?" << endl;
    print_visitor vis(out);
    breadth_first_search(g, vertex(yow_h, g), visitor(vis));
    out << endl;
  }
  out << endl;
	cout << out.str();
	CHECK(out.str()=="A change to yow.h will cause what to be re-made?\nyow.h bar.cpp zag.cpp bar.o zag.o libfoobar.a libzigzag.a killerapp \n\n");
	out.clear();
	out.str("");
}

template<class adjacency_list> void ModifyGraph(adjacency_list &g)
{
	using namespace boost;
	using namespace std;
	typedef adjacency_list Graph;
	typedef typename graph_traits<Graph>::vertex_descriptor Vertex;
	ostringstream out;

  // are there any cycles in the graph?
  {
    bool has_cycle = false;
    cycle_detector vis(has_cycle);
    depth_first_search(g, visitor(vis));
    out << "The graph has a cycle? " << has_cycle << endl;
  }
  out << endl;
	cout << out.str();
	CHECK(out.str()=="The graph has a cycle? 0\n\n");
	out.clear();
	out.str("");

  // add a dependency going from bar.cpp to dax.h
  {
    out << "adding edge bar_cpp -> dax_h" << endl;
    add_edge(bar_cpp, dax_h, g);
  }
  out << endl;

  // are there any cycles in the graph?
  {
    bool has_cycle = false;
    cycle_detector vis(has_cycle);
    depth_first_search(g, visitor(vis));
    out << "The graph has a cycle now? " << has_cycle << endl;
  }
	cout << out.str();
	CHECK(out.str()=="adding edge bar_cpp -> dax_h\n\nThe graph has a cycle now? 1\n");
	out.clear();
	out.str("");
}

TEST_CASE("boost.graph/works", "Tests that one of the samples from Boost.Graph works as advertised")
{
	using namespace boost;
	typedef adjacency_list<vecS, vecS, bidirectionalS> Graph;
	Graph g(used_by, used_by + nedges, N);
	TestGraph<>(g);
	ModifyGraph<>(g);
}

static int task()
{
#ifdef __GNUC__
	triplegit::async_io::thread::id this_id = boost::this_thread::get_id();
#else
	std::thread::id this_id = std::this_thread::get_id();
#endif
	std::cout << "I am worker thread " << this_id << std::endl;
	return 78;
}
TEST_CASE("async_io/thread_pool/works", "Tests that the async i/o thread pool implementation works")
{
	using namespace triplegit::async_io;
#ifdef __GNUC__
	triplegit::async_io::thread::id this_id = boost::this_thread::get_id();
#else
	std::thread::id this_id = std::this_thread::get_id();
#endif
	std::cout << "I am main thread " << this_id << std::endl;
	thread_pool pool(4);
	auto r=task();
	CHECK(r==78);
	std::vector<future<int>> results(8);
	for(auto &i : results)
	{
		i=std::move(pool.enqueue(task));
	}
	std::vector<future<int>> results2;
	results2.push_back(pool.enqueue(task));
	results2.push_back(pool.enqueue(task));
	std::pair<size_t, int> allresults2=when_any(results2.begin(), results2.end()).get();
	CHECK(allresults2.first<2);
	CHECK(allresults2.second==78);
	std::vector<int> allresults=when_all(results.begin(), results.end()).get();
	for(int i : allresults)
	{
		CHECK(i==78);
	}
}

TEST_CASE("waitaround", "Primes SpeedStep")
{
	typedef std::chrono::duration<double, std::ratio<1>> secs_type;
	auto begin=std::chrono::high_resolution_clock::now();
	while(std::chrono::duration_cast<secs_type>(std::chrono::high_resolution_clock::now()-begin).count()<3);
}

TEST_CASE("async_io/works", "Tests that the async i/o implementation works")
{
	using namespace triplegit::async_io;
	using namespace std;
	using triplegit::async_io::future;
	typedef std::chrono::duration<double, ratio<1>> secs_type;
	auto dispatcher=async_file_io_dispatcher();

	{
		auto mkdir(dispatcher->dir(async_path_op_req("testdir", file_flags::Create)));

		// Start opening 1000 files
		auto begin=chrono::high_resolution_clock::now();
		std::vector<async_path_op_req> manyfilereqs;
		manyfilereqs.reserve(1000);
		for(size_t n=0; n<1000; n++)
			manyfilereqs.push_back(async_path_op_req(mkdir, "testdir/"+std::to_string(n), file_flags::Create|file_flags::Write));
		auto manyopenfiles(dispatcher->file(manyfilereqs));

		// Write one byte to each of those 1000 files as they are opened
		char towrite='N';
		std::vector<async_data_op_req<const char>> manyfilewrites;
		manyfilewrites.reserve(manyfilereqs.size());
		auto openit=manyopenfiles.begin();
		for(size_t n=0; n<manyfilereqs.size(); n++)
			manyfilewrites.push_back(async_data_op_req<const char>(*openit++, &towrite, 1, 0));
		auto manywrittenfiles(dispatcher->write(manyfilewrites));

		// Close each of those 1000 files once one byte has been written
		auto manyclosedfiles(dispatcher->close(manywrittenfiles));

		// Delete each of those 1000 files once they are closed
		auto it(manyclosedfiles.begin());
		for(auto &i : manyfilereqs)
			i.precondition=*it++;
		auto manydeletedfiles(dispatcher->rmfile(manyfilereqs));
		auto dispatched=chrono::high_resolution_clock::now();

		// Wait for all files to open
		when_all(manyopenfiles.begin(), manyopenfiles.end()).wait();
		auto openedsync=chrono::high_resolution_clock::now();
		// Wait for all files to write
		when_all(manywrittenfiles.begin(), manywrittenfiles.end()).wait();
		auto writtensync=chrono::high_resolution_clock::now();
		// Wait for all files to close
		when_all(manyclosedfiles.begin(), manyclosedfiles.end()).wait();
		auto closedsync=chrono::high_resolution_clock::now();
		// Wait for all files to delete
		when_all(manydeletedfiles.begin(), manydeletedfiles.end()).wait();
		auto deletedsync=chrono::high_resolution_clock::now();

		auto end=deletedsync;
		auto rmdir(dispatcher->rmdir(async_path_op_req("testdir")));

		auto diff=chrono::duration_cast<secs_type>(dispatched-begin);
		cout << "It took " << diff.count() << " secs to dispatch all operations" << endl;
		diff=chrono::duration_cast<secs_type>(end-dispatched);
		cout << "It took " << diff.count() << " secs to finish all operations" << endl << endl;

		diff=chrono::duration_cast<secs_type>(openedsync-begin);
		cout << "It took " << diff.count() << " secs to do " << manyfilereqs.size()/diff.count() << " file opens per sec" << endl;
		diff=chrono::duration_cast<secs_type>(openedsync-dispatched);
		cout << "   It took " << diff.count() << " secs to synchronise file opens" << endl;
		diff=chrono::duration_cast<secs_type>(writtensync-openedsync);
		cout << "It took " << diff.count() << " secs to do " << manyfilereqs.size()/diff.count() << " file writes per sec" << endl;
		diff=chrono::duration_cast<secs_type>(closedsync-writtensync);
		cout << "It took " << diff.count() << " secs to do " << manyfilereqs.size()/diff.count() << " file closes per sec" << endl;
		diff=chrono::duration_cast<secs_type>(deletedsync-closedsync);
		cout << "It took " << diff.count() << " secs to do " << manyfilereqs.size()/diff.count() << " file deletions per sec" << endl;

		// Fetch any outstanding error
		rmdir.h.get();
	}
}

TEST_CASE("async_io/errors", "Tests that the async i/o error handling works")
{
	using namespace triplegit::async_io;
	using namespace std;
	using triplegit::async_io::future;

	{
		auto dispatcher=async_file_io_dispatcher();
		auto mkdir(dispatcher->dir(async_path_op_req("testdir", file_flags::Create)));
		{
			vector<async_io_op> files;
			files.push_back(dispatcher->file(async_path_op_req(mkdir, "testdir/a", file_flags::Create)));
			files.push_back(dispatcher->file(async_path_op_req(mkdir, "testdir/b", file_flags::Create)));
			when_all(files.begin(), files.end()).wait();
			auto copy(dispatcher->call(files.front(), []{
				std::filesystem::copy("testdir/c", "testdir/d");
			}));
			CHECK_THROWS(copy.h.get()); // This should trip with failure to copy
			CHECK_THROWS(when_all({copy}).get()); // This should trip with failure to copy
		}
		vector<async_io_op> files;
		files.push_back(dispatcher->rmfile(async_path_op_req(mkdir, "testdir/a")));
		files.push_back(dispatcher->rmfile(async_path_op_req(mkdir, "testdir/b")));
		// TODO: Insert a barrier() here once I write it.
		files.push_back(dispatcher->rmdir(async_path_op_req(mkdir, "testdir")));
	}
}

#if 0
TEST_CASE("triplegit/works", "Tests that one of the samples from Boost.Graph works as advertised with triplegit")
{
	typedef triplegit::boost::adjacency_list<boost::vecS, boost::vecS, boost::bidirectionalS> Graph;
	Graph g(used_by, used_by + nedges, N);
	g.attach(store, testgraph);
	g.begincommit().wait();
	TestGraph<>(g);

	Graph g2(store, testgraph);
	TestGraph<>(g2);

	ModifyGraph<>(g);
	g.begincommit().wait();

	Graph g3(store, testgraph);
	CHECK(boost::isomorphism(g, g3));
}
#endif