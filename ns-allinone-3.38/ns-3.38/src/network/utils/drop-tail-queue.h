/*
 * Copyright (c) 2007 University of Washington
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

#ifndef DROPTAIL_H
#define DROPTAIL_H

#include "ns3/queue.h"
#include <queue>
#include "ns3/packet.h"//更新

namespace ns3
{


/**
 * \ingroup queue
 *
 * \brief A FIFO packet queue that drops tail-end packets on overflow
 */
template <typename Item>
class DropTailQueue : public Queue<Item>
{
  public:
    /**
     * \brief Get the type ID.
     * \return the object TypeId
     */
    static TypeId GetTypeId();
    /**
     * \brief DropTailQueue Constructor
     *
     * Creates a droptail queue with a maximum size of 100 packets by default
     */
    DropTailQueue();

    ~DropTailQueue() override;

    bool Enqueue(Ptr<Item> item) override;
    Ptr<Item> Dequeue() override;
    Ptr<Item> Remove() override;
    Ptr<const Item> Peek() const override;

    /** 更新
     * Set the operating mode of this device.
     *
     * \param mode The operating mode of this device.
     *
     */
    void SetMode (typename Queue<Item>::QueueMode mode);

    /** 更新
     * Get the encapsulation mode of this device.
     *
     * \returns The encapsulation mode of this device.
     */
    typename Queue<Item>::QueueMode GetMode (void);


  private:
    using Queue<Item>::GetContainer;
    using Queue<Item>::DoEnqueue;
    bool DoEnqueue(Ptr<Item> p);//新增的重载
    using Queue<Item>::DoDequeue;
    Ptr<Item> DoDequeue(void);//新增的重载

    using Queue<Item>::DoRemove;
    using Queue<Item>::DoPeek;

    std::queue<Ptr<Item> > m_packets;///更新
    // Queue<Item> m_packets; 会出错
    uint32_t m_maxPackets;
    uint32_t m_maxBytes;
    uint32_t m_bytesInQueue;
    typename Queue<Item>::QueueMode m_mode;

    //void Drop(Ptr<Item> p); // 新增声明

    NS_LOG_TEMPLATE_DECLARE; //!< redefinition of the log component
};

/**
 * Implementation of the templates declared above.
 */

template <typename Item>
TypeId
DropTailQueue<Item>::GetTypeId()
{
    static TypeId tid =
        TypeId(GetTemplateClassName<DropTailQueue<Item>>())
            .SetParent<Queue<Item>>()
            .SetGroupName("Network")
            .template AddConstructor<DropTailQueue<Item>>()
            .AddAttribute("MaxSize",
                          "The max queue size",
                          QueueSizeValue(QueueSize("100p")),
                          MakeQueueSizeAccessor(&QueueBase::SetMaxSize, &QueueBase::GetMaxSize),
                          MakeQueueSizeChecker())
            //rhy modify
            .AddAttribute ("Mode", 
                            "Whether to use bytes (see MaxBytes) or packets (see MaxPackets) as the maximum queue size metric.",
                            EnumValue (Queue<Item>::QueueMode::QUEUE_MODE_BYTES),
                            MakeEnumAccessor (&DropTailQueue::SetMode),
                            MakeEnumChecker (Queue<Item>::QueueMode::QUEUE_MODE_BYTES, "QUEUE_MODE_BYTES",
                                            Queue<Item>::QueueMode::QUEUE_MODE_PACKETS, "QUEUE_MODE_PACKETS"))
            .AddAttribute ("MaxPackets", 
                            "The maximum number of packets accepted by this DropTailQueue.",
                            UintegerValue (100),
                            MakeUintegerAccessor (&DropTailQueue::m_maxPackets),
                            MakeUintegerChecker<uint32_t> ())
            .AddAttribute ("MaxBytes", 
                            "The maximum number of bytes accepted by this DropTailQueue.",
                            UintegerValue (30000 * 65535),
                            MakeUintegerAccessor (&DropTailQueue::m_maxBytes),
                            MakeUintegerChecker<uint32_t> ());
    return tid;
}

template <typename Item>
DropTailQueue<Item>::DropTailQueue()
    : Queue<Item>(),
      m_packets(),
      m_bytesInQueue(0),
      NS_LOG_TEMPLATE_DEFINE("DropTailQueue")
{
    NS_LOG_FUNCTION(this);
}

template <typename Item>
DropTailQueue<Item>::~DropTailQueue()
{
    NS_LOG_FUNCTION(this);
}

template <typename Item>//更新
void
DropTailQueue<Item>::SetMode(typename Queue<Item>::QueueMode mode)
{
    NS_LOG_FUNCTION (mode);
    m_mode = mode;
}

template <typename Item>//更新
typename Queue<Item>::QueueMode
DropTailQueue<Item>::GetMode(void)
{
    NS_LOG_FUNCTION(this);
    return m_mode;
}

template <typename Item>
bool
DropTailQueue<Item>::DoEnqueue(Ptr<Item> p){//更新
  NS_LOG_FUNCTION (this << p);

  if (m_mode == Queue<Item>::QueueMode::QUEUE_MODE_PACKETS && (m_packets.size () >= m_maxPackets))
    {
      NS_LOG_LOGIC ("Queue full (at max packets) -- droppping pkt");
      //Drop (p);
      this->Drop(p); // 调用成员函数 Drop
      return false;
    }

  if (m_mode == Queue<Item>::QueueMode::QUEUE_MODE_BYTES && (m_bytesInQueue + p->GetSize () >= m_maxBytes))
    {
      NS_LOG_LOGIC ("Queue full (packet would exceed max bytes) -- droppping pkt");
      //Drop (p);
      this->Drop(p); // 调用成员函数 Drop
      return false;
    }

  m_bytesInQueue += p->GetSize ();
  m_packets.push (p);

  NS_LOG_LOGIC ("Number packets " << m_packets.size ());
  NS_LOG_LOGIC ("Number bytes " << m_bytesInQueue);

  //std::cout << "Buffer enqueue:" << p->GetSize() << "\n";

  return true;
}

template <typename Item>//更新
Ptr<Item>
DropTailQueue<Item>::DoDequeue(void)
{
  NS_LOG_FUNCTION (this);
  
  if (m_packets.empty ())
    {
      NS_LOG_LOGIC ("Queue empty");
      return 0;
    }
    


  Ptr<Item> p = m_packets.front ();
  m_packets.pop ();
  m_bytesInQueue -= p->GetSize ();

  NS_LOG_LOGIC ("Popped " << p);

  NS_LOG_LOGIC ("Number packets " << m_packets.size ());
  NS_LOG_LOGIC ("Number bytes " << m_bytesInQueue);

  return p;
}

template <typename Item>
bool
DropTailQueue<Item>::Enqueue(Ptr<Item> item)
{
    NS_LOG_FUNCTION(this << item);

    return DoEnqueue(GetContainer().end(), item);
}

template <typename Item>
Ptr<Item>
DropTailQueue<Item>::Dequeue()
{
    NS_LOG_FUNCTION(this);

    Ptr<Item> item = DoDequeue(GetContainer().begin());

    NS_LOG_LOGIC("Popped " << item);

    return item;
}

template <typename Item>
Ptr<Item>
DropTailQueue<Item>::Remove()
{
    NS_LOG_FUNCTION(this);

    Ptr<Item> item = DoRemove(GetContainer().begin());

    NS_LOG_LOGIC("Removed " << item);

    return item;
}

template <typename Item>
Ptr<const Item>
DropTailQueue<Item>::Peek() const
{
    NS_LOG_FUNCTION(this);

    return DoPeek(GetContainer().begin());
}

// The following explicit template instantiation declarations prevent all the
// translation units including this header file to implicitly instantiate the
// DropTailQueue<Packet> class and the DropTailQueue<QueueDiscItem> class. The
// unique instances of these classes are explicitly created through the macros
// NS_OBJECT_TEMPLATE_CLASS_DEFINE (DropTailQueue,Packet) and
// NS_OBJECT_TEMPLATE_CLASS_DEFINE (DropTailQueue,QueueDiscItem), which are included
// in drop-tail-queue.cc
extern template class DropTailQueue<Packet>;
extern template class DropTailQueue<QueueDiscItem>;

} // namespace ns3

#endif /* DROPTAIL_H */
