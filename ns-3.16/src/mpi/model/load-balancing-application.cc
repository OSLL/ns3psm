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
#include "ns3/channel.h"
#include "mpi-interface.h"
#include <boost/graph/graphviz.hpp>
#include <boost/graph/adj_list_serialize.hpp>
#include <utility>

#include "lso_cluster_func.hpp"

NS_LOG_COMPONENT_DEFINE ("LoadBalancingApplication");

namespace ns3 {

int get(global_value_owner_map, global_value k) {
  return k.processor;
}

NS_OBJECT_ENSURE_REGISTERED (LoadBalancingApplication);

TypeId
LoadBalancingApplication::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::LoadBalancingApplication")
    .SetParent<Application> ()
    .AddConstructor<LoadBalancingApplication> ()
    .AddAttribute ("ReclusteringInterval", "Reclustering interval.",
                   TimeValue (Seconds (10)),
                   MakeTimeAccessor (&LoadBalancingApplication::m_reclusteringInterval),
                   MakeTimeChecker ())
  ;
  return tid;
}


LoadBalancingApplication::LoadBalancingApplication ()
  : m_reclusteringInterval (Seconds (10)),
    m_clusterLoad (0),
    m_iterationNum (0)
{
  NS_LOG_FUNCTION (this);

  char** argv; int argc = 0;
  boost::mpi::environment env(argc, argv);
  boost::mpi::communicator world;

  process_group pg(world);
  m_mpiProcessId = process_id(pg);
  m_mpiNumProcesses = num_processes(pg);
  m_mpiTaskQueue = new dist_queue_type(pg, global_value_owner_map());
}

LoadBalancingApplication::~LoadBalancingApplication()
{
  NS_LOG_FUNCTION (this);
  delete m_mpiTaskQueue;
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
  if ((context >= 0) && (context < m_networkGraphVertexMap.size ()))
  {
    boost::put(boost::vertex_color,
               m_networkGraph,
               m_networkGraphVertexMap[context],
               boost::get(boost::vertex_color,
                          m_networkGraph,
                          m_networkGraphVertexMap[context]) + 1);
  }
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

  /*if (m_reclusteringEvent.IsRunning ())
    {
	  // TODO do smth
    }
  Simulator::Cancel (m_reclusteringEvent);*/
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

  std::cout << "Reclustering" << std::endl;
  MergeNetworkGraph ();
  m_iterationNum ++;
  ScheduleReclusteringEvent ();
}

void
LoadBalancingApplication::CreateNetworkGraph (void)
{
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
}

void
LoadBalancingApplication::UpdateNetworkGraph ()
{
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
}

void
LoadBalancingApplication::WriteNetworkGraph (const std::string& filename)
{
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
}

void
LoadBalancingApplication::MergeNetworkGraph ()
{
  UpdateNetworkGraph ();

  if (m_mpiProcessId != 0)
  {
      global_value v(0, m_networkGraph);
      m_mpiTaskQueue->push(v);
  }

  double max_traffic = 0;

  while (!m_mpiTaskQueue->empty())
  {
    global_value v = m_mpiTaskQueue->top(); m_mpiTaskQueue->pop();
    graph_t tmp = v.value;

    // Merging nodes load

    graph_vertex_iterator sti, eni;
    boost::tie(sti, eni) = boost::vertices(tmp);
    std::vector<vertex_descriptor> vertex_list(sti, eni);

    for (uint32_t i = 0; i < vertex_list.size(); ++i) {
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

}

void
LoadBalancingApplication::ClusterNetworkGraph ()
{

  {
    std::ofstream forClusterStream(std::string("example.txt").c_str());

    graph_edge_iterator st, en;
    boost::tie(st, en) = boost::edges(m_networkGraph);
    std::vector<edge_descriptor> edge_list(st, en);

    for (uint32_t i = 0; i < edge_list.size(); ++i)
    {
      forClusterStream << boost::get(boost::vertex_index, m_networkGraph, boost::source(edge_list[i], m_networkGraph));
      forClusterStream << " ";
      forClusterStream << boost::get(boost::vertex_index, m_networkGraph, boost::target(edge_list[i], m_networkGraph));
      forClusterStream << " ";
      forClusterStream << boost::get(boost::edge_weight, m_networkGraph, edge_list[i]);
      forClusterStream << "\n";
    }
  }

  std::string num_clusters_string = boost::lexical_cast<std::string>(m_mpiNumProcesses);
  char const *num_clusters_char = num_clusters_string.c_str();

  const char* argv[] = {"example.txt",
                       "--loss",
                       "ncut",//"modularity",
                       "--num_clusters",
                       num_clusters_char,
                       NULL};

  int argc = sizeof(argv) / sizeof(char*) - 1;


  try {
    // parse arguments, and run the clustering algorithm
    ParamSourceCommandline param_source(argc, argv);
    LsoMainFunctionCommandLine runner;
    runner.add_all_parameters(param_source);
    runner.run();

    //first - node id
    //second first - old cluster
    //second second - new cluster
    std::vector<std::pair <uint32_t, std::pair <uint32_t, uint32_t> > > nodes_for_moving;

    for (size_t i = 0 ; i < runner.clustering.size() ; ++i)
    {
      uint32_t old_node = boost::get (boost::vertex_distance, m_networkGraph, boost::vertex (i, m_networkGraph));
      uint32_t new_node = runner.clustering[i];
      if (old_node != new_node)
      {
         std::pair <uint32_t, uint32_t> nodes (old_node, new_node);
         std::pair <uint32_t, std::pair<uint32_t, uint32_t> > node_for_moving (i, nodes);
         nodes_for_moving.push_back(node_for_moving);

         boost::put (boost::vertex_distance,
                     m_networkGraph,
                     boost::vertex (i, m_networkGraph),
                     new_node);

         std::cout << "node " << i << " change cluster node from " << old_node << " to " << new_node << std::endl;
      }

    }

    WriteClusterGraph (std::string("cluster"));

  } catch (std::exception const& e)
  {
	  std::cerr << e.what() << std::endl;
    return;
  } catch (...)
  {
	  std::cerr << "Unexpected error" << std::endl;
    return;
  }
}

void
LoadBalancingApplication::WriteClusterGraph (const std::string& filename)
{
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

}

} /* namespace ns3 */
