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
 * (c) 2009, GTech Systems, Inc. - Alfred Park <park@gtech-systems.com>
 *
 * DARPA NMS Campus Network Model
 *
 * This topology replicates the original NMS Campus Network model
 * with the exception of chord links (which were never utilized in the
 * original model)
 * Link Bandwidths and Delays may not be the same as the original
 * specifications
 *
 * Modified for distributed simulation by Josh Pelkey <jpelkey@gatech.edu>
 *
 * The fundamental unit of the NMS model consists of a campus network. The
 * campus network topology can been seen here:
 * http://www.nsnam.org/~jpelkey3/nms.png
 * The number of hosts (default 42) is variable.  Finally, an arbitrary
 * number of these campus networks can be connected together (default 2)
 * to make very large simulations.
 */

// for timing functions
#include <cstdlib>
#include <sys/time.h>
#include <fstream>

#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/network-module.h"
#include "ns3/on-off-helper.h"
#include "ns3/packet-sink-helper.h"
#include "ns3/point-to-point-helper.h"
#include "ns3/mpi-interface.h"
#include "ns3/ipv4-static-routing-helper.h"
#include "ns3/ipv4-list-routing-helper.h"
#include "ns3/ipv4-nix-vector-helper.h"

#include "../model/load-balancing-application.h"

#include <boost/graph/graphviz.hpp>
#include <boost/graph/adjacency_list.hpp>
#include "ns3/node-container.h"

// vertex_name - for reading from dot
// vertex_color - load
// vertex_distance - # cluster node
typedef boost::property < boost::vertex_name_t, uint32_t, boost::property < boost::vertex_color_t, uint32_t, boost::property < boost::vertex_distance_t, uint32_t > > > vertex_p;
// edge_weight - traffic
// edge_weight2 - delay
typedef boost::property < boost::edge_weight_t, uint32_t, boost::property < boost::edge_weight2_t, int64_t > > edge_p;

typedef boost::adjacency_list <
                    boost::vecS,
                    boost::vecS,
                    boost::undirectedS,
                    vertex_p,
                    edge_p
> graph_nms_t; //graph type

typedef boost:: graph_traits<  graph_nms_t >::vertex_descriptor vertex_descriptor;
typedef boost::graph_traits<  graph_nms_t >::edge_descriptor edge_descriptor;
typedef boost::graph_traits<  graph_nms_t >::adjacency_iterator graph_adjacency_iterator;
typedef boost::graph_traits<  graph_nms_t >::vertex_iterator graph_vertex_iterator;
typedef boost::graph_traits<  graph_nms_t >::edge_iterator graph_edge_iterator;

#ifdef NS3_MPI
#include <mpi.h>
#endif

using namespace ns3;

typedef struct timeval TIMER_TYPE;
#define TIMER_NOW(_t) gettimeofday (&_t,NULL);
#define TIMER_SECONDS(_t) ((double)(_t).tv_sec + (_t).tv_usec * 1e-6)
#define TIMER_DIFF(_t1, _t2) (TIMER_SECONDS (_t1) - TIMER_SECONDS (_t2))

NS_LOG_COMPONENT_DEFINE ("CampusNetworkModelDistributed");

int
main (int argc, char *argv[])
{
#ifdef NS3_MPI
  // Enable MPI with the command line arguments
  MpiInterface::Enable (&argc, &argv);

  TIMER_TYPE t0, t1, t2;
  TIMER_NOW (t0);
  std::cout << " ==== DARPA NMS CAMPUS NETWORK SIMULATION ====" << std::endl;

  GlobalValue::Bind ("SimulatorImplementationType",
                     StringValue ("ns3::DistributedSimulatorImpl"));

  uint32_t systemId = MpiInterface::GetSystemId ();
  uint32_t systemCount = MpiInterface::GetSize ();

  LogComponentEnable ("CampusNetworkModelDistributed", LOG_LEVEL_FUNCTION);

  //temporary fix see bug 1560
  #define nCN (2)
  #define nLANClients (42)
  //uint32_t nCN = 2, nLANClients = 42;
  int32_t single = 0;
  int nBytes = 500000; // Bytes for each on/off app
  bool nix = true;
  bool load = true;

  CommandLine cmd;
  //cmd.AddValue ("CN", "Number of total CNs [2]", nCN);
  //cmd.AddValue ("LAN", "Number of nodes per LAN [42]", nLANClients);
  cmd.AddValue ("single", "1 if use single flow", single);
  cmd.AddValue ("nBytes", "Number of bytes for each on/off app", nBytes);
  cmd.AddValue ("nix", "Toggle the use of nix-vector or global routing", nix);
  cmd.AddValue ("load", "load_balancing", load);
  cmd.Parse (argc,argv);

  if (nCN < 2)
    {
      std::cout << "Number of total CNs (" << nCN << ") lower than minimum of 2"
           << std::endl;
      return 1;
    }
  if (systemCount > nCN)
    {
      std::cout << "Number of total CNs (" << nCN << ") should be >= systemCount ("
           << systemCount << ")." << std::endl;
      return 1;
    }

  std::cout << "Number of CNs: " << nCN << ", LAN nodes: " << nLANClients << std::endl;

  NodeContainer nodes_net0[nCN][3], nodes_net1[nCN][6], nodes_netLR[nCN],
                nodes_net2[nCN][14], nodes_net2LAN[nCN][7][nLANClients],
                nodes_net3[nCN][9], nodes_net3LAN[nCN][5][nLANClients];

  uint32_t clusters_net0[nCN][3], clusters_net1[nCN][6], clusters_netLR[2 * nCN],
  clusters_net2[nCN][14], clusters_net2LAN[nCN][7][nLANClients],
  clusters_net3[nCN][9], clusters_net3LAN[nCN][5][nLANClients];

  graph_nms_t g;

  boost::dynamic_properties dp;

  boost::property_map<graph_nms_t, boost::vertex_name_t>::type name =
  boost::get(boost::vertex_name, g);
  dp.property("node_id", name);

  boost::property_map<graph_nms_t, boost::vertex_distance_t>::type color =
  boost::get(boost::vertex_distance, g);
  dp.property("label", color);

  std::ifstream res_file("graph_cl.dot");
  boost::read_graphviz(res_file, g, dp, "node_id");

  int node_num = 0;

  for (uint32_t z = 0; z < nCN; ++z)
      {
        for (int i = 0; i < 3; ++i)
          {
            clusters_net0[z][i] = boost::get(boost::vertex_distance, g, boost::get(boost::vertex_index, g, node_num++));
          }

        for (int i = 0; i < 6; ++i)
          {
            clusters_net1[z][i] =  boost::get(boost::vertex_distance, g, boost::get(boost::vertex_index, g, node_num++));
          }

        for (int i = 0; i < 14; ++i)
          {
            clusters_net2[z][i] =  boost::get(boost::vertex_distance, g, boost::get(boost::vertex_index, g, node_num++));
          }
        for (int i = 0; i < 7; ++i)
          {

            for (uint32_t j = 0; j < nLANClients; ++j)
              {
                clusters_net2LAN[z][i][j] =  boost::get(boost::vertex_distance, g, boost::get(boost::vertex_index, g, node_num++));
              }
          }

        for (int i = 0; i < 9; ++i)
          {
            clusters_net3[z][i] =  boost::get(boost::vertex_distance, g, boost::get(boost::vertex_index, g, node_num++));
          }

        for (int i = 0; i < 5; ++i)
          {
            for (uint32_t j = 0; j < nLANClients; ++j)
              {
                clusters_net3LAN[z][i][j] =  boost::get(boost::vertex_distance, g, boost::get(boost::vertex_index, g, node_num++));
              }
          }

        clusters_netLR[2 * z] =  boost::get(boost::vertex_distance, g, boost::get(boost::vertex_index, g, node_num++));
        clusters_netLR[2 * z + 1] =  boost::get(boost::vertex_distance, g, boost::get(boost::vertex_index, g, node_num++));
      }




  PointToPointHelper p2p_2gb200ms, p2p_1gb5ms, p2p_100mb1ms;
  InternetStackHelper stack;
  Ipv4InterfaceContainer ifs, ifs0[nCN][3], ifs1[nCN][6], ifs2[nCN][14],
                         ifs3[nCN][9], ifs2LAN[nCN][7][nLANClients],
                         ifs3LAN[nCN][5][nLANClients];
  Ipv4AddressHelper address;
  std::ostringstream oss;
  p2p_1gb5ms.SetDeviceAttribute ("DataRate", StringValue ("1Gbps"));
  p2p_1gb5ms.SetChannelAttribute ("Delay", StringValue ("5ms"));
  p2p_2gb200ms.SetDeviceAttribute ("DataRate", StringValue ("2Gbps"));
  p2p_2gb200ms.SetChannelAttribute ("Delay", StringValue ("200ms"));
  p2p_100mb1ms.SetDeviceAttribute ("DataRate", StringValue ("100Mbps"));
  p2p_100mb1ms.SetChannelAttribute ("Delay", StringValue ("1ms"));

  Ipv4NixVectorHelper nixRouting;
  Ipv4StaticRoutingHelper staticRouting;

  Ipv4ListRoutingHelper list;
  list.Add (staticRouting, 0);
  list.Add (nixRouting, 10);

  if (nix)
    {
      stack.SetRoutingHelper (list); // has effect on the next Install ()
    }


  // Create Campus Networks
  for (uint32_t z = 0; z < nCN; ++z)
    {
      std::cout << "Creating Campus Network " << z << ":" << std::endl;
      // Create Net0
      std::cout << "  SubNet [ 0";
      for (int i = 0; i < 3; ++i)
        {
          Ptr<Node> node = CreateObject<Node> (clusters_net0[z][i]);
          nodes_net0[z][i].Add (node);
          stack.Install (nodes_net0[z][i]);
        }
      nodes_net0[z][0].Add (nodes_net0[z][1].Get (0));
      nodes_net0[z][1].Add (nodes_net0[z][2].Get (0));
      nodes_net0[z][2].Add (nodes_net0[z][0].Get (0));
      NetDeviceContainer ndc0[3];
      for (int i = 0; i < 3; ++i)
        {
          ndc0[i] = p2p_1gb5ms.Install (nodes_net0[z][i]);
        }
      // Create Net1
      std::cout << " 1";
      for (int i = 0; i < 6; ++i)
        {
          Ptr<Node> node = CreateObject<Node> (clusters_net1[z][i]);
          nodes_net1[z][i].Add (node);
          stack.Install (nodes_net1[z][i]);
        }
      nodes_net1[z][0].Add (nodes_net1[z][1].Get (0));
      nodes_net1[z][2].Add (nodes_net1[z][0].Get (0));
      nodes_net1[z][3].Add (nodes_net1[z][0].Get (0));
      nodes_net1[z][4].Add (nodes_net1[z][1].Get (0));
      nodes_net1[z][5].Add (nodes_net1[z][1].Get (0));
      NetDeviceContainer ndc1[6];
      for (int i = 0; i < 6; ++i)
        {
          if (i == 1)
            {
              continue;
            }
          ndc1[i] = p2p_1gb5ms.Install (nodes_net1[z][i]);
        }
      // Connect Net0 <-> Net1
      NodeContainer net0_1;
      net0_1.Add (nodes_net0[z][2].Get (0));
      net0_1.Add (nodes_net1[z][0].Get (0));
      NetDeviceContainer ndc0_1;
      ndc0_1 = p2p_1gb5ms.Install (net0_1);
      oss.str ("");
      oss << 10 + z << ".1.252.0";
      address.SetBase (oss.str ().c_str (), "255.255.255.0");
      ifs = address.Assign (ndc0_1);
      // Create Net2
      std::cout << " 2";
      for (int i = 0; i < 14; ++i)
        {
          Ptr<Node> node = CreateObject<Node> (clusters_net2[z][i]);
          nodes_net2[z][i].Add (node);
          stack.Install (nodes_net2[z][i]);
        }
      nodes_net2[z][0].Add (nodes_net2[z][1].Get (0));
      nodes_net2[z][2].Add (nodes_net2[z][0].Get (0));
      nodes_net2[z][1].Add (nodes_net2[z][3].Get (0));
      nodes_net2[z][3].Add (nodes_net2[z][2].Get (0));
      nodes_net2[z][4].Add (nodes_net2[z][2].Get (0));
      nodes_net2[z][5].Add (nodes_net2[z][3].Get (0));
      nodes_net2[z][6].Add (nodes_net2[z][5].Get (0));
      nodes_net2[z][7].Add (nodes_net2[z][2].Get (0));
      nodes_net2[z][8].Add (nodes_net2[z][3].Get (0));
      nodes_net2[z][9].Add (nodes_net2[z][4].Get (0));
      nodes_net2[z][10].Add (nodes_net2[z][5].Get (0));
      nodes_net2[z][11].Add (nodes_net2[z][6].Get (0));
      nodes_net2[z][12].Add (nodes_net2[z][6].Get (0));
      nodes_net2[z][13].Add (nodes_net2[z][6].Get (0));
      NetDeviceContainer ndc2[14];
      for (int i = 0; i < 14; ++i)
        {
          ndc2[i] = p2p_1gb5ms.Install (nodes_net2[z][i]);
        }
      NetDeviceContainer ndc2LAN[7][nLANClients];
      for (int i = 0; i < 7; ++i)
        {
          oss.str ("");
          oss << 10 + z << ".4." << 15 + i << ".0";
          address.SetBase (oss.str ().c_str (), "255.255.255.0");
          for (uint32_t j = 0; j < nLANClients; ++j)
            {
              Ptr<Node> node = CreateObject<Node> (clusters_net2LAN[z][i][j]);
              nodes_net2LAN[z][i][j].Add (node);
              stack.Install (nodes_net2LAN[z][i][j]);
              nodes_net2LAN[z][i][j].Add (nodes_net2[z][i + 7].Get (0));
              ndc2LAN[i][j] = p2p_100mb1ms.Install (nodes_net2LAN[z][i][j]);
              ifs2LAN[z][i][j] = address.Assign (ndc2LAN[i][j]);
            }
        }
      // Create Net3
      std::cout << " 3 ]" << std::endl;
      for (int i = 0; i < 9; ++i)
        {
          Ptr<Node> node = CreateObject<Node> (clusters_net3[z][i]);
          nodes_net3[z][i].Add (node);
          stack.Install (nodes_net3[z][i]);
        }
      nodes_net3[z][0].Add (nodes_net3[z][1].Get (0));
      nodes_net3[z][1].Add (nodes_net3[z][2].Get (0));
      nodes_net3[z][2].Add (nodes_net3[z][3].Get (0));
      nodes_net3[z][3].Add (nodes_net3[z][1].Get (0));
      nodes_net3[z][4].Add (nodes_net3[z][0].Get (0));
      nodes_net3[z][5].Add (nodes_net3[z][0].Get (0));
      nodes_net3[z][6].Add (nodes_net3[z][2].Get (0));
      nodes_net3[z][7].Add (nodes_net3[z][3].Get (0));
      nodes_net3[z][8].Add (nodes_net3[z][3].Get (0));
      NetDeviceContainer ndc3[9];
      for (int i = 0; i < 9; ++i)
        {
          ndc3[i] = p2p_1gb5ms.Install (nodes_net3[z][i]);
        }
      NetDeviceContainer ndc3LAN[5][nLANClients];
      for (int i = 0; i < 5; ++i)
        {
          oss.str ("");
          oss << 10 + z << ".5." << 10 + i << ".0";
          address.SetBase (oss.str ().c_str (), "255.255.255.255");
          for (uint32_t j = 0; j < nLANClients; ++j)
            {
              Ptr<Node> node = CreateObject<Node> (clusters_net3LAN[z][i][j]);
              nodes_net3LAN[z][i][j].Add (node);
              stack.Install (nodes_net3LAN[z][i][j]);
              nodes_net3LAN[z][i][j].Add (nodes_net3[z][i + 4].Get (0));
              ndc3LAN[i][j] = p2p_100mb1ms.Install (nodes_net3LAN[z][i][j]);
              ifs3LAN[z][i][j] = address.Assign (ndc3LAN[i][j]);
            }
        }
      std::cout << "  Connecting Subnets..." << std::endl;
      // Create Lone Routers (Node 4 & 5)
      Ptr<Node> node1 = CreateObject<Node> (clusters_netLR[2 * z]);
      Ptr<Node> node2 = CreateObject<Node> (clusters_netLR[2 * z + 1]);
      nodes_netLR[z].Add (node1);
      nodes_netLR[z].Add (node2);
      stack.Install (nodes_netLR[z]);
      NetDeviceContainer ndcLR;
      ndcLR = p2p_1gb5ms.Install (nodes_netLR[z]);
      // Connect Net2/Net3 through Lone Routers to Net0
      NodeContainer net0_4, net0_5, net2_4a, net2_4b, net3_5a, net3_5b;
      net0_4.Add (nodes_netLR[z].Get (0));
      net0_4.Add (nodes_net0[z][0].Get (0));
      net0_5.Add (nodes_netLR[z].Get (1));
      net0_5.Add (nodes_net0[z][1].Get (0));
      net2_4a.Add (nodes_netLR[z].Get (0));
      net2_4a.Add (nodes_net2[z][0].Get (0));
      net2_4b.Add (nodes_netLR[z].Get (1));
      net2_4b.Add (nodes_net2[z][1].Get (0));
      net3_5a.Add (nodes_netLR[z].Get (1));
      net3_5a.Add (nodes_net3[z][0].Get (0));
      net3_5b.Add (nodes_netLR[z].Get (1));
      net3_5b.Add (nodes_net3[z][1].Get (0));
      NetDeviceContainer ndc0_4, ndc0_5, ndc2_4a, ndc2_4b, ndc3_5a, ndc3_5b;
      ndc0_4 = p2p_1gb5ms.Install (net0_4);
      oss.str ("");
      oss << 10 + z << ".1.253.0";
      address.SetBase (oss.str ().c_str (), "255.255.255.0");
      ifs = address.Assign (ndc0_4);
      ndc0_5 = p2p_1gb5ms.Install (net0_5);
      oss.str ("");
      oss << 10 + z << ".1.254.0";
      address.SetBase (oss.str ().c_str (), "255.255.255.0");
      ifs = address.Assign (ndc0_5);
      ndc2_4a = p2p_1gb5ms.Install (net2_4a);
      oss.str ("");
      oss << 10 + z << ".4.253.0";
      address.SetBase (oss.str ().c_str (), "255.255.255.0");
      ifs = address.Assign (ndc2_4a);
      ndc2_4b = p2p_1gb5ms.Install (net2_4b);
      oss.str ("");
      oss << 10 + z << ".4.254.0";
      address.SetBase (oss.str ().c_str (), "255.255.255.0");
      ifs = address.Assign (ndc2_4b);
      ndc3_5a = p2p_1gb5ms.Install (net3_5a);
      oss.str ("");
      oss << 10 + z << ".5.253.0";
      address.SetBase (oss.str ().c_str (), "255.255.255.0");
      ifs = address.Assign (ndc3_5a);
      ndc3_5b = p2p_1gb5ms.Install (net3_5b);
      oss.str ("");
      oss << 10 + z << ".5.254.0";
      address.SetBase (oss.str ().c_str (), "255.255.255.0");
      ifs = address.Assign (ndc3_5b);
      // Assign IP addresses
      std::cout << "  Assigning IP addresses..." << std::endl;
      for (int i = 0; i < 3; ++i)
        {
          oss.str ("");
          oss << 10 + z << ".1." << 1 + i << ".0";
          address.SetBase (oss.str ().c_str (), "255.255.255.0");
          ifs0[z][i] = address.Assign (ndc0[i]);
        }
      for (int i = 0; i < 6; ++i)
        {
          if (i == 1)
            {
              continue;
            }
          oss.str ("");
          oss << 10 + z << ".2." << 1 + i << ".0";
          address.SetBase (oss.str ().c_str (), "255.255.255.0");
          ifs1[z][i] = address.Assign (ndc1[i]);
        }
      oss.str ("");
      oss << 10 + z << ".3.1.0";
      address.SetBase (oss.str ().c_str (), "255.255.255.0");
      ifs = address.Assign (ndcLR);
      for (int i = 0; i < 14; ++i)
        {
          oss.str ("");
          oss << 10 + z << ".4." << 1 + i << ".0";
          address.SetBase (oss.str ().c_str (), "255.255.255.0");
          ifs2[z][i] = address.Assign (ndc2[i]);
        }
      for (int i = 0; i < 9; ++i)
        {
          oss.str ("");
          oss << 10 + z << ".5." << 1 + i << ".0";
          address.SetBase (oss.str ().c_str (), "255.255.255.0");
          ifs3[z][i] = address.Assign (ndc3[i]);
        }
    }
  // Create Ring Links
  if (nCN > 1)
    {
      std::cout << "Forming Ring Topology..." << std::endl;
      NodeContainer nodes_ring[nCN];
      for (uint32_t z = 0; z < nCN - 1; ++z)
        {
          nodes_ring[z].Add (nodes_net0[z][0].Get (0));
          nodes_ring[z].Add (nodes_net0[z + 1][0].Get (0));
        }
      nodes_ring[nCN - 1].Add (nodes_net0[nCN - 1][0].Get (0));
      nodes_ring[nCN - 1].Add (nodes_net0[0][0].Get (0));
      NetDeviceContainer ndc_ring[nCN];
      for (uint32_t z = 0; z < nCN; ++z)
        {
          ndc_ring[z] = p2p_2gb200ms.Install (nodes_ring[z]);
          oss.str ("");
          oss << "254.1." << z + 1 << ".0";
          address.SetBase (oss.str ().c_str (), "255.255.255.0");
          ifs = address.Assign (ndc_ring[z]);
        }
    }

  // Create Traffic Flows
  std::cout << "Creating UDP Traffic Flows:" << std::endl;
  Config::SetDefault ("ns3::OnOffApplication::MaxBytes",
                      UintegerValue (nBytes));
  Config::SetDefault ("ns3::OnOffApplication::OnTime",
                      StringValue ("ns3::ConstantRandomVariable[Constant=1]"));
  Config::SetDefault ("ns3::OnOffApplication::OffTime",
                      StringValue ("ns3::ConstantRandomVariable[Constant=0]"));


  if (single)
    {
      if (systemCount == 1)
        {
          PacketSinkHelper sinkHelper ("ns3::UdpSocketFactory",
                                       InetSocketAddress (Ipv4Address::GetAny (),
                                                          9999));
          ApplicationContainer sinkApp = sinkHelper.Install (nodes_net1[0][2].Get (0));
          sinkApp.Start (Seconds (0.0));

          OnOffHelper client ("ns3::UdpSocketFactory", Address ());
          AddressValue remoteAddress (InetSocketAddress (ifs1[0][2].GetAddress (0), 9999));
          std::cout << "Remote Address is " << ifs1[0][2].GetAddress (0) << std::endl;
          client.SetAttribute ("Remote", remoteAddress);

          ApplicationContainer clientApp;
          clientApp.Add (client.Install (nodes_net2LAN[0][0][0].Get (0)));
          clientApp.Start (Seconds (0));
        }
      else if (systemId == 1)
        {
          PacketSinkHelper sinkHelper ("ns3::UdpSocketFactory",
                                       InetSocketAddress (Ipv4Address::GetAny (),
                                                          9999));
          ApplicationContainer sinkApp =
            sinkHelper.Install (nodes_net1[1][0].Get (0));

          sinkApp.Start (Seconds (0.0));
        }
      else if (systemId == 0)
        {
          OnOffHelper client ("ns3::UdpSocketFactory", Address ());
          AddressValue remoteAddress
            (InetSocketAddress (ifs1[1][0].GetAddress (0), 9999));

          std::cout << "Remote Address is " << ifs1[1][0].GetAddress (0) << std::endl;
          client.SetAttribute ("Remote", remoteAddress);

          ApplicationContainer clientApp;
          clientApp.Add (client.Install (nodes_net2LAN[0][0][0].Get (0)));
          clientApp.Start (Seconds (0));
        }
    }
  else
    {
      Ptr<UniformRandomVariable> urng = CreateObject<UniformRandomVariable> ();
      int r1;
      double r2;
      for (uint32_t z = 0; z < nCN; ++z)
        {
          uint32_t x = z + 1;
          if (z == nCN - 1)
            {
              x = 0;
            }
          // Subnet 2 LANs
          std::cout << "  Campus Network " << z << " Flows [ Net2 ";
          for (int i = 0; i < 7; ++i)
            {
              for (uint32_t j = 0; j < nLANClients; ++j)
                {
                  // Sinks
                  if (systemCount == 1)
                    {
                      PacketSinkHelper sinkHelper
                        ("ns3::UdpSocketFactory",
                        InetSocketAddress (Ipv4Address::GetAny (), 9999));

                      ApplicationContainer sinkApp =
                        sinkHelper.Install (nodes_net2LAN[z][i][j].Get (0));

                      sinkApp.Start (Seconds (0.0));
                    }
                  else if (systemId == z % systemCount)
                    {
                      PacketSinkHelper sinkHelper
                        ("ns3::UdpSocketFactory",
                        InetSocketAddress (Ipv4Address::GetAny (), 9999));

                      ApplicationContainer sinkApp =
                        sinkHelper.Install (nodes_net2LAN[z][i][j].Get (0));

                      sinkApp.Start (Seconds (0.0));
                    }
                  // Sources
                  if (systemCount == 1)
                    {
                      r1 = 2 + (int)(4 * urng->GetValue ());
                      r2 = 10 * urng->GetValue ();
                      OnOffHelper client ("ns3::UdpSocketFactory", Address ());

                      AddressValue remoteAddress
                        (InetSocketAddress (ifs2LAN[z][i][j].GetAddress (0), 9999));

                      client.SetAttribute ("Remote", remoteAddress);
                      ApplicationContainer clientApp;
                      clientApp.Add (client.Install (nodes_net1[x][r1].Get (0)));
                      clientApp.Start (Seconds (r2));
                    }
                  else if (systemId == x % systemCount)
                    {
                      r1 = 2 + (int)(4 * urng->GetValue ());
                      r2 = 10 * urng->GetValue ();
                      OnOffHelper client ("ns3::UdpSocketFactory", Address ());

                      AddressValue remoteAddress
                        (InetSocketAddress (ifs2LAN[z][i][j].GetAddress (0), 9999));

                      client.SetAttribute ("Remote", remoteAddress);
                      ApplicationContainer clientApp;
                      clientApp.Add (client.Install (nodes_net1[x][r1].Get (0)));
                      clientApp.Start (Seconds (r2));
                    }
                }
            }
          // Subnet 3 LANs
          std::cout << "Net3 ]" << std::endl;
          for (int i = 0; i < 5; ++i)
            {
              for (uint32_t j = 0; j < nLANClients; ++j)
                {
                  // Sinks
                  if (systemCount == 1)
                    {
                      PacketSinkHelper sinkHelper
                        ("ns3::UdpSocketFactory",
                        InetSocketAddress (Ipv4Address::GetAny (), 9999));

                      ApplicationContainer sinkApp =
                        sinkHelper.Install (nodes_net3LAN[z][i][j].Get (0));

                      sinkApp.Start (Seconds (0.0));
                    }
                  else if (systemId == z % systemCount)
                    {
                      PacketSinkHelper sinkHelper
                        ("ns3::UdpSocketFactory",
                        InetSocketAddress (Ipv4Address::GetAny (), 9999));

                      ApplicationContainer sinkApp =
                        sinkHelper.Install (nodes_net3LAN[z][i][j].Get (0));

                      sinkApp.Start (Seconds (0.0));
                    }
                  // Sources
                  if (systemCount == 1)
                    {
                      r1 = 2 + (int)(4 * urng->GetValue ());
                      r2 = 10 * urng->GetValue ();
                      OnOffHelper client ("ns3::UdpSocketFactory", Address ());

                      AddressValue remoteAddress
                        (InetSocketAddress (ifs3LAN[z][i][j].GetAddress (0), 9999));

                      client.SetAttribute ("Remote", remoteAddress);
                      ApplicationContainer clientApp;
                      clientApp.Add (client.Install (nodes_net1[x][r1].Get (0)));
                      clientApp.Start (Seconds (r2));
                    }
                  else if (systemId == x % systemCount)
                    {
                      r1 = 2 + (int)(4 * urng->GetValue ());
                      r2 = 10 * urng->GetValue ();
                      OnOffHelper client ("ns3::UdpSocketFactory", Address ());

                      AddressValue remoteAddress
                        (InetSocketAddress (ifs3LAN[z][i][j].GetAddress (0), 9999));

                      client.SetAttribute ("Remote", remoteAddress);
                      ApplicationContainer clientApp;
                      clientApp.Add (client.Install (nodes_net1[x][r1].Get (0)));
                      clientApp.Start (Seconds (r2));
                    }
                }
            }
        }
    }

  std::cout << "Created " << NodeList::GetNNodes () << " nodes." << std::endl;
  TIMER_TYPE routingStart;
  TIMER_NOW (routingStart);

  if (nix)
    {
      std::cout << "Using Nix-vectors..." << std::endl;
    }
  else
    {
      // Calculate routing tables
      std::cout << "Populating Routing tables..." << std::endl;
      Ipv4GlobalRoutingHelper::PopulateRoutingTables ();
    }

  TIMER_TYPE routingEnd;
  TIMER_NOW (routingEnd);
  std::cout << "Routing tables population took "
       << TIMER_DIFF (routingEnd, routingStart) << std::endl;

  std::cout << "Running simulator..." << std::endl;
  TIMER_NOW (t1);
  Simulator::Stop (Seconds (100.0));
  Simulator::Run ();
  TIMER_NOW (t2);
  std::cout << "Simulator finished." << std::endl;
  Simulator::Destroy ();
  // Exit the MPI execution environment
  MpiInterface::Disable ();
  double d1 = TIMER_DIFF (t1, t0), d2 = TIMER_DIFF (t2, t1);
  std::cout << "-----" << std::endl << "Runtime Stats:" << std::endl;
  std::cout << "Simulator init time: " << d1 << std::endl;
  std::cout << "Simulator run time: " << d2 << std::endl;
  std::cout << "Total elapsed time: " << d1 + d2 << std::endl;

  return 0;
#else
  NS_FATAL_ERROR ("Can't use distributed simulator without MPI compiled in");
#endif
}

