/* Unit tests for TripleGit
(C) 2013 Niall Douglas http://www.nedprod.com/
Created: Feb 2013
*/

#define CATCH_CONFIG_RUNNER
#include "catch.hpp"
#include <utility>
#include <sstream>
#include "../triplegit/src/std_filesystem.hpp"
#include "../triplegit/include/triplegit.hpp"
#include "boost/graph/topological_sort.hpp"
#include "boost/graph/depth_first_search.hpp"
#include "boost/graph/dijkstra_shortest_paths.hpp"
#include "boost/graph/visitors.hpp"
#include "boost/graph/isomorphism.hpp"

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

static triplegit::fs_store store(std::filesystem::current_path());
static triplegit::collection_id testgraph(store, "unittests.testgraph");

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
	typedef graph_traits<Graph>::vertex_descriptor Vertex;
	ostringstream out;

  // Determine ordering for a full recompilation
  // and the order with files that can be compiled in parallel
  {
    typedef list<Vertex> MakeOrder;
    MakeOrder::iterator i;
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
        Graph::in_edge_iterator j, j_end;
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
      graph_traits<Graph>::vertex_iterator i, iend;
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
	typedef graph_traits<Graph>::vertex_descriptor Vertex;
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
