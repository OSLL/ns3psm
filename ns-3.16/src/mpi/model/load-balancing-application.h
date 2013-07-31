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

enum LoadBalancingApplicationState { STATIC = 0, DYNAMIC = 1 };

/**
 * Boost graph for easy working with dot format
 * boost::vertex_color_t - vertex load
 * boost::vertex_distance_t - partition
 *
 * boost::edge_weight2_t - edge delay
 */
typedef boost::property < boost::vertex_name_t, uint32_t, boost::property < boost::vertex_color_t, uint32_t, boost::property < boost::vertex_distance_t, uint32_t > > > vertex_p;
typedef boost::property < boost::edge_weight_t, uint32_t, boost::property < boost::edge_weight2_t, int64_t > > edge_p;

typedef boost::adjacency_list <
                    boost::vecS,
                    boost::vecS,
                    boost::undirectedS,
                    vertex_p,
                    edge_p
> boost_graph_t; //boost graph type

typedef boost:: graph_traits< boost_graph_t >::vertex_descriptor vertex_descriptor;
typedef boost::graph_traits< boost_graph_t >::edge_descriptor edge_descriptor;
typedef boost::graph_traits< boost_graph_t >::adjacency_iterator graph_adjacency_iterator;
typedef boost::graph_traits< boost_graph_t >::vertex_iterator graph_vertex_iterator;
typedef boost::graph_traits< boost_graph_t >::edge_iterator graph_edge_iterator;


// redefine types as in diff versions of parmetis used diff types - for easy version change
typedef idx_t parmetis_idx_t;
typedef real_t parmetis_real_t;

/**
 * graph in csr format for working with parmetis library (for graph partition)
 */
struct parmetis_graph_t
{

  // num of vertex (global), num of vertex (local)
  int gnvtxs, nvtxs;

  // physical vertex distribution (for partition computing)
  parmetis_idx_t* vtxdist;

  // bounds for neighbours list adjncy (local)
  // for example [0, 3, 5] means that
  // for local vertex 0 neighbours lying in adjncy[0] - adjncy[2]
  // for local vertex 1 neighbours lying in adjncy[3] - adjncy[4]
  parmetis_idx_t* xadj;

  // list of neighbours, see xadj (local)
  parmetis_idx_t* adjncy;

  // weights of vertex (local)
  parmetis_idx_t* vwgt;

  // weights of vertex (global)
  parmetis_idx_t* gvwgt;

  // weights of edges (local)
  parmetis_idx_t* adjwgt;

  //num of desired parts, equals to num of processes (local)
  parmetis_idx_t nparts;

  // result of partition (local)
  //This is an array of size equal to the number of locally-stored vertices. Upon successful completion the
  //partition vector of the locally-stored vertices is written to this array.
  parmetis_idx_t* part;

  // result of partition (global)
  parmetis_idx_t* part_all;

  // An array of size ncon × nparts that is used to specify the fraction of vertex weight that should
  // be distributed to each sub-domain for each balance constraint. If all of the sub-domains are to be of
  // the same size for every vertex weight, then each of the ncon × nparts elements should be set to
  // a value of 1/nparts. If ncon is greater than 1, the target sub-domain weights for each sub-domain
  // are stored contiguously (similar to the vwgt array). Note that the sum of all of the tpwgts for a
  // give vertex weight should be one.
  parmetis_real_t* tpwgts;

  //This is used to indicate if the graph is weighted. wgtflag can take one of four values:
  // 0 No weights (vwgt and adjwgt are both NULL).
  // 1 Weights on the edges only (vwgt is NULL).
  // 2 Weights on the vertices only (adjwgt is NULL).
  // 3 Weights on both the vertices and edges.
  parmetis_idx_t wgtflag;

  // This is used to indicate the numbering scheme that is used for the vtxdist, xadj, adjncy, and part
  // arrays. numflag can take one of two values:
  // 0 C-style numbering that starts from 0.
  // 1 Fortran-style numbering that starts from 1.
  parmetis_idx_t numflag;

  // This is used to specify the number of weights that each vertex has. It is also the number of balance
  // constraints that must be satisfied.
  parmetis_idx_t ncon;

  // An array of size ncon that is used to specify the imbalance tolerance for each vertex weight, with 1
  // being perfect balance and nparts being perfect imbalance. A value of 1.05 for each of the ncon
  // weights is recommended.
  parmetis_real_t* ubvec;

  // This is an array of integers that is used to pass additional parameters for the routine. The first element
  // (i.e., options[0]) can take either the value of 0 or 1. If it is 0, then the default values are used,
  // otherwise the remaining two elements of options are interpreted as follows:
  //
  // options[1] This specifies the level of information to be returned during the execution of the
  //            algorithm. Timing information can be obtained by setting this to 1. Additional
  //            options for this parameter can be obtained by looking at parmetis.h. The nu-
  //            merical values there should be added to obtain the correct value. The default value
  //            is 0.
  // options[2] This is the random number seed for the routine.
  parmetis_idx_t* options;

  // Upon successful completion, the number of edges that are cut by the partitioning is written to this
  // parameter.
  parmetis_idx_t edgecut;


  // constructor for define defqault values
  parmetis_graph_t()
    {
	  // means Weights on both the vertices and edges.
	  wgtflag = 3;

	  // means C-style numbering that starts from 0
	  numflag = 0;

	  // means number of weights that each vertex has - only num of events used
	  ncon = 1;

	  // standart options
	  options = new parmetis_idx_t[3];
	  options[0] = 1;
	  options[1] = 3;
	  options[2] = 1;

	  // balance for each vertex weight (we have only one weight)
	  ubvec = new parmetis_real_t[1];
	  ubvec[0] = 1.05;
    }
};

class LoadBalancingApplication : public Application {

public:
  static TypeId GetTypeId (void);

  LoadBalancingApplication ();

  virtual ~LoadBalancingApplication();

  void Init (void);
  void SetReclusteringInterval (Time reclusteringInterval);

  LoadBalancingApplicationState GetState() const {
    return m_state;
  }

  void SetState(LoadBalancingApplicationState state) {
    m_state = state;
  }

  // network graph
  parmetis_graph_t m_networkGraph;

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

  // Compute partition at static mode
  void ComputeNetworkGraphPartition (void);

  Time            m_reclusteringInterval;          // reclustering interval
  EventId         m_reclusteringEvent;             // Eventid of pending "clustering" event
  Time            m_lastReclusteringTime;          // Last clustering time
  TypeId          m_tid;

  // map: network node context -> graph vertex description
  //std::map<uint32_t, vertex_descriptor> m_networkGraphVertexMap;

  uint32_t m_iterationNum;

  MPI_Comm m_comm;
  int m_mpiProcessId;
  int m_mpiNumProcesses;

  LoadBalancingApplicationState m_state;
};

} /* namespace ns3 */

#endif /* LOAD_BALANCING_APPLICATION_H_ */
