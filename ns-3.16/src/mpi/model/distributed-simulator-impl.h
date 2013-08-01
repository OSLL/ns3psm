/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Author: George Riley <riley@ece.gatech.edu>
 */

#ifndef DISTRIBUTED_SIMULATOR_IMPL_H
#define DISTRIBUTED_SIMULATOR_IMPL_H

#include "ns3/simulator-impl.h"
#include "ns3/scheduler.h"
#include "ns3/event-impl.h"
#include "ns3/ptr.h"

#include <list>

/**
* Added by olya - start
*/
#ifdef NS3_MPI
#include <boost/property_map/property_map.hpp>
#include <boost/graph/adjacency_list.hpp>

#include <boost/serialization/serialization.hpp>
#include <boost/serialization/vector.hpp>

#include <boost/serialization/split_free.hpp>
#include <boost/serialization/nvp.hpp>

#include <parmetis.h>
#endif
/**
* Added by olya - end
*/

namespace ns3 {

/**
 * Added by olya - start
 */

enum DistributedSimulatorState { STATIC = 0, DYNAMIC = 1 };

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


  // constructor for define default values
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

/**
 * Added by olya - end
 */

/**
 * \ingroup mpi
 *
 * \brief Structure used for all-reduce LBTS computation
 */
class LbtsMessage
{
public:
  LbtsMessage ()
    : m_txCount (0),
      m_rxCount (0),
      m_myId (0)
  {
  }

  /**
   * \param rxc received count
   * \param txc transmitted count
   * \param id mpi rank
   * \param t smallest time
   */
  LbtsMessage (uint32_t rxc, uint32_t txc, uint32_t id, const Time& t)
    : m_txCount (txc),
      m_rxCount (rxc),
      m_myId (id),
      m_smallestTime (t)
  {
  }

  ~LbtsMessage ();

  /**
   * \return smallest time
   */
  Time GetSmallestTime ();
  /**
   * \return transmitted count
   */
  uint32_t GetTxCount ();
  /**
   * \return receieved count
   */
  uint32_t GetRxCount ();
  /**
   * \return id which corresponds to mpi rank
   */
  uint32_t GetMyId ();

private:
  uint32_t m_txCount;
  uint32_t m_rxCount;
  uint32_t m_myId;
  Time     m_smallestTime;
};

/**
 * \ingroup mpi
 *
 * \brief distributed simulator implementation using lookahead
 */
class DistributedSimulatorImpl : public SimulatorImpl
{
public:
  static TypeId GetTypeId (void);

  DistributedSimulatorImpl ();
  ~DistributedSimulatorImpl ();

  // virtual from SimulatorImpl
  virtual void Destroy ();
  virtual bool IsFinished (void) const;
  virtual void Stop (void);
  virtual void Stop (Time const &time);
  virtual EventId Schedule (Time const &time, EventImpl *event);
  virtual void ScheduleWithContext (uint32_t context, Time const &time, EventImpl *event);
  virtual EventId ScheduleNow (EventImpl *event);
  virtual EventId ScheduleDestroy (EventImpl *event);
  virtual void Remove (const EventId &ev);
  virtual void Cancel (const EventId &ev);
  virtual bool IsExpired (const EventId &ev) const;
  virtual void Run (void);
  virtual Time Now (void) const;
  virtual Time GetDelayLeft (const EventId &id) const;
  virtual Time GetMaximumSimulationTime (void) const;
  virtual void SetScheduler (ObjectFactory schedulerFactory);
  virtual uint32_t GetSystemId (void) const;
  virtual uint32_t GetContext (void) const;

private:
  virtual void DoDispose (void);
  void CalculateLookAhead (void);

  void ProcessOneEvent (void);
  uint64_t NextTs (void) const;
  Time Next (void) const;

  /**
   * Added by olya - start
   */

#ifdef NS3_MPI
  // Event handlers
  void StartDynamicReclustering (void);
  void StartStaticReclustering (void);

  // Recompute clustering
  void Reclustering (void);

  // Create network graph before run
  void CreateNetworkGraph (void);

  // Update network graph in .dot format with nodes load and edge traffic
  void UpdateNetworkGraph (void);

  // Write network graph in .dot format
  void WriteClusterGraph (const std::string& filename);

  // Compute partition at the end (static mode)
  void ComputeNetworkGraphPartition (void);
#endif

  /**
   * Added by olya - end
   */

  typedef std::list<EventId> DestroyEvents;

  DestroyEvents m_destroyEvents;
  bool m_stop;
  Ptr<Scheduler> m_events;
  uint32_t m_uid;
  uint32_t m_currentUid;
  uint64_t m_currentTs;
  uint32_t m_currentContext;
  // number of events that have been inserted but not yet scheduled,
  // not counting the "destroy" events; this is used for validation
  int m_unscheduledEvents;

  LbtsMessage* m_pLBTS;       // Allocated once we know how many systems
  uint32_t     m_myId;        // MPI Rank
  uint32_t     m_systemCount; // MPI Size
  Time         m_grantedTime; // Last LBTS
  static Time  m_lookAhead;   // Lookahead value


  /**
   * Added by olya - start
   */

#ifdef NS3_MPI
  // reclustering interval
  Time m_reclusteringInterval;

  // network graph
  parmetis_graph_t m_networkGraph;

  // repartition iteration number - for dynamic static
  uint32_t m_iterationNum;

  // for easy using parmetis
  MPI_Comm m_comm;
  int m_mpiProcessId;
  int m_mpiNumProcesses;
#endif

  // state: static or dynamic
  DistributedSimulatorState m_state;

  /**
   * Added by olya - end
   */

};

} // namespace ns3

#endif /* DISTRIBUTED_SIMULATOR_IMPL_H */
