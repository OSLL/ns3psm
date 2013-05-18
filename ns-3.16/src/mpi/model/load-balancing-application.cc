/*
 * load-balancing-application.cpp
 *
 *  Created on: May 2, 2013
 *      Author: olya
 */

#include "load-balancing-application.h"
#include "ns3/log.h"
#include "ns3/simulator.h"
#include "ns3/node-container.h"
#include "ns3/node-list.h"
#include "ns3/channel.h"

#ifdef NS3_MPI

#include "mpi-interface.h"
#include <boost/graph/graphviz.hpp>
#include <boost/graph/adj_list_serialize.hpp>
#include <utility>
#include <sstream>
#include "lso_cluster_func.hpp"

#endif

NS_LOG_COMPONENT_DEFINE ("LoadBalancingApplication");

namespace ns3 {

#ifdef NS3_MPI
int get(global_value_owner_map_graph, global_value_graph k) {
  return k.processor;
}

int get(global_value_owner_map_node, global_value_node k) {
  return k.processor;
}

int get(global_value_owner_map_applications, global_value_applications k) {
  return k.processor;
}

#endif

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
    m_clusterLoad (0),
    m_iterationNum (0)
{
  NS_LOG_FUNCTION (this);
  #ifdef NS3_MPI

  char** argv; int argc = 0;
  boost::mpi::environment env(argc, argv);
  boost::mpi::communicator world;

  process_group pg(world);
  m_mpiProcessId = process_id(pg);
  m_mpiNumProcesses = num_processes(pg);
  m_mpiProcessGroup = pg;

  m_mpiGraphQueue = new dist_queue_graph_t(pg, global_value_owner_map_graph());
  m_mpiNodeQueue = new dist_queue_node_t(pg, global_value_owner_map_node());
  m_mpiApplicationsQueue =  new dist_queue_applications_t(pg, global_value_owner_map_applications());
  #endif
}

LoadBalancingApplication::~LoadBalancingApplication()
{
  NS_LOG_FUNCTION (this);
  #ifdef NS3_MPI
  delete m_mpiGraphQueue;
  delete m_mpiNodeQueue;
  delete m_mpiApplicationsQueue;
  #endif
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
LoadBalancingApplication::IncNodeLoad (uint32_t context)
{
  #ifdef NS3_MPI
  if ((context >= 0) && (context < m_networkGraphVertexMap.size ()))
  {
    boost::put(boost::vertex_color,
               m_networkGraph,
               m_networkGraphVertexMap[context],
               boost::get(boost::vertex_color,
                          m_networkGraph,
                          m_networkGraphVertexMap[context]) + 1);
  }
  #endif
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
  #ifdef NS3_MPI
  Init ();
  CancelEvents ();
  ScheduleReclusteringEvent ();
  #endif
}

void LoadBalancingApplication::StopApplication () // Called at time specified by Stop
{
  NS_LOG_FUNCTION (this);
  #ifdef NS3_MPI
  CancelEvents ();
  #endif
}

void LoadBalancingApplication::CancelEvents ()
{
  NS_LOG_FUNCTION (this);

  /*if (m_reclusteringEvent.IsRunning ())
    {
	  // TODO do smth
    }
  Simulator::Cancel (m_reclusteringEvent);*/
}

void LoadBalancingApplication::ScheduleReclusteringEvent ()
{
  NS_LOG_FUNCTION (this);
  #ifdef NS3_MPI
  m_reclusteringEvent = Simulator::Schedule (m_reclusteringInterval, &LoadBalancingApplication::StartReclustering, this);
  #endif
}

// Event handlers
void LoadBalancingApplication::StartReclustering ()
{
  NS_LOG_FUNCTION (this);

  Reclustering ();
}

void LoadBalancingApplication::StartCreateNetworkGraph ()
{
  NS_LOG_FUNCTION (this);

  CreateNetworkGraph ();
}

void LoadBalancingApplication::StartMergeNetwork ()
{
  NS_LOG_FUNCTION (this);

  MergeNetworkGraph ();
}

void LoadBalancingApplication::StartUpdateNetworkGraph ()
{
  NS_LOG_FUNCTION (this);

  UpdateNetworkGraph ();
}

void LoadBalancingApplication::StartWriteNetworkGraph ()
{
  NS_LOG_FUNCTION (this);

  WriteNetworkGraph (std::string (""));
}

// Logical functions
void LoadBalancingApplication::Reclustering ()
{
  NS_LOG_FUNCTION (this);
  #ifdef NS3_MPI
  std::cerr << "Reclustering iteration " << m_iterationNum ++ << " on cluster node "<< m_mpiProcessId << std::endl;
  MergeNetworkGraph ();
  ScheduleReclusteringEvent ();
  #endif
}

void
LoadBalancingApplication::CreateNetworkGraph (void)
{
  #ifdef NS3_MPI
  NodeContainer node_container =  NodeContainer::GetGlobal();
  for (NodeContainer::Iterator it = node_container.Begin(); it < node_container.End(); ++it)
    {
      m_networkGraphVertexMap[(*it)->GetId()] = boost::add_vertex(m_networkGraph);
      boost::put(boost::vertex_name, m_networkGraph, m_networkGraphVertexMap[(*it)->GetId()], (*it)->GetId());
    }

  for (NodeContainer::Iterator it = node_container.Begin(); it < node_container.End(); ++it)
    {
      for (uint32_t i = 0; i < (*it)->GetNDevices (); ++i)
        {
          Ptr<NetDevice> localNetDevice = (*it)->GetDevice (i);
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
          if (channel->GetDevice (1) == localNetDevice)
            {
               remoteNode = (channel->GetDevice (0))->GetNode ();
               TimeValue delay;
               channel->GetAttribute ("Delay", delay);
               boost::add_edge (m_networkGraphVertexMap[(*it)->GetId ()],
                                m_networkGraphVertexMap[remoteNode->GetId ()],
                                m_networkGraph);
             }
        }
    }
  #endif
}

void
LoadBalancingApplication::UpdateNetworkGraph ()
{
  #ifdef NS3_MPI
  NodeContainer node_container =  NodeContainer::GetGlobal();
  for (NodeContainer::Iterator it = node_container.Begin(); it < node_container.End(); ++it)
    {
      for (uint32_t i = 0; i < (*it)->GetNDevices (); ++i)
        {

          Ptr<NetDevice> localNetDevice = (*it)->GetDevice (i);

          if (!localNetDevice->IsPointToPoint ())
            {
              continue;
            }
          Ptr<Channel> channel = localNetDevice->GetChannel ();
          if (channel == 0)
            {
              continue;
            }

          Ptr<Node> remoteNode;
          if (channel->GetDevice (1) == localNetDevice)
            {
              remoteNode = (channel->GetDevice (0))->GetNode ();
              boost::put (boost::edge_weight,
                          m_networkGraph,
                          boost::edge(m_networkGraphVertexMap[(*it)->GetId ()], m_networkGraphVertexMap[remoteNode->GetId ()], m_networkGraph).first,
                          channel->GetTraffic ()
              );
            }

          }
      }
  #endif
}

void
LoadBalancingApplication::WriteNetworkGraph (const std::string& filename)
{
  #ifdef NS3_MPI
  std::ofstream graphStream((filename +
                             std::string("_").c_str() +
                             boost::lexical_cast<std::string>(MpiInterface::GetSystemId()) +
                             std::string("_").c_str() +
                             boost::lexical_cast<std::string>(m_iterationNum) +
                             std::string(".dot")).c_str());

  boost::dynamic_properties dp;

  boost::property_map<graph_t, boost::vertex_index_t>::type name =
  boost::get(boost::vertex_index, m_networkGraph);
  dp.property("node_id", name);

  boost::property_map<graph_t, boost::vertex_color_t>::type color =
  boost::get(boost::vertex_color, m_networkGraph);
  dp.property("label", color);

  boost::property_map<graph_t, boost::edge_weight_t>::type weight =
  boost::get(boost::edge_weight, m_networkGraph);
  dp.property("label", weight);

  boost::write_graphviz_dp(graphStream, m_networkGraph, dp);
  #endif
}

void
LoadBalancingApplication::MergeNetworkGraph ()
{
  #ifdef NS3_MPI
  UpdateNetworkGraph ();

  if (m_mpiProcessId != 0)
  {
      global_value_graph v_graph(0, m_networkGraph);
      m_mpiGraphQueue->push(v_graph);
  }

  double max_traffic = 0;

  while (!m_mpiGraphQueue->empty())
  {
    global_value_graph v_graph = m_mpiGraphQueue->top(); m_mpiGraphQueue->pop();
    graph_t tmp = v_graph.value;

    // Merging nodes load
    graph_vertex_iterator sti, eni;
    boost::tie(sti, eni) = boost::vertices(tmp);
    std::vector<vertex_descriptor> vertex_list(sti, eni);

    for (uint32_t i = 0; i < vertex_list.size(); ++i)
    {
      boost::put (boost::vertex_color,
                  m_networkGraph,
                  boost::vertex (boost::get (boost::vertex_index, tmp, vertex_list[i]), m_networkGraph),
                  boost::get (boost::vertex_color,
                             m_networkGraph,
                             boost::vertex (boost::get(boost::vertex_index, tmp, vertex_list[i]), m_networkGraph)) +
                  boost::get (boost::vertex_color,
                             tmp,
                             vertex_list[i]));
    }

    // Merging channels traffic
    graph_edge_iterator st, en;
    boost::tie(st, en) = boost::edges(tmp);
    std::vector<edge_descriptor> edge_list(st, en);

    for (uint32_t i = 0; i < edge_list.size(); ++i)
    {
      double sum_traffic = (double) (boost::get (boost::edge_weight,
                                                 m_networkGraph,
                                                 boost::edge (boost::vertex (boost::get (boost::vertex_index,
                                                                                         tmp,
                                                                                         boost::source(edge_list[i], tmp)),
                                                                             m_networkGraph),
                                                              boost::vertex (boost::get (boost::vertex_index,
                                                                                         tmp,
                                                                                         boost::target(edge_list[i], tmp)),
                                                                             m_networkGraph),
                                                              m_networkGraph).first) +
                                                 boost::get (boost::edge_weight, tmp, edge_list[i]));
      if (sum_traffic > max_traffic)
      {
         max_traffic = sum_traffic;
      }

      boost::put (boost::edge_weight,
                  m_networkGraph,
                  boost::edge (boost::vertex (boost::get(boost::vertex_index,
                                                         tmp,
                                                         boost::source(edge_list[i], tmp)),
                                              m_networkGraph),
                               boost::vertex (boost::get(boost::vertex_index,
                                                         tmp,
                                                         boost::target(edge_list[i], tmp)),
                                              m_networkGraph),
                               m_networkGraph).first,
                               sum_traffic
                  );
    }

  }
  synchronize(m_mpiProcessGroup);

  // Reclustering
  if (m_mpiProcessId == 0)
  {
    graph_edge_iterator st, en;
    boost::tie(st, en) = boost::edges(m_networkGraph);
    std::vector<edge_descriptor> edge_list(st, en);

    for (uint32_t i = 0; i < edge_list.size(); ++i)
    {
    boost::put (boost::edge_weight,
                m_networkGraph,
                edge_list[i],
                (max_traffic - boost::get (boost::edge_weight, m_networkGraph, edge_list[i])) / max_traffic
               );
    }
    WriteNetworkGraph (std::string ("graph"));
    ClusterNetworkGraph ();
  }

  synchronize(m_mpiProcessGroup);

  //
  while (!m_mpiNodeQueue->empty())
    {
      global_value_node v_node = m_mpiNodeQueue->top(); m_mpiNodeQueue->pop();
      node_for_moving_t node_for_moving = v_node.value;

      Ptr<Node> nodeForMoving = NodeList::GetNode (node_for_moving.context);
      nodeForMoving-> SetSystemId (node_for_moving.new_cluster);

      std::vector< std::string > applicationsType;
      std::vector<Ptr<Application> > nodeApplications = nodeForMoving->GetApplications ();

      for (uint32_t i = 0; i < nodeApplications.size (); ++i)
      {
    	  applicationsType.push_back(nodeApplications[i]->GetInstanceTypeId ().GetName ());
      }

      apllications_for_moving_t applications;
      applications.context = node_for_moving.context;
      applications.appliations = applicationsType;
      global_value_applications v_applications(node_for_moving.new_cluster, applications);
      m_mpiApplicationsQueue->push (v_applications);
    }

  synchronize(m_mpiProcessGroup);

  while (!m_mpiApplicationsQueue->empty())
    {
      global_value_applications v_applications = m_mpiApplicationsQueue->top(); m_mpiApplicationsQueue->pop();
      apllications_for_moving_t apllications_for_moving = v_applications.value;

      Ptr<Node> nodeForMoving = NodeList::GetNode (apllications_for_moving.context);

      std::vector< std::string > applications = apllications_for_moving.appliations;

      for (uint32_t i = 0; i < applications.size(); ++i)
      {
        ObjectFactory objectFactory;
        objectFactory.SetTypeId (TypeId::LookupByName (applications[i]) );
        Ptr<Application> application = objectFactory.Create<Application> ();
        application-> SetStartTime (Simulator::Now());
        application-> Start ();
        nodeForMoving->AddApplication (application);
      }
    }
  synchronize(m_mpiProcessGroup);
  #endif
}

void
LoadBalancingApplication::ClusterNetworkGraph ()
{
  #ifdef NS3_MPI
  {
    std::ofstream clusterGraphStream (std::string ("example.txt").c_str ());

    graph_edge_iterator st, en;
    boost::tie (st, en) = boost::edges (m_networkGraph);
    std::vector<edge_descriptor> edgeList (st, en);

    for (uint32_t i = 0; i < edgeList.size (); ++i)
    {
      clusterGraphStream << boost::get(boost::vertex_index, m_networkGraph, boost::source(edgeList[i], m_networkGraph));
      clusterGraphStream << " ";
      clusterGraphStream << boost::get(boost::vertex_index, m_networkGraph, boost::target(edgeList[i], m_networkGraph));
      clusterGraphStream << " ";
      clusterGraphStream << boost::get(boost::edge_weight, m_networkGraph, edgeList[i]);
      clusterGraphStream << "\n";
    }
  }

  std::string num_clusters_string = boost::lexical_cast<std::string> (m_mpiNumProcesses);
  char const *num_clusters_char = num_clusters_string.c_str ();

  const char* argv[] = {"example.txt", "--loss",
                       /*"rcut","infomap",*/ "ncut", /*"modularity",*/
                       "--num_clusters", num_clusters_char, NULL};

  int argc = sizeof (argv) / sizeof (char*) - 1;


  try
  {
    // parse arguments, and run the clustering algorithm
    ParamSourceCommandline param_source (argc, argv);
    LsoMainFunctionCommandLine runner;
    runner.add_all_parameters (param_source);
    runner.run ();

    std::vector< node_for_moving_t > nodes_for_moving;

    for (size_t i = 0 ; i < runner.clustering.size () ; ++i)
    {
      uint32_t old_cluster = NodeList::GetNode (i)-> GetSystemId ();
      uint32_t new_cluster = runner.clustering[i];
      if (old_cluster != new_cluster)
      {
         node_for_moving_t node_for_moving;
         node_for_moving.context = i;
         node_for_moving.old_cluster = old_cluster;
         node_for_moving.new_cluster = new_cluster;

         nodes_for_moving.push_back(node_for_moving);

         boost::put (boost::vertex_distance,
                     m_networkGraph,
                     boost::vertex (i, m_networkGraph),
                     new_cluster);
      }
    }

    for (uint32_t i = 0; i < nodes_for_moving.size(); ++i)
    {
      node_for_moving_t node_for_moving = nodes_for_moving[i];
      global_value_node v(node_for_moving.old_cluster, node_for_moving);

      m_mpiNodeQueue->push(v);
    }

  } catch (std::exception const& e)
  {
	  std::cerr << e.what() << std::endl;
    return;
  } catch (...)
  {
	  std::cerr << "Unexpected error" << std::endl;
    return;
  }

  WriteClusterGraph (std::string("cluster"));

/*
  // get border network nodes

  // border nodes
  //first - node id
  //second first - own cluster
  //second second - list of neighboring clusters
  std::vector<std::pair <uint32_t, std::pair <uint32_t, vector <uint32_t> > > > border_nodes;

  graph_vertex_iterator sti, eni;
  boost::tie(sti, eni) = boost::vertices(m_networkGraph);
  std::vector<vertex_descriptor> vertex_list(sti, eni);

  for (uint32_t i = 0; i < vertex_list.size(); ++i)
  {

      // get all vertices adjacent to vertex back
      graph_adjacency_iterator st, en;
      boost::tie(st, en) = adjacent_vertices(vertex_list[i], m_networkGraph);

      // copy to vector
      std::vector<vertex_descriptor> adjacent_vertex(st, en);
      vector <uint32_t> neighbors;

      for (uint32_t j = 0; j < adjacent_vertex.size(); ++j) {
        if (boost::get (boost::vertex_distance, m_networkGraph, vertex_list[i]) !=
            boost::get (boost::vertex_distance, m_networkGraph, adjacent_vertex[j]))
        {
           neighbors.push_back(boost::get(boost::vertex_index, m_networkGraph, adjacent_vertex[j]));
           std::cout << "border node " << i << std::endl;
        }
      }

      if (neighbors.size() > 0)
      {
          std::pair <uint32_t, vector <uint32_t> >
                                     border_node_neighbors (
                                                boost::get (boost::vertex_distance, m_networkGraph, vertex_list[i]),
                                                neighbors);

          std::pair <uint32_t,  std::pair <uint32_t, vector <uint32_t> > > border_node (i, border_node_neighbors);
          border_nodes.push_back(border_node);
      }
  }*/
  #endif

}

void
LoadBalancingApplication::WriteClusterGraph (const std::string& filename)
{
  #ifdef NS3_MPI
  std::ofstream graphStream((filename +
                             std::string("_").c_str() +
                             boost::lexical_cast<std::string>(MpiInterface::GetSystemId()) +
                             std::string("_").c_str() +
                             boost::lexical_cast<std::string>(m_iterationNum) +
                             std::string(".dot")).c_str());

  boost::dynamic_properties dp;

  boost::property_map<graph_t, boost::vertex_name_t>::type name =
  boost::get(boost::vertex_name, m_networkGraph);
  dp.property("node_id", name);

  boost::property_map<graph_t, boost::vertex_distance_t>::type distance =
  boost::get(boost::vertex_distance, m_networkGraph);
  dp.property("label", distance);

  boost::write_graphviz_dp(graphStream, m_networkGraph, dp);
  #endif
}

} /* namespace ns3 */
