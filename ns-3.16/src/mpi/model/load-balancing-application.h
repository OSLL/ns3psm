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
#include "ns3/onoff-application.h"

#include <boost/property_map/property_map.hpp>
#include <boost/graph/adjacency_list.hpp>

#include <boost/serialization/serialization.hpp>
#include <boost/serialization/vector.hpp>

#include <boost/serialization/split_free.hpp>
#include <boost/serialization/nvp.hpp>

#include <parmetis.h>

namespace ns3 {

typedef boost::property < boost::vertex_name_t, uint32_t, boost::property < boost::vertex_color_t, uint32_t, boost::property < boost::vertex_distance_t, uint32_t > > > vertex_p;
typedef boost::property < boost::edge_weight_t, uint32_t, boost::property < boost::edge_weight2_t, int64_t > > edge_p;

typedef boost::adjacency_list <
                    boost::vecS,
                    boost::vecS,
                    boost::undirectedS,
                    vertex_p,
                    edge_p
> graph_t2; //graph type

typedef boost:: graph_traits<  graph_t2 >::vertex_descriptor vertex_descriptor;
typedef boost::graph_traits<  graph_t2 >::edge_descriptor edge_descriptor;
typedef boost::graph_traits<  graph_t2 >::adjacency_iterator graph_adjacency_iterator;
typedef boost::graph_traits<  graph_t2 >::vertex_iterator graph_vertex_iterator;
typedef boost::graph_traits<  graph_t2 >::edge_iterator graph_edge_iterator;


typedef idx_t parmetis_idx_t;
typedef real_t parmetis_real_t;

struct graph_t
{
  int gnvtxs, nvtxs;
  parmetis_idx_t* vtxdist;
  parmetis_idx_t* xadj;
  parmetis_idx_t* adjncy;
  parmetis_idx_t* vwgt;
  parmetis_idx_t* adjwgt;
  parmetis_idx_t nparts;
  parmetis_real_t* tpwgts;
  parmetis_idx_t* part;
  parmetis_idx_t* part_all;

  parmetis_idx_t wgtflag;
  parmetis_idx_t numflag;
  parmetis_idx_t ncon;
  parmetis_real_t* ubvec;
  parmetis_idx_t* options;
  parmetis_idx_t edgecut;


  graph_t()
    {
	  wgtflag = 3;
	  numflag = 0;
	  ncon = 1;

	  options = new parmetis_idx_t[3];
	  options[0] = 1;
	  options[1] = 3;
	  options[2] = 1;

	  ubvec = new parmetis_real_t[1];
	  ubvec[0] = 1.05;

	  edgecut = 0;

    }
};

class LoadBalancingApplication : public Application {

public:
  static TypeId GetTypeId (void);

  LoadBalancingApplication ();

  virtual ~LoadBalancingApplication();

  void Init (void);
  void SetReclusteringInterval (Time reclusteringInterval);

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

  // Recompute clustering
  void Reclustering (void);
  // Create network graph before run
  void CreateNetworkGraph (void);
  // Update network graph in .dot format with nodes load and edge traffic
  void UpdateNetworkGraph (void);
  // Write network graph in .dot format
  void WriteClusterGraph (const std::string& filename);

  Time            m_reclusteringInterval;          // reclustering interval
  EventId         m_reclusteringEvent;             // Eventid of pending "clustering" event
  Time            m_lastReclusteringTime;          // Last clustering time
  TypeId          m_tid;

  // network graph
  graph_t m_networkGraph;
  // map: network node context -> graph vertex description
  //std::map<uint32_t, vertex_descriptor> m_networkGraphVertexMap;

  uint32_t m_iterationNum;

  int m_mpiProcessId;
  int m_mpiNumProcesses;

private:
  void ScheduleReclusteringEvent ();
};

} /* namespace ns3 */

#endif /* LOAD_BALANCING_APPLICATION_H_ */
