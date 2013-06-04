/*
 * load-balancing-application.cpp
 *
 *  Created on: May 2, 2013
 *      Author: olya
 */

#include "load-balancing-application.h"
#include "ns3/log.h"
#include "ns3/simulator.h"
#include "ns3/onoff-application.h"
#include "ns3/node-container.h"
#include "ns3/node-list.h"
#include "ns3/channel.h"
#include "mpi-interface.h"
#include <boost/graph/graphviz.hpp>
#include <boost/graph/adj_list_serialize.hpp>

#include <boost/algorithm/string/split.hpp>
#include <boost/algorithm/string/classification.hpp>

#include <utility>
#include <sstream>
#include <mpi.h>


#include "lso_cluster_func.hpp"

NS_LOG_COMPONENT_DEFINE ("LoadBalancingApplication");

namespace ns3 {

NS_OBJECT_ENSURE_REGISTERED (LoadBalancingApplication);

TypeId
LoadBalancingApplication::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::LoadBalancingApplication")
    .SetParent<Application> ()
    .AddConstructor<LoadBalancingApplication> ()
    .AddAttribute ("ReclusteringInterval", "Reclustering interval.",
                   TimeValue (Seconds (20)),
                   MakeTimeAccessor (&LoadBalancingApplication::m_reclusteringInterval),
                   MakeTimeChecker ());
  return tid;
}


LoadBalancingApplication::LoadBalancingApplication ()
  : m_reclusteringInterval (Seconds (20)),
    m_iterationNum (0)
{
  NS_LOG_FUNCTION (this);
  MPI_Comm comm;
  m_mpiComm = &comm;

  MPI_Comm_size(comm, &m_mpiProcessId);
  MPI_Comm_rank(comm, &m_mpiNumProcesses);
}

LoadBalancingApplication::~LoadBalancingApplication()
{
  NS_LOG_FUNCTION (this);
}

void
LoadBalancingApplication::Init (void)
{
  CreateNetworkGraph ();
}

void
LoadBalancingApplication::SetReclusteringInterval (Time reclusteringInterval)
{
  NS_LOG_FUNCTION (this << reclusteringInterval);
  m_reclusteringInterval = reclusteringInterval;
}

void
LoadBalancingApplication::DoDispose (void)
{
  NS_LOG_FUNCTION (this);
  Application::DoDispose ();
}

// Application Methods
void LoadBalancingApplication::StartApplication () // Called at time specified by Start
{
  NS_LOG_FUNCTION (this);
  Init ();
  CancelEvents ();
  ScheduleReclusteringEvent ();
}

void LoadBalancingApplication::StopApplication () // Called at time specified by Stop
{
  NS_LOG_FUNCTION (this);
  CancelEvents ();
}

void LoadBalancingApplication::CancelEvents ()
{
  NS_LOG_FUNCTION (this);
}

void LoadBalancingApplication::ScheduleReclusteringEvent ()
{
  NS_LOG_FUNCTION (this);
  m_reclusteringEvent = Simulator::Schedule (m_reclusteringInterval, &LoadBalancingApplication::StartReclustering, this);
}

// Event handlers
void LoadBalancingApplication::StartReclustering ()
{
  NS_LOG_FUNCTION (this);
  std::cerr << "Reclustering iteration " << m_iterationNum ++ << " on cluster node "<< m_mpiProcessId << std::endl;
  Reclustering ();
  ScheduleReclusteringEvent ();
}


// Logical functions
void LoadBalancingApplication::Reclustering ()
{

  NS_LOG_FUNCTION (this);
  UpdateNetworkGraph ();
  MPI_Status stat;

    ParMETIS_V3_RefineKway(m_networkGraph.vtxdist, m_networkGraph.xadj, m_networkGraph.adjncy, m_networkGraph.vwgt,
  		  m_networkGraph.adjwgt, &m_networkGraph.wgtflag, &m_networkGraph.numflag, &m_networkGraph.ncon,
  		  &m_networkGraph.nparts, m_networkGraph.tpwgts, m_networkGraph.ubvec, m_networkGraph.options,
            &m_networkGraph.edgecut, m_networkGraph.part, m_mpiComm);

    for (int i =0; i < m_mpiNumProcesses; i++){
      if (i != m_mpiProcessId) {
        MPI_Send((void *)m_networkGraph.part, m_networkGraph.nvtxs, MPI_INT, i, 0, *m_mpiComm);
      }
    }


    for (int i = 0; i < m_mpiNumProcesses; i++){
      if (i != m_mpiProcessId) {
        MPI_Recv((void *)&m_networkGraph.part_all[m_networkGraph.vtxdist[i]],
      		  m_networkGraph.vtxdist[i + 1] - m_networkGraph.vtxdist[i], MPI_INT, i, 0, *m_mpiComm, &stat);
      }
    }

    for (int i = 0; i < m_networkGraph.gnvtxs; ++i) {
  	  if (((int)NodeList::GetNode (i)->GetSystemId() == m_mpiProcessId) && (m_networkGraph.part_all[i] != m_mpiProcessId)) {
  		Ptr<Node> nodeForMoving = NodeList::GetNode (i);
  		//nodeForMoving-> SetSystemId (m_networkGraph.part_all[i]);
  		std::string applications;
  		std::vector<Ptr<Application> > nodeApplications = nodeForMoving->GetApplications ();

  		for (uint32_t j = 0; j < nodeApplications.size (); ++j)
  		{
  			nodeApplications[j]-> SetStopTime(Simulator::Now());
  			applications.append(" ");
  			applications.append(nodeApplications[j]->GetInstanceTypeId ().GetName ());
  		}
  		size_t app_size = applications.size();
  		MPI_Send((void *)(&app_size), 1, MPI_UNSIGNED, m_networkGraph.part_all[i], 0, *m_mpiComm);
  		MPI_Send((void *)applications.c_str(), applications.size(), MPI_CHAR, m_networkGraph.part_all[i], 0, *m_mpiComm);

  	  }
    }

    for (int i = 0; i < m_networkGraph.gnvtxs; ++i) {
  	  if ((m_networkGraph.part_all[i] == m_mpiProcessId) && ((int)NodeList::GetNode (i)->GetSystemId() != m_mpiProcessId)) {
  		Ptr<Node> nodeForMoving = NodeList::GetNode (i);
  		size_t applicationsNum;
  		MPI_Recv((void *)&applicationsNum, 1, MPI_UNSIGNED, (int)NodeList::GetNode (i)->GetSystemId(), 0, *m_mpiComm, &stat);
  		char* applications = new char[applicationsNum];
  		MPI_Recv((void *)applications, applicationsNum, MPI_CHAR, (int)NodeList::GetNode (i)->GetSystemId(), 0, *m_mpiComm, &stat);

  		std::string applicationsString(applications);
  		std::vector <std::string> nodeApplications;
  		boost::algorithm::split(nodeApplications, applicationsString, boost::algorithm::is_any_of(" "));
  		nodeForMoving-> SetSystemId (m_networkGraph.part_all[i]);

  		for (uint32_t j = 0; j < nodeApplications.size (); ++j)
  		{
  	        ObjectFactory objectFactory;
  	        objectFactory.SetTypeId (TypeId::LookupByName (nodeApplications[j]) );
  	        Ptr<Application> application = objectFactory.Create<Application> ();
  	        application-> SetStartTime (Simulator::Now());
  	        application-> Start ();
  	        nodeForMoving->AddApplication (application);
  		}
  	  }
    }
}

void
LoadBalancingApplication::CreateNetworkGraph (void)
{

  NodeContainer node_container =  NodeContainer::GetGlobal ();
  m_networkGraph.gnvtxs = node_container.GetN ();

  m_networkGraph.vtxdist = new parmetis_idx_t [m_mpiNumProcesses + 1];
  m_networkGraph.vtxdist[0] = 0;
  for (int i = 0, k = m_networkGraph.gnvtxs; i < m_mpiNumProcesses; i++) {
    int l = k / (m_mpiNumProcesses - i);
    m_networkGraph.vtxdist[i + 1] = m_networkGraph.vtxdist[i] + l;
    k -= l;
  }

  m_networkGraph.nvtxs = m_networkGraph.vtxdist[m_mpiProcessId + 1] - m_networkGraph.vtxdist[m_mpiProcessId];
  m_networkGraph.xadj = new parmetis_idx_t [m_networkGraph.nvtxs + 1];
  m_networkGraph.vwgt = new parmetis_idx_t [m_networkGraph.nvtxs];
  m_networkGraph.part = new parmetis_idx_t[m_networkGraph.nvtxs];
  m_networkGraph.part_all = new parmetis_idx_t[m_networkGraph.gnvtxs];

  m_networkGraph.gnedges = 0;
  parmetis_idx_t index = 0;

  for (NodeContainer::Iterator it = node_container.Begin(); it < node_container.End(); ++it)
    {
      if (((int)(*it)->GetId() > m_networkGraph.vtxdist[m_mpiProcessId]) && ((int)(*it)->GetId() > m_networkGraph.vtxdist[m_mpiProcessId + 1]))
        {
    	  index = (*it)->GetId() - m_networkGraph.vtxdist[m_mpiProcessId];
    	  m_networkGraph.part[index] = (*it)->GetSystemId();
          for (uint32_t i = 0; i < (*it)->GetNDevices (); ++i)
            {
              Ptr<NetDevice> localNetDevice = (*it)->GetDevice (i);
              if (!localNetDevice->IsPointToPoint ()) continue;
              Ptr<Channel> channel = localNetDevice->GetChannel ();
              if (channel == 0) continue;
              m_networkGraph.gnedges++;
            }
          m_networkGraph.xadj[index + 1] = m_networkGraph.gnedges;
        }
      m_networkGraph.part_all[(*it)->GetId()] = (*it)->GetSystemId();
    }

  m_networkGraph.xadj[0] = 0;
  for (int i = 1; i < m_networkGraph.nvtxs + 1; i++)
  {
	  m_networkGraph.xadj[i] += m_networkGraph.xadj[i - 1];
  }

  m_networkGraph.adjncy = new parmetis_idx_t[m_networkGraph.xadj[m_networkGraph.nvtxs]];
  m_networkGraph.adjwgt = new parmetis_idx_t[m_networkGraph.xadj[m_networkGraph.nvtxs]];

  for (NodeContainer::Iterator it = node_container.Begin(); it < node_container.End(); ++it)
    {
      if (((int)(*it)->GetId() > m_networkGraph.vtxdist[m_mpiProcessId]) && ((int)(*it)->GetId() > m_networkGraph.vtxdist[m_mpiProcessId + 1]))
        {
    	  index = (*it)->GetId() - m_networkGraph.vtxdist[m_mpiProcessId];
    	  int current_edge = 0;
          for (uint32_t i = 0; i < (*it)->GetNDevices (); ++i)
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

  m_networkGraph.nparts = m_mpiNumProcesses;
  m_networkGraph.tpwgts = new parmetis_real_t[m_networkGraph.nparts];
  parmetis_real_t tpw = 1.0/(parmetis_real_t)m_networkGraph.nparts;

  for (int i = 0; i < m_networkGraph.nparts; i++) {
	  m_networkGraph.tpwgts[i] =tpw;
  }

}

void
LoadBalancingApplication::UpdateNetworkGraph ()
{

  MPI_Status stat;
  parmetis_idx_t *loads = new parmetis_idx_t[m_networkGraph.gnvtxs];
  parmetis_idx_t *tmp = new parmetis_idx_t[m_networkGraph.gnvtxs];

  NodeContainer node_container =  NodeContainer::GetGlobal();

  for (NodeContainer::Iterator it = node_container.Begin(); it < node_container.End(); ++it)
    {
       loads[(*it)->GetId()] = (*it)->GetLoad();
      (*it)->RemoveLoad();
    }

  for (int i =0; i < m_mpiNumProcesses; i++){
    if (i != m_mpiProcessId) {
      MPI_Send((void *)loads, m_networkGraph.gnvtxs, MPI_INT, i, 0, *m_mpiComm);
    }
  }

  for (int i = 0; i < m_mpiNumProcesses; i++){
    if (i != m_mpiProcessId) {
      MPI_Recv((void *)tmp, m_networkGraph.gnvtxs, MPI_INT, i, 0, *m_mpiComm, &stat);
      for (int j = 0; j < m_networkGraph.nvtxs; j++) {
         m_networkGraph.vwgt[j] = tmp[m_networkGraph.vtxdist[m_mpiProcessId] + j] + (i == 0 ? 0 : m_networkGraph.vwgt[j]);
      }
    }
  }

}

void
LoadBalancingApplication::WriteClusterGraph (const std::string& filename)
{
	graph_t2 g;
	std::map<uint32_t, vertex_descriptor> m_networkGraphVertexMap;
	NodeContainer node_container =  NodeContainer::GetGlobal();
	  for (NodeContainer::Iterator it = node_container.Begin(); it < node_container.End(); ++it)
	    {
	      m_networkGraphVertexMap[(*it)->GetId()] = boost::add_vertex(g);
	      boost::put(boost::vertex_name, g, m_networkGraphVertexMap[(*it)->GetId()], (*it)->GetId());
	      boost::put(boost::vertex_color, g, m_networkGraphVertexMap[(*it)->GetId()], m_networkGraph.part_all[(*it)->GetId()]);
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
	               boost::add_edge (m_networkGraphVertexMap[(*it)->GetId ()],
	                                m_networkGraphVertexMap[remoteNode->GetId ()],
	                                g);
	             }
	        }
	    }
  std::ofstream graphStream((filename + std::string(".dot")).c_str());

  boost::dynamic_properties dp;

  boost::property_map<graph_t2, boost::vertex_index_t>::type name =
  boost::get(boost::vertex_index, g);
  dp.property("node_id", name);

  boost::property_map<graph_t2, boost::vertex_color_t>::type color =
  boost::get(boost::vertex_color, g);
  dp.property("label", color);


  boost::write_graphviz_dp(graphStream, g, dp);

}

} /* namespace ns3 */
