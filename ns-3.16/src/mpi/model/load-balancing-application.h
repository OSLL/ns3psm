/*
 * load-balancing-application.h
 *
 *  Created on: May 2, 2013
 *      Author: olya
 */

#ifndef LOAD_BALANCING_APPLICATION_H_
#define LOAD_BALANCING_APPLICATION_H_

#include "ns3/application.h"
#include "ns3/event-id.h"

#ifdef NS3_MPI

#include <boost/graph/use_mpi.hpp>
#include <boost/pending/queue.hpp>
#include <boost/graph/distributed/mpi_process_group.hpp>
#include <boost/graph/distributed/queue.hpp>
#include <boost/property_map/property_map.hpp>
#include <boost/graph/adjacency_list.hpp>
#include <boost/serialization/version.hpp>
#include <boost/serialization/serialization.hpp>

#endif

namespace ns3 {

#ifdef NS3_MPI
// vertex_name - for reading from dot
// vertex_color - load
// vertex_distance - # cluster node
typedef boost::property < boost::vertex_name_t, uint32_t, boost::property < boost::vertex_color_t, uint32_t, boost::property < boost::vertex_distance_t, uint32_t > > > vertex_p;
// edge_weight - traffic
// edge_weight2 - delay
typedef boost::property < boost::edge_weight_t, double, boost::property < boost::edge_weight2_t, int64_t > > edge_p;

typedef boost::adjacency_list <
                    boost::vecS,
                    boost::vecS,
                    boost::undirectedS,
                    vertex_p,
                    edge_p
> graph_t; //graph type

typedef boost::graph_traits< graph_t >::vertex_descriptor vertex_descriptor;
typedef boost::graph_traits< graph_t >::edge_descriptor edge_descriptor;
typedef boost::graph_traits< graph_t >::adjacency_iterator graph_adjacency_iterator;
typedef boost::graph_traits< graph_t >::vertex_iterator graph_vertex_iterator;
typedef boost::graph_traits< graph_t >::edge_iterator graph_edge_iterator;

typedef boost::graph::distributed::mpi_process_group process_group;
typedef boost::graph::distributed::mpi_process_group::process_id_type process_id_type;

struct node_for_moving_t
{
  uint32_t context;
  uint32_t old_cluster;
  uint32_t new_cluster;
};

struct apllications_for_moving_t
{
  uint32_t context;
  std::vector< std::string > appliations;
};

struct global_value_graph {

    global_value_graph() {processor = -1;}
    global_value_graph(int processor, graph_t graph) : processor(processor), value(graph) { }

    int processor;
    graph_t value;

    template<class Archiver>
    void serialize(Archiver& ar, const unsigned int) {
      ar & processor & value;
    }
};

struct global_value_node {

    global_value_node() {processor = -1;}
    global_value_node(int processor, node_for_moving_t node) : processor(processor), value(node) { }

    int processor;
    node_for_moving_t value;

    template<class Archiver>
    void serialize(Archiver& ar, const unsigned int) {
      ar & processor & value.context & value.old_cluster & value.new_cluster;
    }
};

struct global_value_applications {

    global_value_applications() {processor = -1;}
    global_value_applications(int processor, apllications_for_moving_t applications) : processor(processor), value(applications) { }

    int processor;
    apllications_for_moving_t value;

    template<class Archiver>
    void serialize(Archiver& ar, const unsigned int) {
      ar & processor & value.context & value.appliations;
    }
};

struct global_value_owner_map_graph
{
  typedef int value_type;
  typedef value_type reference;
  typedef global_value_graph key_type;
  typedef boost::readable_property_map_tag category;
};

struct global_value_owner_map_node
{
  typedef int value_type;
  typedef value_type reference;
  typedef global_value_node key_type;
  typedef boost::readable_property_map_tag category;
};

struct global_value_owner_map_applications
{
  typedef int value_type;
  typedef value_type reference;
  typedef global_value_applications key_type;
  typedef boost::readable_property_map_tag category;
};


typedef boost::queue<global_value_graph> local_queue_graph_t;
typedef boost::graph::distributed::distributed_queue<
                                            process_group,
                                            global_value_owner_map_graph,
                                            local_queue_graph_t > dist_queue_graph_t;

typedef boost::queue<global_value_node> local_queue_node_t;
typedef boost::graph::distributed::distributed_queue<
                                            process_group,
                                            global_value_owner_map_node,
                                            local_queue_node_t> dist_queue_node_t;

typedef boost::queue<global_value_applications> local_queue_applications_t;
typedef boost::graph::distributed::distributed_queue<
                                            process_group,
                                            global_value_owner_map_applications,
                                            local_queue_applications_t> dist_queue_applications_t;



#endif

class LoadBalancingApplication : public Application {

public:
  static TypeId GetTypeId (void);

  LoadBalancingApplication ();

  virtual ~LoadBalancingApplication();

  void Init (void);
  void SetReclusteringInterval (Time reclusteringInterval);
  void IncNodeLoad (uint32_t context);

protected:
  virtual void DoDispose (void);

private:
  // inherited from Application base class.
  virtual void StartApplication (void);    // Called at time specified by Start
  virtual void StopApplication (void);     // Called at time specified by Stop


  //helpers
  void CancelEvents ();

  // Event handlers
  void StartReclustering (void);
  void StartCreateNetworkGraph (void);
  void StartMergeNetwork (void);
  void StartUpdateNetworkGraph (void);
  void StartWriteNetworkGraph (void);

  // Recompute clustering
  void Reclustering (void);
  // Create network graph before run
  void CreateNetworkGraph (void);
  // Merge weighted graphs from all cluster nodes
  void MergeNetworkGraph (void);
  // Update network graph in .dot format with nodes load and edge traffic
  void UpdateNetworkGraph (void);
  // Write network graph in .dot format
  void WriteNetworkGraph (const std::string& filename);
  // Clustering graph with lso
  void ClusterNetworkGraph ();
  // Write network graph in .dot format
  void WriteClusterGraph (const std::string& filename);

  Time     m_reclusteringInterval;          // reclustering interval
  uint32_t m_clusterLoad;
  uint32_t m_iterationNum;
  TypeId   m_tid;

  #ifdef NS3_MPI

  EventId         m_reclusteringEvent;             // Eventid of pending "clustering" event
  Time            m_lastReclusteringTime;          // Last clustering time

  // network graph
  graph_t m_networkGraph;
  // map: network node context -> graph vertex description
  std::map<uint32_t, vertex_descriptor> m_networkGraphVertexMap;
  // summary cluster load (not used now)

  process_id_type m_mpiProcessId;
  process_id_type m_mpiNumProcesses;
  process_group m_mpiProcessGroup;
  dist_queue_graph_t* m_mpiGraphQueue;
  dist_queue_node_t* m_mpiNodeQueue;
  dist_queue_applications_t* m_mpiApplicationsQueue;

  #endif

private:
  void ScheduleReclusteringEvent ();

};

} /* namespace ns3 */
#endif /* LOAD_BALANCING_APPLICATION_H_ */
