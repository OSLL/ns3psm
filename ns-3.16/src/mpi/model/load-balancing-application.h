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

#include <boost/graph/use_mpi.hpp>
#include <boost/pending/queue.hpp>
#include <boost/property_map/property_map.hpp>
#include <boost/graph/adjacency_list.hpp>
#include <boost/serialization/version.hpp>
#include <boost/serialization/serialization.hpp>

#include <boost/graph/distributed/mpi_process_group.hpp>
#include <boost/graph/distributed/queue.hpp>

namespace ns3 {

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


struct global_value {

    global_value() {processor = -1;}
    global_value(int processor, graph_t graph) : processor(processor), value(graph) { }

    int processor;
    graph_t value;

    template<class Archiver>
    void serialize(Archiver& ar, const unsigned int) {
      ar & processor & value;
    }
};


struct global_value_owner_map
{
  typedef int value_type;
  typedef value_type reference;
  typedef global_value key_type;
  typedef boost::readable_property_map_tag category;
};


typedef boost::queue<global_value> local_queue_type;
typedef boost::graph::distributed::distributed_queue<
                                            process_group,
                                            global_value_owner_map,
                                            local_queue_type > dist_queue_type;


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

  Time            m_reclusteringInterval;          // reclustering interval
  EventId         m_reclusteringEvent;             // Eventid of pending "clustering" event
  Time            m_lastReclusteringTime;          // Last clustering time
  TypeId          m_tid;

  // network graph
  graph_t m_networkGraph;
  // map: network node context -> graph vertex description
  std::map<uint32_t, vertex_descriptor> m_networkGraphVertexMap;
  // summary cluster load (not used now)
  uint32_t m_clusterLoad;

  uint32_t m_iterationNum;

  process_id_type m_mpiProcessId;
  process_id_type m_mpiNumProcesses;
  dist_queue_type* m_mpiTaskQueue;


private:
  void ScheduleReclusteringEvent ();
};

} /* namespace ns3 */
#endif /* LOAD_BALANCING_APPLICATION_H_ */
