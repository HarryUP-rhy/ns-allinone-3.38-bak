/* -*-  Mode: C++; c-file-style: "gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2018 Piotr Gawlowicz
 *
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
 * Author: Piotr Gawlowicz <gawlowicz.p@gmail.com>
 * Based on script: ./examples/tcp/tcp-variants-comparison.cc
 * Modify: Pengyu Liu <eic_lpy@hust.edu.cn> 
 *         Hao Yin <haoyin@uw.edu>
 * Topology:
 *
 *   Left Leafs (Clients)                       Right Leafs (Sinks)
 *           |            \                    /        |
 *           |             \    bottleneck    /         |
 *           |              R0--------------R1          |
 *           |             /                  \         |
 *           |   access   /                    \ access |
 *  
 */

#include <iostream>
#include <fstream>
#include <string>

#include "ns3/tcp-westwood-plus.h"
#include "ns3/tcp-westwood.h"

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/point-to-point-layout-module.h"
#include "ns3/applications-module.h"
#include "ns3/error-model.h"
#include "ns3/tcp-header.h"
#include "ns3/enum.h"
#include "ns3/event-id.h"
#include "ns3/flow-monitor-helper.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/traffic-control-module.h"

#include "ns3/ns3-ai-module.h"
#include "tcp-rl.h"



//执行通信，连接python和c++构建的网络拓扑的桥梁

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("TcpRl");//定义一个日志组件，后面经常用到，但是没用开启

static std::vector<uint32_t> rxPkts; //用于 统计每个节点[sindId]接收到的pkts数量

//统计每个节点[sindId]接收到的pkts数量
static void
CountRxPkts (uint32_t sinkId, Ptr<const Packet> packet, const Address &srcAddr)
{
  rxPkts[sinkId]++;
}

//对每个sink接收到的数据包数量进行一个输出
static void
PrintRxCount ()
{
  uint32_t size = rxPkts.size ();
  NS_LOG_UNCOND ("RxPkts:");
  for (uint32_t i = 0; i < size; i++)
    {
      NS_LOG_UNCOND ("---SinkId: " << i << " RxPkts: " << rxPkts.at (i));
    }
}


//========================
int
main (int argc, char *argv[])
{

  // std::cout<<"success"<<std::endl;

  // LogComponentEnable ("Pool", LOG_LEVEL_ALL); // 没有找到日志组件pool的定义

  //定义变量
  uint32_t nLeaf = 1;
  std::string transport_prot = "ns3::TcpRlTimeBased";//定义为time-based RL_TCP
  double error_p = 0.0;
  std::string bottleneck_bandwidth = "2Mbps";
  std::string bottleneck_delay = "0.01ms";
  std::string access_bandwidth = "10Mbps";
  std::string access_delay = "20ms";
  std::string prefix_file_name = "TcpVariantsComparison";
  uint64_t data_mbytes = 0;
  uint32_t mtu_bytes = 400;
  double duration = 1000.0;//仿真时长
  uint32_t run = 0;
  bool flow_monitor = false;
  bool sack = true;
  std::string queue_disc_type = "ns3::PfifoFastQueueDisc";//队列调度器
  std::string recovery = "ns3::TcpClassicRecovery";

  CommandLine cmd;
  cmd.AddValue ("simSeed", "Seed for random generator. Default: 1", run);
  // other parameters
  cmd.AddValue ("nLeaf", "Number of left and right side leaf nodes", nLeaf);
  cmd.AddValue ("transport_prot",
                "Transport protocol to use: TcpNewReno, "
                "TcpHybla, TcpHighSpeed, TcpHtcp, TcpVegas, TcpScalable, TcpVeno, "
                "TcpBic, TcpYeah, TcpIllinois, TcpWestwood, TcpWestwoodPlus, TcpLedbat, "
                "TcpLp, TcpRl, TcpRlTimeBased",
                transport_prot);
  cmd.AddValue ("error_p", "Packet error rate", error_p);
  cmd.AddValue ("bottleneck_bandwidth", "Bottleneck bandwidth", bottleneck_bandwidth);
  cmd.AddValue ("bottleneck_delay", "Bottleneck delay", bottleneck_delay);
  cmd.AddValue ("access_bandwidth", "Access link bandwidth", access_bandwidth);
  cmd.AddValue ("access_delay", "Access link delay", access_delay);
  cmd.AddValue ("prefix_name", "Prefix of output trace file", prefix_file_name);
  cmd.AddValue ("data", "Number of Megabytes of data to transmit", data_mbytes);
  cmd.AddValue ("mtu", "Size of IP packets to send in bytes", mtu_bytes);
  cmd.AddValue ("duration", "Time to allow flows to run in seconds", duration);
  cmd.AddValue ("run", "Run index (for setting repeatable seeds)", run);
  cmd.AddValue ("flow_monitor", "Enable flow monitor", flow_monitor);
  cmd.AddValue ("queue_disc_type", "Queue disc type for gateway (e.g. ns3::CoDelQueueDisc)",
                queue_disc_type);
  cmd.AddValue ("sack", "Enable or disable SACK option", sack);
  cmd.AddValue ("recovery", "Recovery algorithm type to use (e.g., ns3::TcpPrrRecovery", recovery);
  cmd.Parse (argc, argv);

  SeedManager::SetSeed (1);
  SeedManager::SetRun (run);//设置随机数发生器，使多次模拟的随随机行为一样

  NS_LOG_UNCOND ("--seed: " << run); //使用了 NS_LOG_UNCOND 宏来无条件地打印消息，这意味着无论日志级别设置如何，这些消息都会被输出
  NS_LOG_UNCOND ("--Tcp version: " << transport_prot);

  //   // OpenGym Env --- has to be created before any other thing
  //   Ptr<OpenGymInterface> openGymInterface;
  //   if (transport_prot.compare ("ns3::TcpRl") == 0)
  //     {
  //       openGymInterface = OpenGymInterface::Get (openGymPort);
  //       Config::SetDefault ("ns3::TcpRl::Reward",
  //                           DoubleValue (2.0)); // Reward when increasing congestion window
  //       Config::SetDefault ("ns3::TcpRl::Penalty",
  //                           DoubleValue (-30.0)); // Penalty when decreasing congestion window
  //     }

  // if (transport_prot.compare ("ns3::TcpRlTimeBased") == 0)
  //   {
  //     Config::SetDefault ("ns3::TcpRlTimeBased::StepTime",
  //                         TimeValue (Seconds (0.01))); // Time step of TCP env
  //   }

  // Calculate the ADU size （Application Data Unit）
  Header *temp_header = new Ipv4Header ();
  uint32_t ip_header = temp_header->GetSerializedSize ();
  NS_LOG_LOGIC ("IP Header size is: " << ip_header);
  delete temp_header;
  temp_header = new TcpHeader ();
  uint32_t tcp_header = temp_header->GetSerializedSize ();
  NS_LOG_LOGIC ("TCP Header size is: " << tcp_header);
  delete temp_header;
  uint32_t tcp_adu_size = mtu_bytes - 20 - (ip_header + tcp_header); // 20可能是指额外的开销或者是一个预设值
  NS_LOG_LOGIC ("TCP ADU size is: " << tcp_adu_size);

  // Set the simulation start and stop time
  double start_time = 0.1;
  double stop_time = start_time + duration; //默认是1000s,可以命令行传参修改

  // 4 MB of TCP buffer 设置收发缓冲区
  Config::SetDefault ("ns3::TcpSocket::RcvBufSize", UintegerValue (1 << 21));
  Config::SetDefault ("ns3::TcpSocket::SndBufSize", UintegerValue (1 << 21));
  Config::SetDefault ("ns3::TcpSocketBase::Sack", BooleanValue (sack));//设置是否启用sack，默认是启用
  Config::SetDefault ("ns3::TcpSocket::DelAckCount", UintegerValue (2));//一旦接收到2个新的数据包且没有立即发送确认，则会发送一个ACK。这有助于提高性能，尤其是在网络延迟较高的环境中

  Config::SetDefault ("ns3::TcpL4Protocol::RecoveryType",
                      TypeIdValue (TypeId::LookupByName (recovery)));//"ns3::TcpClassicRecovery"
  // Select TCP variant
  if (transport_prot.compare ("ns3::TcpWestwoodPlus") == 0)//相同才返回0
    {
      // TcpWestwoodPlus is not an actual TypeId name; we need TcpWestwood here
      std::cout <<"test cout" << std::endl;
      Config::SetDefault ("ns3::TcpL4Protocol::SocketType",
                          TypeIdValue (TcpWestwoodPlus::GetTypeId ()));
      // the default protocol type in ns3::TcpWestwood is WESTWOOD
      //Config::SetDefault ("ns3::TcpWestwood::ProtocolType", EnumValue (TcpWestwoodPlus::TcpWestwoodPlus));
    }
  else
    {
      TypeId tcpTid;
      NS_ABORT_MSG_UNLESS (TypeId::LookupByNameFailSafe (transport_prot, &tcpTid),
                           "TypeId " << transport_prot << " not found"); //使用宏进行安全检查
      Config::SetDefault ("ns3::TcpL4Protocol::SocketType",
                          TypeIdValue (TypeId::LookupByName (transport_prot))); //设置TCP层协议的默认套接字类型为TcpRlTimeBased的TypeId值。
    }
    
    
    /*NS3仿真通信中的协议栈
    L4是应用层（TCP、UDP协议 端到端通信）
    L3是网络层（IP协议 路由选择和数据转发）
    L2是链路层（节点间的物理连接和逻辑连接（如MAC地址），以及错误检测）
    L1是物理层（涉及信号的发送和接收，电磁波的调制解调等）
    */

  // 配置一个基于数据包错误率的错误模型，引入丢包率
  // Configure the error model
  // Here we use RateErrorModel with packet error rate
  Ptr<UniformRandomVariable> uv = CreateObject<UniformRandomVariable> ();
  uv->SetStream (50);
  RateErrorModel error_model;
  error_model.SetRandomVariable (uv);
  error_model.SetUnit (RateErrorModel::ERROR_UNIT_PACKET);
  error_model.SetRate (error_p);

  //定义瓶颈链路
  // Create the point-to-point link helpers
  PointToPointHelper bottleNeckLink;
  bottleNeckLink.SetDeviceAttribute ("DataRate", StringValue (bottleneck_bandwidth));
  bottleNeckLink.SetChannelAttribute ("Delay", StringValue (bottleneck_delay));
  //bottleNeckLink.SetDeviceAttribute  ("ReceiveErrorModel", PointerValue (&error_model)); //模拟传输错误

  //定义接入链路
  PointToPointHelper pointToPointLeaf;
  pointToPointLeaf.SetDeviceAttribute ("DataRate", StringValue (access_bandwidth));
  pointToPointLeaf.SetChannelAttribute ("Delay", StringValue (access_delay));

  PointToPointDumbbellHelper d (nLeaf, pointToPointLeaf, nLeaf, pointToPointLeaf, bottleNeckLink);//创建dumbell拓扑

  // Install IP stack
  InternetStackHelper stack;
  stack.InstallAll ();

  // Traffic Control
  TrafficControlHelper tchPfifo;
  tchPfifo.SetRootQueueDisc ("ns3::PfifoFastQueueDisc");//基于优先级的fifo队列调度器，通常用于处理不同优先级的数据包

  TrafficControlHelper tchCoDel;
  tchCoDel.SetRootQueueDisc ("ns3::CoDelQueueDisc");//一种旨在减少缓冲区膨胀和提高网络响应时间的主动队列管理机制。

  //类型转换，把前面的string类型转换为ns3中相对应的类型
  DataRate access_b (access_bandwidth);
  DataRate bottle_b (bottleneck_bandwidth);
  Time access_d (access_delay);
  Time bottle_d (bottleneck_delay);

  //===========================================================

  uint32_t size = static_cast<uint32_t> ((std::min (access_b, bottle_b).GetBitRate () / 8) *
                                         ((access_d + bottle_d + access_d) * 2).GetSeconds ());//计算链路需要的队列容量

  Config::SetDefault ("ns3::PfifoFastQueueDisc::MaxSize",
                      QueueSizeValue (QueueSize (QueueSizeUnit::PACKETS, size / mtu_bytes)));//pfifofast队列以数据包数量为单位
  Config::SetDefault ("ns3::CoDelQueueDisc::MaxSize",
                      QueueSizeValue (QueueSize (QueueSizeUnit::BYTES, size)));//codel队列以字节为单位

  if (queue_disc_type.compare ("ns3::PfifoFastQueueDisc") == 0)
    {
      tchPfifo.Install (d.GetLeft ()->GetDevice (1));
      tchPfifo.Install (d.GetRight ()->GetDevice (1));
    }
  else if (queue_disc_type.compare ("ns3::CoDelQueueDisc") == 0)
    {
      tchCoDel.Install (d.GetLeft ()->GetDevice (1));
      tchCoDel.Install (d.GetRight ()->GetDevice (1));
    }
  else
    {
      NS_FATAL_ERROR ("Queue not recognized. Allowed values are ns3::CoDelQueueDisc or "
                      "ns3::PfifoFastQueueDisc");
    }

  // Assign IP Addresses
  d.AssignIpv4Addresses (Ipv4AddressHelper ("10.1.1.0", "255.255.255.0"),
                         Ipv4AddressHelper ("10.2.1.0", "255.255.255.0"),
                         Ipv4AddressHelper ("10.3.1.0", "255.255.255.0"));

  NS_LOG_INFO ("Initialize Global Routing.");
  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  // Install apps in left and right nodes
  uint16_t port = 50000;
  Address sinkLocalAddress (InetSocketAddress (Ipv4Address::GetAny (), port));
  PacketSinkHelper sinkHelper ("ns3::TcpSocketFactory", sinkLocalAddress);
  ApplicationContainer sinkApps;
  for (uint32_t i = 0; i < d.RightCount (); ++i)//对右侧的节点进行设置
    {
      sinkHelper.SetAttribute ("Protocol", TypeIdValue (TcpSocketFactory::GetTypeId ()));
      sinkApps.Add (sinkHelper.Install (d.GetRight (i)));
    }
  sinkApps.Start (Seconds (0.0));
  sinkApps.Stop (Seconds (stop_time));

  for (uint32_t i = 0; i < d.LeftCount (); ++i)//左侧节点
    {
      // Create an on/off app sending packets to the left side
      AddressValue remoteAddress (InetSocketAddress (d.GetRightIpv4Address (i), port));
      Config::SetDefault ("ns3::TcpSocket::SegmentSize", UintegerValue (tcp_adu_size));
      BulkSendHelper ftp ("ns3::TcpSocketFactory", Address ());
      ftp.SetAttribute ("Remote", remoteAddress);
      ftp.SetAttribute ("SendSize", UintegerValue (tcp_adu_size));
      ftp.SetAttribute ("MaxBytes", UintegerValue (data_mbytes * 1000000));

      ApplicationContainer clientApp = ftp.Install (d.GetLeft (i));
      clientApp.Start (Seconds (start_time * i)); // Start after sink
      clientApp.Stop (Seconds (stop_time - 3)); // Stop before the sink
    }

  // Flow monitor
  FlowMonitorHelper flowHelper;
  if (flow_monitor)
    {
      flowHelper.InstallAll ();
    }

  // Count RX packets
  for (uint32_t i = 0; i < d.RightCount (); ++i)
    {
      rxPkts.push_back (0);
      Ptr<PacketSink> pktSink = DynamicCast<PacketSink> (sinkApps.Get (i));
      pktSink->TraceConnectWithoutContext ("Rx", MakeBoundCallback (&CountRxPkts, i));
    }

  Simulator::Stop (Seconds (stop_time));
  Simulator::Run ();

  if (flow_monitor)
    {
      flowHelper.SerializeToXmlFile (prefix_file_name + ".flowmonitor", true, true);
    }

  //   if (transport_prot.compare ("ns3::TcpRl") == 0 or
  //       transport_prot.compare ("ns3::TcpRlTimeBased") == 0)
  //     {
  //       openGymInterface->NotifySimulationEnd ();
  //     }

  PrintRxCount ();
  Simulator::Destroy ();
  return 0;
}
