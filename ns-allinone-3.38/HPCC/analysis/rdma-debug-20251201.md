# 2025-12-01 RDMA 调试归档

## 思维导图
- 调试成果导图
  - 代码改动
    - `src/applications/model/seq-ts-header.{h,cc}`：序列时间戳头新增 16 位 PG 字段，更新序列化/反序列化逻辑并暴露 `SetPG`、`GetPG`
    - `src/point-to-point/model/rdma-hw.cc`：`ReceiveUdp` 扣除 INT 字节后再校验序列；`GetNxtPacket` 预留 INT 空间新增 SeqTsHeader；在 `Setup`、`AddQueuePair`、`ReceiveUdp`、`ReceiveAck`、`ReceiverCheckSeq`、`QpComplete` 增加日志
    - `src/point-to-point/model/qbb-net-device.cc`：主机接收路径加强日志和回调判空；交换机转发时附加 `FlowIdTag`
    - `mix/flow.txt`：缩减为两个回归流；`mix/config.txt`：`SIMULATOR_STOP_TIME` 恢复为 30.00
  - RDMA 数据流调用链
    - 流量调度：`scratch/third.cc` 读取 `mix/flow.txt`，`ReadFlowInput/ScheduleFlowInputs` 创建 `RdmaClient`
    - 客户端启动：`RdmaClient::StartApplication` 调用 `RdmaDriver::AddQueuePair`
    - 驱动初始化：`RdmaDriver::Init` 绑定 QBB 网卡并交给 `RdmaHw::Setup`
    - 队列对创建：`RdmaHw::AddQueuePair` 建立 QP、记录 `(dip,sport,pg)`、通知 `QbbNetDevice::NewQp`
    - 发送路径：`QbbNetDevice::DequeueAndTransmit` 取包，`RdmaHw::GetNxtPacket` 封装 SeqTs/UDP/IP/PPP，`TransmitStart` 送信道
    - 交换机转发：`QbbNetDevice::Receive` 给帧打 `FlowIdTag`，`SwitchNode::SwitchReceiveFromDevice` 写入 INT 信息后再次排队发送
    - 主机接收：目标主机 `QbbNetDevice::Receive` 调用 `RdmaHw::Receive`，`ReceiveUdp` 更新 `ReceiverCheckSeq`，按需生成 ACK/NACK
    - ACK 处理：源主机 `RdmaHw::ReceiveAck` 推进 `snd_una` 并在完成时触发 `QpComplete` 与 `qp_finish`
  - 后续行动
    - 复跑双流场景验证 PG 日志与 `mix/fct.txt`
    - 将日志与差异说明纳入项目文档

## 变更文件
- src/applications/model/seq-ts-header.cc
- src/applications/model/seq-ts-header.h
- src/point-to-point/model/rdma-hw.cc
- src/point-to-point/model/qbb-net-device.cc
- mix/flow.txt
- mix/config.txt




**触发路径总览**  
- rdma-driver.cc：在 `RdmaDriver::Init` 中，驱动通过 `m_rdma->Setup(...)` 把所有 NIC 的队列组共享给 RDMA 硬件，同时注册 `QpComplete` 回调 (`m_traceQpComplete`)。  
- rdma-hw.cc：`AddQueuePair()` 创建 `RdmaQueuePair`，用 `qp->SetSize(size)` 记录流量规模，并把队列压到 NIC 的队列组里。`snd_nxt`、`snd_una` 等完结判断所需的状态都保存在队列中。  
- `ReceiveUdp()`（约 240 行）更新接收端状态，通过 `ReceiverCheckSeq()` 判断何时生成 ACK/NACK；返回值为 `1` 时会拼装一个 `seq = ReceiverNextExpectedSeq` 的 ACK。  
- `ReceiveAck()`（约 420 行）唯一负责推进 `snd_una` (`qp->Acknowledge(seq)`)；当 `snd_una >= m_size` 时 `qp->IsFinished()` 成立，马上调用 `QpComplete(qp)`。  
- `QpComplete()`（约 500 行）先广播 trace (`m_qpCompleteCallback(qp)`)，再执行每个流的应用层回调 (`qp->m_notifyAppFinish()`)，最终由 `RdmaClient` 收尾。

**运行时自查方法**  
- 在 ACK 路径加日志（`NS_LOG=RdmaHw` 并在 `ReceiveAck` 打 `NS_LOG_INFO`，打印 `seq`、`snd_una`、`qp->m_size`）。确认 ACK 是否到达、`snd_una` 是否追上 `m_size`。  
- 增加额外 trace：除了绑定 `"QpComplete"` 外，也可在 `"RdmaQpDequeue"` 或 `RdmaHw::ReceiveAck` 中输出统计信息。  
- 跑完仿真后查看计数：若 `snd_nxt` 已走到流大小但 `snd_una` 停滞，说明没有收到 ACK；优先排查接收端逻辑 (`ReceiverCheckSeq`、`m_ack_interval`、`m_chunk`)。  
- 复查配置：config.txt 中 `L2_ACK_INTERVAL` 为 `1`，理论上每处理一个 chunk 就会发 ACK。如果设成 `0`，完结要依赖 chunk 门限，需确认该组合是否合理。

**值得细查的代码点**  
- `RdmaQueuePair::IsFinished()`（rdma-queue-pair.cc，约 104 行）仅比较 `snd_una` 和 `m_size`；若应用层给的 `m_size` 与真实负载不符，完结不会触发。  
- `RdmaHw::ReceiveUdp()` 通过 `p->GetSize() - ch.GetSerializedSize()` 得到负载长度。如果 INT 头或 padding 有改动，确认该值仍与 `GetNxtPacket()` 发送一致；否则接收侧会少算字节，ACK 永远追不上。  
- `RdmaDriver::GetTypeId()` 把 trace 类型声明为了 `"ns3::Packet::TracedCallback"`。虽然实际签名 `TracedCallback<Ptr<RdmaQueuePair>>` 能用，但若依赖反射工具，建议把字符串更新得更贴近真实类型。

**后续建议**  
1. 在 `ReceiveAck()` 加临时日志，输出 `(seq, snd_una, m_size)`，重跑观察是否出现 `seq == m_size`。  
2. 若 ACK 永远追不上 `m_size`，再在 `ReceiverCheckSeq()` 打印收方统计（特别是启用 INT 时的 `IntHeader::pint_bytes`），确认字节计数是否一致。