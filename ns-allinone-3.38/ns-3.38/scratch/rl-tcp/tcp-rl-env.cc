/* -*-  Mode: C++; c-file-style: "gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2018 Technische Universität Berlin
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
 * Author: Piotr Gawlowicz <gawlowicz@tkn.tu-berlin.de>
 * Modify: Pengyu Liu <eic_lpy@hust.edu.cn> 
 *         Hao Yin <haoyin@uw.edu>
 */

#include <numeric>
#include "tcp-rl-env.h"
namespace ns3
{
NS_LOG_COMPONENT_DEFINE("ns3::TcpRlEnv");
TcpRlEnv::TcpRlEnv(uint16_t id) : Ns3AIRL<sTcpRlEnv, TcpRlAct>(id)//构造函数：基类
{
  SetCond(2, 0);
}
void TcpRlEnv::SetNodeId(uint32_t id)
{
  NS_LOG_FUNCTION(this);
  m_nodeId = id;
}

void TcpRlEnv::SetSocketUuid(uint32_t id)
{
  NS_LOG_FUNCTION(this);
  m_socketUuid = id;
}

void TcpRlEnv::TxPktTrace(Ptr<const Packet>, const TcpHeader &, Ptr<const TcpSocketBase>)
{//是一个用于追踪TCP数据包发送事件的回调函数
  //   NS_LOG_FUNCTION (this);
  if (m_lastPktTxTime > MicroSeconds(0.0))//检查上一次的发送时间
  {
    Time interTxTime = Simulator::Now() - m_lastPktTxTime;//本次发送与上次发送的时间差
    m_interTxTimeSum += interTxTime;//发送时间累加
    m_interTxTimeNum++;//发送次数累加
  }

  m_lastPktTxTime = Simulator::Now();//更新last发送时间
}

void TcpRlEnv::RxPktTrace(Ptr<const Packet>, const TcpHeader &, Ptr<const TcpSocketBase>)
{//类似上面的操作，但是接收
  //   NS_LOG_FUNCTION (this);
  if (m_lastPktRxTime > MicroSeconds(0.0))
  {
    Time interRxTime = Simulator::Now() - m_lastPktRxTime;
    m_interRxTimeSum += interRxTime;
    m_interRxTimeNum++;
  }

  m_lastPktRxTime = Simulator::Now();
}


TcpTimeStepEnv::TcpTimeStepEnv(uint16_t id) : TcpRlEnv(id)
{
}

void TcpTimeStepEnv::ScheduleNextStateRead()
{
  //   NS_LOG_FUNCTION (this);
  // std::cerr << m_timeStep.GetMilliSeconds() << std::endl;
  Simulator::Schedule(m_timeStep, &TcpTimeStepEnv::ScheduleNextStateRead, this);//调度下一次的读取
  //   Notify();
  // while (GetVersion() % 2 != 0)
  //   ;


  //把action应用到ns3仿真中
  auto env = EnvSetterCond();
  env->socketUid = m_socketUuid;
  env->envType = 1;
  env->simTime_us = Simulator::Now().GetMicroSeconds();
  env->nodeId = m_nodeId;
  env->ssThresh = m_tcb->m_ssThresh;
  env->cWnd = m_tcb->m_cWnd;
  env->segmentSize = m_tcb->m_segmentSize;

  uint64_t bytesInFlightSum = std::accumulate(m_bytesInFlight.begin(), m_bytesInFlight.end(), 0);
  env->bytesInFlight = bytesInFlightSum;
  m_bytesInFlight.clear();

  uint64_t segmentsAckedSum = std::accumulate(m_segmentsAcked.begin(), m_segmentsAcked.end(), 0);
  env->segmentsAcked = segmentsAckedSum;
  m_segmentsAcked.clear();
  std::cerr << "At " << (uint64_t)(Simulator::Now().GetMilliSeconds()) << "ms:\n";
  
  //输出系统时间
  printCurrentTimeWithMillis();

  std::cerr << "\tstate --"
            << " ssThresh=" << env->ssThresh
            << " cWnd=" << env->cWnd
            << " segmentSize=" << env->segmentSize
            << " segmentAcked=" << env->segmentsAcked
            << " bytesInFlightSum=" << bytesInFlightSum
            << std::endl;
  SetCompleted();
  // std::cerr<<"GetVersion () "<<(int)GetVersion ()<<std::endl;
  // while (GetVersion() % 2 != 0)
  //   ;
  auto act = ActionGetterCond();//把决策应用到通信中
  m_new_cWnd = act->new_cWnd;
  m_new_ssThresh = act->new_ssThresh;
  GetCompleted();
  m_rttSampleNum = 0;
  m_rttSum = MicroSeconds(0.0);

  m_interTxTimeNum = 0;
  m_interTxTimeSum = MicroSeconds(0.0);

  m_interRxTimeNum = 0;
  m_interRxTimeSum = MicroSeconds(0.0);
  //   Time avgRtt = Seconds (0.0);
  //   if (m_rttSampleNum)
  //     avgRtt = m_rttSum / m_rttSampleNum;
  //   env->rtt = avgRtt.GetMicroSeconds ();

  //   env->minRtt = m_tcb->m_minRtt.GetMicroSeconds ();
  //   env->calledFunc = m_calledFunc;
  //   env->congState = m_tcb->m_congState;
  //   env->event = m_event;
  //   env->ecnState = m_tcb->m_ecnState;
  std::cerr << "\taction --"
            << " new_cWnd=" << m_new_cWnd
            << " new_ssThresh=" << m_new_ssThresh
            << std::endl;
}

uint32_t
TcpTimeStepEnv::GetSsThresh(Ptr<const TcpSocketState> tcb, uint32_t bytesInFlight)
{
  NS_LOG_FUNCTION(this);
  NS_LOG_INFO(Simulator::Now() << " Node: " << m_nodeId
                               << " GetSsThresh, BytesInFlight: " << bytesInFlight);
  m_tcb = tcb;
  m_bytesInFlight.push_back(bytesInFlight);

  if (!m_started)
  {
    m_started = true;
    // Notify();
    ScheduleNextStateRead();
  }

  // action
  return m_new_ssThresh;
}

void TcpTimeStepEnv::IncreaseWindow(Ptr<TcpSocketState> tcb, uint32_t segmentsAcked)
{
  NS_LOG_FUNCTION(this);
  NS_LOG_INFO(Simulator::Now() << " Node: " << m_nodeId
                               << " IncreaseWindow, SegmentsAcked: " << segmentsAcked);
  m_tcb = tcb;
  m_segmentsAcked.push_back(segmentsAcked);
  m_bytesInFlight.push_back(tcb->m_bytesInFlight);

  if (!m_started)
  {
    m_started = true;
    // Notify();
    ScheduleNextStateRead();
  }
  // action
  tcb->m_cWnd = m_new_cWnd;
}

void TcpTimeStepEnv::PktsAcked(Ptr<TcpSocketState> tcb, uint32_t segmentsAcked, const Time &rtt)
{
  //   NS_LOG_FUNCTION (this);
  //   NS_LOG_INFO(Simulator::Now() << " Node: " << m_nodeId << " PktsAcked, SegmentsAcked: " << segmentsAcked << " Rtt: " << rtt);
  m_tcb = tcb;
  m_rttSum += rtt;
  m_rttSampleNum++;
}

void TcpTimeStepEnv::CongestionStateSet(Ptr<TcpSocketState> tcb,
                                        const TcpSocketState::TcpCongState_t newState)
{
  //   NS_LOG_FUNCTION (this);
  //   std::string stateName = GetTcpCongStateName(newState);
  //   NS_LOG_INFO(Simulator::Now() << " Node: " << m_nodeId << " CongestionStateSet: " << newState << " " << stateName);
  m_tcb = tcb;
}

void TcpTimeStepEnv::CwndEvent(Ptr<TcpSocketState> tcb, const TcpSocketState::TcpCAEvent_t event)
{
  //   NS_LOG_FUNCTION (this);
  //   std::string eventName = GetTcpCAEventName(event);
  //   NS_LOG_INFO(Simulator::Now() << " Node: " << m_nodeId << " CwndEvent: " << event << " " << eventName);
  m_tcb = tcb;
}


//获取系统时间
void printCurrentTimeWithMillis() {
  // 获取当前时间点
  auto now = std::chrono::system_clock::now();
  
  // 转换为time_t以获取秒级时间信息
  auto now_time_t = std::chrono::system_clock::to_time_t(now);
  
  // 将秒级时间转换回chrono时间点以便与毫秒结合
  auto now_time = std::chrono::system_clock::from_time_t(now_time_t);
  
  // 计算毫秒数
  auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(now - now_time).count() % 1000;

  // 提取小时、分钟和秒
  std::tm local_tm = *std::localtime(&now_time_t);

  // 输出时分秒和毫秒
  std::cerr << std::put_time(&local_tm, "%H:%M:%S")
            << '.' << std::setfill('0') << std::setw(3) << millis ;
}
} // namespace ns3