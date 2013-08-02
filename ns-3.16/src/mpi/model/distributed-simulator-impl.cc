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

#include "distributed-simulator-impl.h"
#include "mpi-interface.h"

#include "ns3/simulator.h"
#include "ns3/scheduler.h"
#include "ns3/event-impl.h"
#include "ns3/channel.h"
#include "ns3/node-container.h"
#include "ns3/ptr.h"
#include "ns3/pointer.h"
#include "ns3/assert.h"
#include "ns3/log.h"


  /**
   * Added by olya - start
   */
#ifdef NS3_MPI
#include "ns3/application.h"
#include "ns3/channel.h"
#include "ns3/node-container.h"
#include "ns3/node-list.h"

#include <boost/graph/graphviz.hpp>
#include <boost/graph/adj_list_serialize.hpp>

#include <boost/algorithm/string/split.hpp>
#include <boost/algorithm/string/classification.hpp>
#endif
  /**
   * Added by olya - end
   */

#include <cmath>

#ifdef NS3_MPI
#include <mpi.h>
#endif

NS_LOG_COMPONENT_DEFINE ("DistributedSimulatorImpl");

namespace ns3 {

NS_OBJECT_ENSURE_REGISTERED (DistributedSimulatorImpl);

LbtsMessage::~LbtsMessage ()
{
}

Time
LbtsMessage::GetSmallestTime ()
{
  return m_smallestTime;
}

uint32_t
LbtsMessage::GetTxCount ()
{
  return m_txCount;
}

uint32_t
LbtsMessage::GetRxCount ()
{
  return m_rxCount;
}
uint32_t
LbtsMessage::GetMyId ()
{
  return m_myId;
}

Time DistributedSimulatorImpl::m_lookAhead = Seconds (0);

TypeId
DistributedSimulatorImpl::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::DistributedSimulatorImpl")
    .SetParent<Object> ()
    .AddConstructor<DistributedSimulatorImpl> ()
  ;
  return tid;
}

DistributedSimulatorImpl::DistributedSimulatorImpl ()
{
#ifdef NS3_MPI
  m_myId = MpiInterface::GetSystemId ();
  m_systemCount = MpiInterface::GetSize ();

  // Allocate the LBTS message buffer
  m_pLBTS = new LbtsMessage[m_systemCount];
  m_grantedTime = Seconds (0);

  /**
   * Added by olya - start
   */
#ifdef NS3_MPI
  m_state = STATIC;
  m_reclusteringInterval = Seconds (20),
  m_iterationNum = 0;

  MPI_Comm_size(MPI_COMM_WORLD, &m_mpiNumProcesses);
  MPI_Comm_rank(MPI_COMM_WORLD, &m_mpiProcessId);
  MPI_Comm_dup(MPI_COMM_WORLD, &m_comm);
#endif
  /**
   * Added by olya - end
   */

#else
  NS_FATAL_ERROR ("Can't use distributed simulator without MPI compiled in");
#endif

  m_stop = false;
  // uids are allocated from 4.
  // uid 0 is "invalid" events
  // uid 1 is "now" events
  // uid 2 is "destroy" events
  m_uid = 4;
  // before ::Run is entered, the m_currentUid will be zero
  m_currentUid = 0;
  m_currentTs = 0;
  m_currentContext = 0xffffffff;
  m_unscheduledEvents = 0;
  m_events = 0;
}

DistributedSimulatorImpl::~DistributedSimulatorImpl ()
{
}

void
DistributedSimulatorImpl::DoDispose (void)
{
  while (!m_events->IsEmpty ())
    {
      Scheduler::Event next = m_events->RemoveNext ();
      next.impl->Unref ();
    }
  m_events = 0;
  delete [] m_pLBTS;
  SimulatorImpl::DoDispose ();
}

void
DistributedSimulatorImpl::Destroy ()
{
  while (!m_destroyEvents.empty ())
    {
      Ptr<EventImpl> ev = m_destroyEvents.front ().PeekEventImpl ();
      m_destroyEvents.pop_front ();
      NS_LOG_LOGIC ("handle destroy " << ev);
      if (!ev->IsCancelled ())
        {
          ev->Invoke ();
        }
    }

  MpiInterface::Destroy ();
}


void
DistributedSimulatorImpl::CalculateLookAhead (void)
{
#ifdef NS3_MPI
  if (MpiInterface::GetSize () <= 1)
    {
      DistributedSimulatorImpl::m_lookAhead = Seconds (0);
      m_grantedTime = Seconds (0);
    }
  else
    {
      NodeContainer c = NodeContainer::GetGlobal ();
      for (NodeContainer::Iterator iter = c.Begin (); iter != c.End (); ++iter)
        {
          if ((*iter)->GetSystemId () != MpiInterface::GetSystemId ())
            {
              continue;
            }

          for (size_t i = 0; i < (*iter)->GetNDevices (); ++i)
            {
              Ptr<NetDevice> localNetDevice = (*iter)->GetDevice (i);
              // only works for p2p links currently
              if (!localNetDevice->IsPointToPoint ())
                {
                  continue;
                }
              Ptr<Channel> channel = localNetDevice->GetChannel ();
              if (channel == 0)
                {
                  continue;
                }

              // grab the adjacent node
              Ptr<Node> remoteNode;
              if (channel->GetDevice (0) == localNetDevice)
                {
                  remoteNode = (channel->GetDevice (1))->GetNode ();
                }
              else
                {
                  remoteNode = (channel->GetDevice (0))->GetNode ();
                }

              // if it's not remote, don't consider it
              if (remoteNode->GetSystemId () == MpiInterface::GetSystemId ())
                {
                  continue;
                }

              // compare delay on the channel with current value of
              // m_lookAhead.  if delay on channel is smaller, make
              // it the new lookAhead.
              TimeValue delay;
              channel->GetAttribute ("Delay", delay);
              if (DistributedSimulatorImpl::m_lookAhead.IsZero ())
                {
                  DistributedSimulatorImpl::m_lookAhead = delay.Get ();
                  m_grantedTime = delay.Get ();
                }
              if (delay.Get ().GetSeconds () < DistributedSimulatorImpl::m_lookAhead.GetSeconds ())
                {
                  DistributedSimulatorImpl::m_lookAhead = delay.Get ();
                  m_grantedTime = delay.Get ();
                }
            }
        }
    }
#else
  NS_FATAL_ERROR ("Can't use distributed simulator without MPI compiled in");
#endif
}

void
DistributedSimulatorImpl::SetScheduler (ObjectFactory schedulerFactory)
{
  Ptr<Scheduler> scheduler = schedulerFactory.Create<Scheduler> ();

  if (m_events != 0)
    {
      while (!m_events->IsEmpty ())
        {
          Scheduler::Event next = m_events->RemoveNext ();
          scheduler->Insert (next);
        }
    }
  m_events = scheduler;
}

void
DistributedSimulatorImpl::ProcessOneEvent (void)
{
  Scheduler::Event next = m_events->RemoveNext ();

  NS_ASSERT (next.key.m_ts >= m_currentTs);
  m_unscheduledEvents--;

  NS_LOG_LOGIC ("handle " << next.key.m_ts);
  m_currentTs = next.key.m_ts;
  m_currentContext = next.key.m_context;
  m_currentUid = next.key.m_uid;

  /**
  * Added by olya - start
  */
#ifdef NS3_MPI
  if ((m_currentContext >= 0) && (m_currentContext < m_networkGraph.gnvtxs))
  {
      m_networkGraph.gvwgt[m_currentContext]++;
  }
#endif
  /**
  * Added by olya - end
  */
  next.impl->Invoke ();
  next.impl->Unref ();
}

bool
DistributedSimulatorImpl::IsFinished (void) const
{
  return m_events->IsEmpty () || m_stop;
}

uint64_t
DistributedSimulatorImpl::NextTs (void) const
{
  NS_ASSERT (!m_events->IsEmpty ());
  Scheduler::Event ev = m_events->PeekNext ();
  return ev.key.m_ts;
}

Time
DistributedSimulatorImpl::Next (void) const
{
  return TimeStep (NextTs ());
}

void
DistributedSimulatorImpl::Run (void)
{
#ifdef NS3_MPI

  /**
  * Added by olya - start
  */
#ifdef NS3_MPI
  // create network graph
  CreateNetworkGraph ();

  // start reclustering
  if (m_state == DYNAMIC) {
    Simulator::Schedule (m_reclusteringInterval, &DistributedSimulatorImpl::StartDynamicReclustering, this);
  }

  if (m_state == STATIC) {
    Simulator::ScheduleDestroy(&DistributedSimulatorImpl::StartStaticReclustering, this);
  }
#endif
  /**
  * Added by olya - end
  */

  CalculateLookAhead ();
  m_stop = false;
  while (!m_events->IsEmpty () && !m_stop)
    {
      Time nextTime = Next ();
      if (nextTime > m_grantedTime)
        { // Can't process, calculate a new LBTS
          // First receive any pending messages
          MpiInterface::ReceiveMessages ();
          // reset next time
          nextTime = Next ();
          // And check for send completes
          MpiInterface::TestSendComplete ();
          // Finally calculate the lbts
          LbtsMessage lMsg (MpiInterface::GetRxCount (), MpiInterface::GetTxCount (), m_myId, nextTime);
          m_pLBTS[m_myId] = lMsg;
          MPI_Allgather (&lMsg, sizeof (LbtsMessage), MPI_BYTE, m_pLBTS,
                         sizeof (LbtsMessage), MPI_BYTE, MPI_COMM_WORLD);
          Time smallestTime = m_pLBTS[0].GetSmallestTime ();
          // The totRx and totTx counts insure there are no transient
          // messages;  If totRx != totTx, there are transients,
          // so we don't update the granted time.
          uint32_t totRx = m_pLBTS[0].GetRxCount ();
          uint32_t totTx = m_pLBTS[0].GetTxCount ();

          for (uint32_t i = 1; i < m_systemCount; ++i)
            {
              if (m_pLBTS[i].GetSmallestTime () < smallestTime)
                {
                  smallestTime = m_pLBTS[i].GetSmallestTime ();
                }
              totRx += m_pLBTS[i].GetRxCount ();
              totTx += m_pLBTS[i].GetTxCount ();

            }
          if (totRx == totTx)
            {
              m_grantedTime = smallestTime + DistributedSimulatorImpl::m_lookAhead;
            }
        }
      if (nextTime <= m_grantedTime)
        { // Save to process
          ProcessOneEvent ();
        }
    }

  // If the simulator stopped naturally by lack of events, make a
  // consistency test to check that we didn't lose any events along the way.
  NS_ASSERT (!m_events->IsEmpty () || m_unscheduledEvents == 0);
#else
  NS_FATAL_ERROR ("Can't use distributed simulator without MPI compiled in");
#endif
}

uint32_t DistributedSimulatorImpl::GetSystemId () const
{
  return m_myId;
}

void
DistributedSimulatorImpl::Stop (void)
{
  m_stop = true;
}

void
DistributedSimulatorImpl::Stop (Time const &time)
{
  Simulator::Schedule (time, &Simulator::Stop);
}

//
// Schedule an event for a _relative_ time in the future.
//
EventId
DistributedSimulatorImpl::Schedule (Time const &time, EventImpl *event)
{
  Time tAbsolute = time + TimeStep (m_currentTs);

  NS_ASSERT (tAbsolute.IsPositive ());
  NS_ASSERT (tAbsolute >= TimeStep (m_currentTs));
  Scheduler::Event ev;
  ev.impl = event;
  ev.key.m_ts = static_cast<uint64_t> (tAbsolute.GetTimeStep ());
  ev.key.m_context = GetContext ();
  ev.key.m_uid = m_uid;
  m_uid++;
  m_unscheduledEvents++;
  m_events->Insert (ev);
  return EventId (event, ev.key.m_ts, ev.key.m_context, ev.key.m_uid);
}

void
DistributedSimulatorImpl::ScheduleWithContext (uint32_t context, Time const &time, EventImpl *event)
{
  NS_LOG_FUNCTION (this << context << time.GetTimeStep () << m_currentTs << event);

  Scheduler::Event ev;
  ev.impl = event;
  ev.key.m_ts = m_currentTs + time.GetTimeStep ();
  ev.key.m_context = context;
  ev.key.m_uid = m_uid;
  m_uid++;
  m_unscheduledEvents++;
  m_events->Insert (ev);
}

EventId
DistributedSimulatorImpl::ScheduleNow (EventImpl *event)
{
  Scheduler::Event ev;
  ev.impl = event;
  ev.key.m_ts = m_currentTs;
  ev.key.m_context = GetContext ();
  ev.key.m_uid = m_uid;
  m_uid++;
  m_unscheduledEvents++;
  m_events->Insert (ev);
  return EventId (event, ev.key.m_ts, ev.key.m_context, ev.key.m_uid);
}

EventId
DistributedSimulatorImpl::ScheduleDestroy (EventImpl *event)
{
  EventId id (Ptr<EventImpl> (event, false), m_currentTs, 0xffffffff, 2);
  m_destroyEvents.push_back (id);
  m_uid++;
  return id;
}

Time
DistributedSimulatorImpl::Now (void) const
{
  return TimeStep (m_currentTs);
}

Time
DistributedSimulatorImpl::GetDelayLeft (const EventId &id) const
{
  if (IsExpired (id))
    {
      return TimeStep (0);
    }
  else
    {
      return TimeStep (id.GetTs () - m_currentTs);
    }
}

void
DistributedSimulatorImpl::Remove (const EventId &id)
{
  if (id.GetUid () == 2)
    {
      // destroy events.
      for (DestroyEvents::iterator i = m_destroyEvents.begin (); i != m_destroyEvents.end (); i++)
        {
          if (*i == id)
            {
              m_destroyEvents.erase (i);
              break;
            }
        }
      return;
    }
  if (IsExpired (id))
    {
      return;
    }
  Scheduler::Event event;
  event.impl = id.PeekEventImpl ();
  event.key.m_ts = id.GetTs ();
  event.key.m_context = id.GetContext ();
  event.key.m_uid = id.GetUid ();
  m_events->Remove (event);
  event.impl->Cancel ();
  // whenever we remove an event from the event list, we have to unref it.
  event.impl->Unref ();

  m_unscheduledEvents--;
}

void
DistributedSimulatorImpl::Cancel (const EventId &id)
{
  if (!IsExpired (id))
    {
      id.PeekEventImpl ()->Cancel ();
    }
}

bool
DistributedSimulatorImpl::IsExpired (const EventId &ev) const
{
  if (ev.GetUid () == 2)
    {
      if (ev.PeekEventImpl () == 0
          || ev.PeekEventImpl ()->IsCancelled ())
        {
          return true;
        }
      // destroy events.
      for (DestroyEvents::const_iterator i = m_destroyEvents.begin (); i != m_destroyEvents.end (); i++)
        {
          if (*i == ev)
            {
              return false;
            }
        }
      return true;
    }
  if (ev.PeekEventImpl () == 0
      || ev.GetTs () < m_currentTs
      || (ev.GetTs () == m_currentTs
          && ev.GetUid () <= m_currentUid)
      || ev.PeekEventImpl ()->IsCancelled ())
    {
      return true;
    }
  else
    {
      return false;
    }
}

Time
DistributedSimulatorImpl::GetMaximumSimulationTime (void) const
{
  // XXX: I am fairly certain other compilers use other non-standard
  // post-fixes to indicate 64 bit constants.
  return TimeStep (0x7fffffffffffffffLL);
}

uint32_t
DistributedSimulatorImpl::GetContext (void) const
{
  return m_currentContext;
}



/**
 * Added by olya - start
 */
#ifdef NS3_MPI

void
DistributedSimulatorImpl::Reclustering ()
{
  std::cerr << "Reclustering start... " << std::endl;

  // update local weight on vertex
  UpdateNetworkGraph ();

  // call parmetis clustering func
  MPI_Status stat;

  ParMETIS_V3_RefineKway(m_networkGraph.vtxdist, m_networkGraph.xadj, m_networkGraph.adjncy, m_networkGraph.vwgt,
    m_networkGraph.adjwgt, &m_networkGraph.wgtflag, &m_networkGraph.numflag, &m_networkGraph.ncon,
    &m_networkGraph.nparts, m_networkGraph.tpwgts, m_networkGraph.ubvec, m_networkGraph.options,
    &m_networkGraph.edgecut, m_networkGraph.part, &m_comm);

  // synchronize processes
  MPI_Barrier (MPI_COMM_WORLD);

  // send partition results to other processes
  for (int i = 0; i < m_mpiNumProcesses; i++){
    if (i != m_mpiProcessId) {
      MPI_Send((void *)m_networkGraph.part, m_networkGraph.nvtxs, MPI_SHORT, i, 123, MPI_COMM_WORLD);
    }
  }

  // replace results from own m_networkGraph.part into global m_networkGraph.part_all
  for (size_t i = 0; i < m_networkGraph.nvtxs; i++) {
    m_networkGraph.part_all[m_networkGraph.vtxdist[m_mpiProcessId] + i] = m_networkGraph.part[i];
  }

  // get partition results from other processes
  for (int i = 0; i < m_mpiNumProcesses; i++){
    if (i != m_mpiProcessId) {
      MPI_Recv((void *)&m_networkGraph.part_all[m_networkGraph.vtxdist[i]],
         m_networkGraph.vtxdist[i + 1] - m_networkGraph.vtxdist[i], MPI_SHORT, i, 123, MPI_COMM_WORLD, &stat);
    }
  }

  // applications exchange - only on dynamic state
  if (m_state == DYNAMIC) {

    // for each vertex
    for (size_t i = 0; i < m_networkGraph.gnvtxs; ++i) {

    	// find vertex that must goes to another process
      if (((int)NodeList::GetNode (i)->GetSystemId() == m_mpiProcessId) && (m_networkGraph.part_all[i] != m_mpiProcessId)) {

        // this process send information about applications, running on this node to new process
        Ptr<Node> nodeForMoving = NodeList::GetNode (i);

        // todo now only application name sending - need for serialization
        std::string applications;
        std::vector<Ptr<Application> > nodeApplications = nodeForMoving->GetApplications ();

        for (size_t j = 0; j < nodeApplications.size (); ++j)
        {
          // save application type
          applications.append(nodeApplications[j]->GetInstanceTypeId ().GetName ());
          applications.append(" ");
          nodeApplications[j]-> SetStopTime(Simulator::Now());
        }

        unsigned int  app_size = applications.size();
        MPI_Send((void *)(&app_size), 1, MPI_UNSIGNED, m_networkGraph.part_all[i], i + 123, MPI_COMM_WORLD);
        if (app_size > 0) MPI_Send((void *)applications.c_str(), applications.size(), MPI_CHAR, m_networkGraph.part_all[i], i + 124, MPI_COMM_WORLD);
      }
    }

    // synchronize processes
    MPI_Barrier (MPI_COMM_WORLD);

    // for each vertex
    for (size_t i = 0; i < m_networkGraph.gnvtxs; ++i) {

      // find vertex that must goes to this process
      if ((m_networkGraph.part_all[i] == m_mpiProcessId) && ((int)NodeList::GetNode (i)->GetSystemId() != m_mpiProcessId)) {

        // this process get information about applications, running on this node on old process
        Ptr<Node> nodeForMoving = NodeList::GetNode (i);

        unsigned int applicationsNum;

        MPI_Recv((void *)&applicationsNum, 1, MPI_UNSIGNED, (int)NodeList::GetNode (i)->GetSystemId(), i + 123, MPI_COMM_WORLD, &stat);
        if (applicationsNum > 0)
          {

            char* applications = new char[applicationsNum];

            MPI_Recv((void *)applications, applicationsNum, MPI_CHAR, (int)NodeList::GetNode (i)->GetSystemId(), i + 124, MPI_COMM_WORLD, &stat);

            std::string applicationsString(applications);
            std::vector <std::string> nodeApplications;
            boost::algorithm::split(nodeApplications, applicationsString, boost::algorithm::is_any_of(" "));
            nodeForMoving-> SetSystemId (m_networkGraph.part_all[i]);

            // start applications
            for (size_t j = 0; j < nodeApplications.size (); ++j)
              {
                // create application by type
                ObjectFactory objectFactory;
                objectFactory.SetTypeId (TypeId::LookupByName (nodeApplications[j]) );
                Ptr<Application> application = objectFactory.Create<Application> ();
                // start application
                application-> SetStartTime (Simulator::Now());
                application-> Start ();
                nodeForMoving->AddApplication (application);
              }

          } // if (applicationsNum > 0)

      } // find vertex that must goes to this process

    } // for each vertex


  } // if dynamic

  std::cerr << "Reclustering finished" << std::endl;
}


void
DistributedSimulatorImpl::CreateNetworkGraph (void)
{
  std::cerr << "Graph creation start... " << std::endl;

  NodeContainer node_container =  NodeContainer::GetGlobal ();

  // set global num of vertex
  m_networkGraph.gnvtxs = node_container.GetN ();

  // init array for physical vertex distribution (for partition computing)
  m_networkGraph.vtxdist = new parmetis_idx_t [m_mpiNumProcesses + 1];

  // fill array for physical vertex distribution (for partition computing)
  // each physical process get the same num of vertex
  m_networkGraph.vtxdist[0] = 0;
  for (int i = 0, k = m_networkGraph.gnvtxs; i < m_mpiNumProcesses; i++) {
    int l = k / (m_mpiNumProcesses - i);
    m_networkGraph.vtxdist[i + 1] = m_networkGraph.vtxdist[i] + l;
    k -= l;
  }

  // local num of vertex for physical vertex distribution (for partition computing)
  m_networkGraph.nvtxs = m_networkGraph.vtxdist[m_mpiProcessId + 1] - m_networkGraph.vtxdist[m_mpiProcessId];

  // fill graph structure arrays
  m_networkGraph.xadj = new parmetis_idx_t [m_networkGraph.nvtxs + 1];
  m_networkGraph.vwgt = new parmetis_idx_t [m_networkGraph.nvtxs];
  m_networkGraph.gvwgt = new parmetis_idx_t [m_networkGraph.gnvtxs];
  m_networkGraph.part = new parmetis_idx_t [m_networkGraph.nvtxs];
  m_networkGraph.part_all = new parmetis_idx_t [m_networkGraph.gnvtxs];


  /*
   * to build specific csr format we need at first step count number of neighbours for each vertex
   */

  // local vertex index (in array part)
  parmetis_idx_t index = 0;

  // go throw all vertex
  for (NodeContainer::Iterator it = node_container.Begin(); it < node_container.End(); ++it)
    {
      // node id into interval [m_networkGraph.vtxdist[m_mpiProcessId], m_networkGraph.vtxdist[m_mpiProcessId + 1]]
	  // so this vertex will handled by this process (at repartition stage)
	  // rem! vertex distribution throw processes for parmetis not the same as for simulator
      if (((int)(*it)->GetId() >= m_networkGraph.vtxdist[m_mpiProcessId]) && ((int)(*it)->GetId() < m_networkGraph.vtxdist[m_mpiProcessId + 1]))
        {
          // edges counter for current vertex
          parmetis_idx_t edge_index = 0;

          // comupute local vertex index
          index = (*it)->GetId() - m_networkGraph.vtxdist[m_mpiProcessId];

          // set current partition for vertex - as set at the beginning of simulation
          m_networkGraph.part[index] = (*it)->GetSystemId();

          // go throw all channels on this node for compute num of neighbours
          for (size_t i = 0; i < (*it)->GetNDevices (); ++i)
            {
              Ptr<NetDevice> localNetDevice = (*it)->GetDevice (i);
              if (!localNetDevice->IsPointToPoint ()) continue;
              Ptr<Channel> channel = localNetDevice->GetChannel ();
              if (channel == 0) continue;
              edge_index++;
            }
          // fill array xadj by computed num of neighbours for each vertex
          m_networkGraph.xadj[index + 1] = edge_index;
        }
      // fill part_all (global)
      m_networkGraph.part_all[(*it)->GetId()] = (*it)->GetSystemId();
    }

  /*
   * build graph structure
   */

  // after pred step xadj[i] = num of neighbours of vertex i
  // as csr format we need to compute prefix sum for xadj
  m_networkGraph.xadj[0] = 0;
  for (size_t i = 1; i < m_networkGraph.nvtxs + 1; i++)
  {
	  m_networkGraph.xadj[i] += m_networkGraph.xadj[i - 1];
  }

  // create arrays
  m_networkGraph.adjncy = new parmetis_idx_t[m_networkGraph.xadj[m_networkGraph.nvtxs]];
  m_networkGraph.adjwgt = new parmetis_idx_t[m_networkGraph.xadj[m_networkGraph.nvtxs]];

  // go throw all vertex and fill adjncy  and adjwgt (by delay in channel) arrays
  for (NodeContainer::Iterator it = node_container.Begin(); it < node_container.End(); ++it)
    {
	  // vertex will handled by this process (at repartition stage)
      if (((int)(*it)->GetId() >= m_networkGraph.vtxdist[m_mpiProcessId]) && ((int)(*it)->GetId() < m_networkGraph.vtxdist[m_mpiProcessId + 1]))
        {
          // compute local vertex index
          index = (*it)->GetId() - m_networkGraph.vtxdist[m_mpiProcessId];
          // go throw all vertex neighbours and fill adjncy and adjwgt
          int current_edge = 0;
          for (size_t i = 0; i < (*it)->GetNDevices (); ++i)
            {
              Ptr<NetDevice> localNetDevice = (*it)->GetDevice (i);
              if (!localNetDevice->IsPointToPoint ()) continue;
              Ptr<Channel> channel = localNetDevice->GetChannel ();
              if (channel == 0) continue;
              int id = (channel->GetDevice (1) == localNetDevice) ? 0 : 1;
              TimeValue delay;
              channel->GetAttribute ("Delay", delay);
              m_networkGraph.adjncy[ m_networkGraph.xadj[index] + current_edge] = (channel->GetDevice (id))->GetNode ()->GetId ();
              m_networkGraph.adjwgt[ m_networkGraph.xadj[index] + current_edge] = delay.Get().GetMilliSeconds();
              current_edge++;
            }
        }
    }

  // set other parameters
  m_networkGraph.nparts = m_mpiNumProcesses;
  m_networkGraph.tpwgts = new parmetis_real_t[m_networkGraph.nparts];
  parmetis_real_t tpw = 1.0/(parmetis_real_t)m_networkGraph.nparts;

  for (int i = 0; i < m_networkGraph.nparts; i++) {
	  m_networkGraph.tpwgts[i] = tpw;
  }

  MPI_Barrier (MPI_COMM_WORLD);

  /*
   * create boost graph for easy writing graph to files
   */
  for (NodeContainer::Iterator it = node_container.Begin(); it < node_container.End(); ++it)
    {
	  m_networkBoostGraphVertexMap[(*it)->GetId()] = boost::add_vertex(m_networkBoostGraph);
      boost::put(boost::vertex_name, m_networkBoostGraph, m_networkBoostGraphVertexMap[(*it)->GetId()], (*it)->GetId());
      boost::put(boost::vertex_color, m_networkBoostGraph, m_networkBoostGraphVertexMap[(*it)->GetId()], m_networkGraph.part_all[(*it)->GetId()]);
     }

  for (NodeContainer::Iterator it = node_container.Begin(); it < node_container.End(); ++it)
    {
      for (uint32_t i = 0; i < (*it)->GetNDevices (); ++i)
        {
          Ptr<NetDevice> localNetDevice = (*it)->GetDevice (i);
          // only works for p2p links currently
          if (!localNetDevice->IsPointToPoint ()) continue;
          Ptr<Channel> channel = localNetDevice->GetChannel ();
          if (channel == 0) continue;

          // grab the adjacent node
          Ptr<Node> remoteNode;
          if (channel->GetDevice (1) == localNetDevice)
            {
               remoteNode = (channel->GetDevice (0))->GetNode ();
               boost::add_edge (m_networkBoostGraphVertexMap[(*it)->GetId ()],
                                m_networkBoostGraphVertexMap[remoteNode->GetId ()],
                                m_networkBoostGraph);
             }
        }
    }

  std::cerr << "Graph creation finished" << std::endl;

}

void
DistributedSimulatorImpl::UpdateNetworkGraph ()
{
  std::cerr << "Graph update start... " << std::endl;

  MPI_Status stat;

  int *loads = new int [m_networkGraph.gnvtxs];

  // each process send to other processes information about statistic at own nodes
  for (int i = 0; i < m_mpiNumProcesses; i++){
    if (i != m_mpiProcessId) {
    	MPI_Send((void *)m_networkGraph.gvwgt, m_networkGraph.gnvtxs, MPI_INT, i, 123, MPI_COMM_WORLD);
    }
  }

  // clean statistic to correct balancing at the next partition iteration
  for (size_t i = 0; i < m_networkGraph.nvtxs; i++) {
	  m_networkGraph.vwgt[i] = 0;
  }

  for (size_t i = 0; i < m_networkGraph.gnvtxs; i++) {
	  m_networkGraph.gvwgt[i] = 0;
  }

  // get load statistic from other processes
  for (int i = 0; i < m_mpiNumProcesses; i++) {
    if (i != m_mpiProcessId) {
      MPI_Recv((void *)loads, m_networkGraph.gnvtxs, MPI_INT, i, 123, MPI_COMM_WORLD, &stat);
      for (size_t j = 0; j < m_networkGraph.nvtxs; j++) {
         m_networkGraph.vwgt[j] += loads[m_networkGraph.vtxdist[m_mpiProcessId] + j];
      }
    }
  }

  MPI_Barrier (MPI_COMM_WORLD);

  std::cerr << "Graph update finished" << std::endl;

}

void
DistributedSimulatorImpl::WriteClusterGraph (const std::string& filename)
{

  std::cerr << "Graph writing start... " << std::endl;

  // put new information about graph partition
  for (size_t i = 0; i < m_networkGraph.gnvtxs; i++)
    {
      boost::put(boost::vertex_color, m_networkBoostGraph, m_networkBoostGraphVertexMap[i], m_networkGraph.part_all[i]);
    }

  std::ofstream graphStream((filename + boost::lexical_cast<std::string>(m_mpiProcessId) + std::string(".dot")).c_str());

  boost::dynamic_properties dp;

  boost::property_map<boost_graph_t, boost::vertex_index_t>::type name =
  boost::get(boost::vertex_index, m_networkBoostGraph);
  dp.property("node_id", name);

  boost::property_map<boost_graph_t, boost::vertex_color_t>::type color =
  boost::get(boost::vertex_color, m_networkBoostGraph);
  dp.property("label", color);


  boost::write_graphviz_dp(graphStream, m_networkBoostGraph, dp);

  std::cerr << "Graph writing finished" << std::endl;

}

void
DistributedSimulatorImpl::StartDynamicReclustering ()
{

  std::cerr << "Reclustering iteration " << m_iterationNum ++ << " on cluster node "<< m_mpiProcessId << std::endl;
  Reclustering ();
  // recursive call with m_reclusteringInterval interval
  Simulator::Schedule (m_reclusteringInterval, &DistributedSimulatorImpl::StartDynamicReclustering, this);

}

void
DistributedSimulatorImpl::StartStaticReclustering(void) {

  std::cerr << "Static reclustering.. " << m_mpiProcessId << std::endl;
  // call reclustering at the end and write result
  Reclustering ();
  WriteClusterGraph("graph_test_");

}

#endif

/**
 * Added by olya - end
 */

} // namespace ns3
