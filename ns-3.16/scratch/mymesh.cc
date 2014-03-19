/*
 *  m_xSize * step
 *  |<--------->|
 *   step
 *  |<--->|
 *  * --- * --- * 
 *  | \   |   / |                ^
 *  |   \ | /   |                |
 *  * --- * --- * m_ySize * step |
 *  |   / | \   |                |
 *  | /   |   \ |                |
 *  * --- * --- *                _
 *  ^ Ping source
 *  This node pings all other nodes.
 */


#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mesh-module.h"
#include "ns3/mobility-module.h"
#include "ns3/mesh-helper.h"

#include <iostream>
#include <sstream>
#include <fstream>

using namespace ns3;

static void
CourseChange (std::string context, Ptr<const MobilityModel> position)
{
  Vector pos = position->GetPosition ();
  std::cout << Simulator::Now () << ", pos=" << position << ", x=" << pos.x << ", y=" << pos.y
            << ", z=" << pos.z << std::endl;
}

NS_LOG_COMPONENT_DEFINE ("TestMeshScript");
class MeshTest
{
public:
  MeshTest ();
  int Run ();
private:
  int       m_xSize;
  int       m_ySize;
  double    m_step;
  double    m_randomStart;
  double    m_totalTime;
  double    m_packetInterval;
  uint16_t  m_packetSize;
  uint32_t  m_nIfaces;
  bool      m_chan;
  std::string m_stack;
  std::string m_root;
  NodeContainer nodes;
  NetDeviceContainer meshDevices;
  Ipv4InterfaceContainer interfaces;
  MeshHelper mesh;

private:
  void CreateNodes ();
  void InstallInternetStack ();
  void InstallApplication ();
};

MeshTest::MeshTest () :
  m_xSize (4),
  m_ySize (4),
  m_step (100.0),
  m_randomStart (0.1),
  m_totalTime (100.0),
  m_packetInterval (0.1),
  m_packetSize (1024),
  m_nIfaces (1),
  m_chan (true),
  m_stack ("ns3::Dot11sStack"),
  m_root ("ff:ff:ff:ff:ff:ff")
{
}

void
MeshTest::CreateNodes ()
{ 
  nodes.Create (m_ySize*m_xSize);
  
  YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default ();
  YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default ();
  wifiPhy.SetChannel (wifiChannel.Create ());
  
  mesh = MeshHelper::Default ();
  if (!Mac48Address (m_root.c_str ()).IsBroadcast ())
    {
      mesh.SetStackInstaller (m_stack, "Root", Mac48AddressValue (Mac48Address (m_root.c_str ())));
    }
  else
    {
      mesh.SetStackInstaller (m_stack);
    }
  if (m_chan)
    {
      mesh.SetSpreadInterfaceChannels (MeshHelper::SPREAD_CHANNELS);
    }
  else
    {
      mesh.SetSpreadInterfaceChannels (MeshHelper::ZERO_CHANNEL);
    }
  mesh.SetMacType ("RandomStart", TimeValue (Seconds (m_randomStart)));
  
  mesh.SetNumberOfInterfaces (m_nIfaces);
  
  meshDevices = mesh.Install (wifiPhy, nodes);
  
  MobilityHelper mobility;

  mobility.SetPositionAllocator ("ns3::RandomDiscPositionAllocator",
                                 "X", StringValue ("100.0"),
                                 "Y", StringValue ("100.0"),
                                 "Rho", StringValue ("ns3::UniformRandomVariable[Min=0|Max=30]"));
  mobility.SetMobilityModel ("ns3::RandomWalk2dMobilityModel",
                             "Mode", StringValue ("Time"),
                             "Time", StringValue ("2s"),
                             "Speed", StringValue ("ns3::ConstantRandomVariable[Constant=1.0]"),
                             "Bounds", StringValue ("0|200|0|200"));
  mobility.Install (nodes);
  Config::Connect ("/NodeList/*/$ns3::MobilityModel/CourseChange",
                   MakeCallback (&CourseChange));
}

void
MeshTest::InstallInternetStack ()
{
  InternetStackHelper internetStack;
  internetStack.Install (nodes);
  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  interfaces = address.Assign (meshDevices);
}

void
MeshTest::InstallApplication ()
{
  UdpEchoServerHelper echoServer (9);
  ApplicationContainer serverApps = echoServer.Install (nodes.Get(1));
  serverApps.Start (Seconds (1.0));
  serverApps.Stop (Seconds (10.0));

  for(int i = 1; i < m_ySize*m_xSize; i++) {
  	UdpEchoClientHelper echoClient (interfaces.GetAddress (i), 9);
  	echoClient.SetAttribute ("MaxPackets", UintegerValue (1));
  	echoClient.SetAttribute ("Interval", TimeValue (Seconds (1.0)));
  	echoClient.SetAttribute ("PacketSize", UintegerValue (1024));
  	ApplicationContainer clientApp = echoClient.Install(nodes.Get(0));
  	clientApp.Start (Seconds (i*3));
  	clientApp.Stop (Seconds (i*3 + 10));
  }


}

int
MeshTest::Run ()
{
  CreateNodes ();
  InstallInternetStack ();
  InstallApplication ();
  Simulator::Stop (Seconds (m_totalTime));
  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}

int
main (int argc, char *argv[])
{
  MeshTest t; 
  return t.Run ();
}
