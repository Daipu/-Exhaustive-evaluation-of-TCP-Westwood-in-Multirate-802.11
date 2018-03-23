/* * This is a simple example to test TcpWestwood in the presence of "Arf","Aarf","Onoe" 
*
* Network topology:
*   
*
*           n4(STA)
*            |
*            *
*(STA)n1---* N0 (Ap )*----  n3(STA) 
*            *
*            |
*           n2(STA)
*
* We report the total throughput received during a window of 100ms.
* The user can specify the application data rate and choose the variant
* of TCP i.e. congestion control algorithm to use.
*/

#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/flow-classifier.h"

NS_LOG_COMPONENT_DEFINE ("wifi-tcp");

using namespace ns3;

Ptr<PacketSink> sink;                         /* Pointer to the packet sink application */
uint64_t lastTotalRx = 0;                     /* The value of the last total received bytes */

void
CalculateThroughput ()
{
   Time now = Simulator::Now ();                                         /* Return the simulator's virtual time. */
   double cur = (sink->GetTotalRx () - lastTotalRx) * (double) 8 / 1e5;     /* Convert Application RX Packets to MBits. */
   std::cout << now.GetSeconds () << "s: \t" << cur << " Mbit/s" << std::endl;
   lastTotalRx = sink->GetTotalRx ();
   Simulator::Schedule (MilliSeconds (100), &CalculateThroughput);
}

int
main (int argc, char *argv[])
{
  uint32_t payloadSize = 1472;                       /* Transport layer payload size in bytes. */
  std::string dataRate = "100Mbps";                  /* Application layer datarate. */
  std::string tcpVariant = "TcpVeno";             /* TCP variant type. */                    /* Physical layer bitrate. */
  std::string wifiManager ("Arf");
  double simulationTime = 10;                        /* Simulation time in seconds. */
  bool pcapTracing = true;                          /* PCAP Tracing is enabled or not. */

  /* Command line argument parser setup. */
  CommandLine cmd;
  cmd.AddValue ("payloadSize", "Payload size in bytes", payloadSize);
  cmd.AddValue ("dataRate", "Application data ate", dataRate);
  cmd.AddValue ("tcpVariant", "Transport protocol to use: TcpNewReno, "
                "TcpHybla, TcpHighSpeed, TcpHtcp, TcpVegas, TcpScalable, TcpVeno, "
                "TcpBic, TcpYeah, TcpIllinois, TcpWestwood, TcpWestwoodPlus, TcpLedbat ", tcpVariant);
  cmd.AddValue ("simulationTime", "Simulation time in seconds", simulationTime);
  cmd.AddValue ("pcap", "Enable/disable PCAP Tracing", pcapTracing);
  cmd.AddValue ("wifiManager", "Set wifi rate manager (Aarf, Aarfcd, Amrr, Arf, Cara, Ideal, Minstrel, Onoe, Rraa)", wifiManager);
  cmd.Parse (argc, argv);

  tcpVariant = std::string ("ns3::") + tcpVariant;

  /* No fragmentation and no RTS/CTS */
  Config::SetDefault ("ns3::WifiRemoteStationManager::FragmentationThreshold", StringValue ("999999"));
  Config::SetDefault ("ns3::WifiRemoteStationManager::RtsCtsThreshold", StringValue ("999999"));

  // Select TCP variant
  if (tcpVariant.compare ("ns3::TcpWestwoodPlus") == 0)
    { 
      // TcpWestwoodPlus is not an actual TypeId name; we need TcpWestwood here
      Config::SetDefault ("ns3::TcpL4Protocol::SocketType", TypeIdValue (TcpWestwood::GetTypeId ()));
      // the default protocol type in ns3::TcpWestwood is WESTWOOD
      Config::SetDefault ("ns3::TcpWestwood::ProtocolType", EnumValue (TcpWestwood::WESTWOODPLUS));
    }
  else
    {
      TypeId tcpTid;
      NS_ABORT_MSG_UNLESS (TypeId::LookupByNameFailSafe (tcpVariant, &tcpTid), "TypeId " << tcpVariant << " not found");
      Config::SetDefault ("ns3::TcpL4Protocol::SocketType", TypeIdValue (TypeId::LookupByName (tcpVariant)));
    }

  /* Configure TCP Options */
  Config::SetDefault ("ns3::TcpSocket::SegmentSize", UintegerValue (payloadSize));

  WifiMacHelper wifiMac;
  WifiHelper wifiHelper;
  wifiHelper.SetStandard (WIFI_PHY_STANDARD_80211b);

  /* Set up Legacy Channel */
  YansWifiChannelHelper wifiChannel;
  wifiChannel.SetPropagationDelay ("ns3::ConstantSpeedPropagationDelayModel");
  wifiChannel.AddPropagationLoss ("ns3::FriisPropagationLossModel", "Frequency", DoubleValue (5e9));

  /* Setup Physical Layer */
  YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default ();
  wifiPhy.SetChannel (wifiChannel.Create ());
  wifiPhy.Set ("TxPowerStart", DoubleValue (10.0));
  wifiPhy.Set ("TxPowerEnd", DoubleValue (10.0));
  wifiPhy.Set ("TxPowerLevels", UintegerValue (1));
  wifiPhy.Set ("TxGain", DoubleValue (0));
  wifiPhy.Set ("RxGain", DoubleValue (0));
  wifiPhy.Set ("RxNoiseFigure", DoubleValue (10));
  wifiPhy.Set ("CcaMode1Threshold", DoubleValue (-79));
  wifiPhy.Set ("EnergyDetectionThreshold", DoubleValue (-79 + 3));
  wifiPhy.SetErrorRateModel ("ns3::YansErrorRateModel");
  wifiHelper.SetRemoteStationManager ("ns3::" + wifiManager + "WifiManager");

  NodeContainer networkNodes; // create 5 nodes
  networkNodes.Create (5);
  Ptr<Node> apWifiNode   = networkNodes.Get (0);
  Ptr<Node> staWifiNode1 = networkNodes.Get (1);
  Ptr<Node> staWifiNode2 = networkNodes.Get (2);
  Ptr<Node> staWifiNode3 = networkNodes.Get (3);
  Ptr<Node> staWifiNode4 = networkNodes.Get (4);

  /* Configure AP */
  Ssid ssid = Ssid ("network");
  wifiMac.SetType ("ns3::ApWifiMac",
                   "Ssid", SsidValue (ssid));

  NetDeviceContainer apDevice;
  apDevice = wifiHelper.Install (wifiPhy, wifiMac, apWifiNode);

  /* Configure STA */
  wifiMac.SetType ("ns3::StaWifiMac",
                   "Ssid", SsidValue (ssid));

  NetDeviceContainer staDevices1, staDevices2, staDevices3, staDevices4;
  staDevices1 = wifiHelper.Install (wifiPhy, wifiMac, staWifiNode1);

  staDevices2 = wifiHelper.Install (wifiPhy, wifiMac, staWifiNode2);

  staDevices3 = wifiHelper.Install (wifiPhy, wifiMac, staWifiNode3);

  staDevices4 = wifiHelper.Install (wifiPhy, wifiMac, staWifiNode4);

  /* Mobility model */
  MobilityHelper mobility;
  Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator> ();
  positionAlloc->Add (Vector (100, 60,   20));
  positionAlloc->Add (Vector (80,  100,  20));
  positionAlloc->Add (Vector (50,  100,  20));
  positionAlloc->Add (Vector (120, 100,  20));
  positionAlloc->Add (Vector (140, 100,  20));


  mobility.SetPositionAllocator (positionAlloc);
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (apWifiNode);
  mobility.Install (staWifiNode1);
  mobility.Install (staWifiNode2);
  mobility.Install (staWifiNode3);
  mobility.Install (staWifiNode4);

  /* Internet stack */
  InternetStackHelper stack;
  stack.Install (networkNodes);

  Ipv4AddressHelper address;
  address.SetBase ("10.0.0.0", "255.255.255.0");
  Ipv4InterfaceContainer apInterface;
  apInterface = address.Assign (apDevice);
  Ipv4InterfaceContainer staInterface1, staInterface2, staInterface3,staInterface4;
  staInterface1 = address.Assign (staDevices1);
  staInterface2 = address.Assign (staDevices2);
  staInterface3 = address.Assign (staDevices3);
  staInterface4 = address.Assign (staDevices4);

  /* Populate routing table */
  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  /* Install TCP Receiver on the access point */
  PacketSinkHelper sinkHelper ("ns3::TcpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), 9));
  ApplicationContainer sinkApp = sinkHelper.Install (apWifiNode);
  sink = StaticCast<PacketSink> (sinkApp.Get (0));

  /* Install TCP/UDP Transmitter on the station */
  OnOffHelper server ("ns3::TcpSocketFactory", (InetSocketAddress (apInterface.GetAddress (0), 9)));
  server.SetAttribute ("PacketSize", UintegerValue (payloadSize));
  server.SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=1]"));
  server.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0]"));
  server.SetAttribute ("DataRate", DataRateValue (DataRate (dataRate)));

  ApplicationContainer serverApp1 = server.Install (staWifiNode1);

  ApplicationContainer serverApp2 = server.Install (staWifiNode2);

  ApplicationContainer serverApp3 = server.Install (staWifiNode3);



  OnOffHelper server1 ("ns3::TcpSocketFactory", (InetSocketAddress (apInterface.GetAddress (0), 9)));
  server1.SetAttribute ("PacketSize", UintegerValue (payloadSize));
  server1.SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=1]"));
  server1.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0]"));
  server1.SetAttribute ("DataRate", DataRateValue (DataRate (dataRate)));

  ApplicationContainer serverApp4 = server1.Install (staWifiNode4);

  /* Start Applications */
  sinkApp.Start (Seconds (0.0));
  serverApp1.Start (Seconds (1.0));
  serverApp2.Start (Seconds (1.0));
  serverApp3.Start (Seconds (1.0));
  serverApp4.Start (Seconds (1.0));

  Simulator::Schedule (Seconds (1.1), &CalculateThroughput);

  /* Enable Traces */
  if (pcapTracing)
    {
      wifiPhy.SetPcapDataLinkType (WifiPhyHelper::DLT_IEEE802_11_RADIO);
      wifiPhy.EnablePcap ("AccessPoint", apDevice);
    }

  //install flowmonitor on all nodes 
  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll();
   
  /* Start Simulation */
  Simulator::Stop (Seconds (simulationTime + 1));
  Simulator::Run ();
  monitor->CheckForLostPackets();
  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowmon.GetClassifier ());
  std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats();
  for(std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin(); i!= stats.end();i++)
  {
          Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(i->first);
          std::cout<<"Flow"<<i->first <<"("<<t.sourceAddress<<"->"<<t.destinationAddress <<")\n";
          std::cout<<" Tx Bytes: "<<i->second.txBytes<<"\n";
          std::cout<<" Rx Bytes: "<<i->second.rxBytes<<"\n";
          std::cout << "  Tx Packets: " << i->second.txPackets << "\n";
          std::cout << "  Rx Packets: " << i->second.rxPackets << "\n";
          std::cout << "  lostPackets: " << i->second.lostPackets << "\n";
  }
 

  double averageThroughput = ((sink->GetTotalRx () * 8) / (1e6  * simulationTime));

  std::cout << "\nAverage throughput: " << averageThroughput << " Mbit/s" << std::endl;
   Simulator::Destroy ();
  return 0;
}
