/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
* Copyright (c) 2006 Georgia Tech Research Corporation, INRIA
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
*/

/*
这实现了 Broadcom ASIC 交换机模型，该模型主要执行各种与缓冲区阈值相关的操作。 
其中包括决定是否应触发 PFC、是否应标记 ECN、缓冲区太满而应丢弃数据包等。它支持 PFC 的静态和动态阈值。*/

#ifndef BROADCOM_EGRESS_H
#define BROADCOM_EGRESS_H

#include <queue>
#include "ns3/packet.h"
#include "queue.h"
#include "drop-tail-queue.h"
#include "ns3/point-to-point-net-device.h"
#include "ns3/event-id.h"

namespace ns3 {

	class TraceContainer;

	//这是一个模板化的队列类，专门用于处理网络包（Packet）。
	class BEgressQueue : public Queue<Packet> {
	public:
		static TypeId GetTypeId(void);
		static const unsigned fCnt = 128; //max number of queues, 128 for NICs // 最大队列数量，针对网卡（NICs）
		static const unsigned qCnt = 8; //max number of queues, 8 for switches // 最大队列数量，针对交换机
		BEgressQueue();
		virtual ~BEgressQueue();
		bool Enqueue(Ptr<Packet> p, uint32_t qIndex);//将一个数据包入队到指定优先级的队列中
		Ptr<Packet> DequeueRR(bool paused[]);//使用轮询调度（Round Robin）从队列中取出数据包
		uint32_t GetNBytes(uint32_t qIndex) const;//获取指定队列中的字节数
		uint32_t GetNBytesTotal() const;//获取所有队列中的总字节数
		uint32_t GetLastQueue();//返回上一次操作的队列索引

		TracedCallback<Ptr<const Packet>, uint32_t> m_traceBeqEnqueue;// 入队事件追踪回调
		TracedCallback<Ptr<const Packet>, uint32_t> m_traceBeqDequeue;// 出队事件追踪回调

		//补全纯虚函数的实现
		bool Enqueue(Ptr<Packet> item) override;
		Ptr<Packet> Dequeue() override;
		Ptr<Packet> Remove() override;
		Ptr<const Packet> Peek() const override;

	private://
		bool DoEnqueue(Ptr<Packet> p, uint32_t qIndex);//实际执行入队操作
		Ptr<Packet> DoDequeueRR(bool paused[]);//实际执行轮询调度出队操作。
		//for compatibility
		virtual bool DoEnqueue(Ptr<Packet> p);//默认入队（兼容性方法）。
		virtual Ptr<Packet> DoDequeue(void);// 默认出队（未实现）
		virtual Ptr<const Packet> DoPeek(void) const;//查看队列头部的数据包
		double m_maxBytes; //total bytes limit 
		uint32_t m_bytesInQueue[fCnt];// 队列允许的最大字节数
		uint32_t m_bytesInQueueTotal;// 每个队列中的字节数
		uint32_t m_rrlast; // 上一次轮询调度的队列索引
		uint32_t m_qlast;// 上一次操作的队列索引
		std::vector<Ptr<Queue<Packet>> > m_queues; // uc queues // 存储多个子队列的容器
	};

} // namespace ns3

#endif /* DROPTAIL_H */


/*
2. 功能实现
(1) 多优先级队列管理
BEgressQueue 实现了一个支持多优先级的队列系统：
使用 std::vector<Ptr<Queue<Packet>>> m_queues 来存储多个子队列。
每个子队列是一个 DropTailQueue（尾部丢弃队列）。
支持最多 fCnt（128）个队列，通常用于网卡；支持最多 qCnt（8）个队列，通常用于交换机。
(2) 轮询调度（Round Robin）
DoDequeueRR(bool paused[]) 方法实现了轮询调度算法：
优先从高优先级队列（索引为 0）中取数据包。
如果高优先级队列为空，则按照轮询顺序从其他队列中取数据包。
参数 paused[] 表示哪些队列当前不可用（暂停）。
(3) 数据包追踪
m_traceBeqEnqueue 和 m_traceBeqDequeue 是两个追踪回调函数：
在数据包入队和出队时触发回调。
通过 AddTraceSource 注册到 ns-3 的日志系统中，便于调试和监控。
(4) 字节限制
m_maxBytes 定义了队列允许的最大字节数。
在 DoEnqueue 中检查当前队列总字节数是否超过限制：
如果超出限制，返回 false，表示入队失败。
否则，将数据包加入队列，并更新相关计数器。
(5) 兼容性方法
提供了默认的 DoEnqueue 和 DoDequeue 方法，以兼容旧版本的 Queue 接口。
默认方法仅操作第一个队列（索引为 0），并在使用时发出警告。

3. 主要作用
(1) 网络设备中的多队列管理
BEgressQueue 是一个专为网络设备设计的多队列管理器。
它可以用于模拟交换机或网卡的出队逻辑，支持多优先级和轮询调度。
(2) 性能优化
通过多队列和轮询调度，BEgressQueue 可以提高网络设备的吞吐量和公平性。
例如，在交换机中，高优先级流量可以优先处理，而低优先级流量则按轮询顺序处理。
(3) 调试与监控
通过 TracedCallback 和 AddTraceSource，可以方便地追踪数据包的入队和出队事件。
这对于分析网络行为和调试问题非常有用。
*/
