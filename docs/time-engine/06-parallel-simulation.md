# 06. 并发仿真

## 1. 为什么需要并发

芯片性能仿真系统会有很多组件：

```text
几十到上百个 Core
多级 Cache
NoC routers
Memory controllers
DMA / IO devices
各种 queue / buffer / port
```

如果所有事件都由一个线程执行，仿真速度会受限。

并发仿真的目标是：

```text
把可以独立推进的组件分给多个线程
减少单线程事件队列压力
利用多核 CPU 提升仿真速度
```

但并发仿真必须保留一个底线：

```text
同样输入下，多线程结果必须和单线程参考模型一致。
```

## 2. 不推荐的方案：多个线程抢全局 EventQueue

直觉方案：

```text
Global EventQueue
  +-- Thread 0 pop
  +-- Thread 1 pop
  +-- Thread 2 pop
```

这个方案问题很大：

```text
EventQueue 锁竞争会很重
同一时间点的事件顺序更难稳定
callback 可能修改共享组件状态
很难判断两个事件是否真的可以并发执行
```

所以不推荐把“单线程 EventQueue”直接加锁变成并发版本。

## 3. 推荐方案：分区并发

推荐把系统拆成多个 Partition。

```text
ParallelTimeEngine
  +-- Partition 0
  |     +-- local EventQueue
  |     +-- Core 0
  |     +-- L1 Cache 0
  |
  +-- Partition 1
  |     +-- local EventQueue
  |     +-- Core 1
  |     +-- L1 Cache 1
  |
  +-- Shared Partition
        +-- L2 Cache
        +-- NoC
        +-- Memory Controller
```

每个 Partition：

```text
拥有一组 Component
拥有自己的 local EventQueue
通常绑定一个 worker thread
只直接修改自己拥有的 Component 状态
```

这样可以把共享状态边界变清楚。

## 4. Local Event 和 Remote Event

事件分两类。

```text
Local Event
  +-- 目标 Component 和当前 Component 在同一个 Partition
  +-- 直接进入本地 local EventQueue

Remote Event
  +-- 目标 Component 在另一个 Partition
  +-- 不能直接修改对方状态
  +-- 通过 Mailbox 发送给目标 Partition
```

例子：

```text
Core 0 -> L1 0
  +-- local event

L1 0 -> Shared L2
  +-- remote event

Shared L2 -> Core 0
  +-- remote event
```

关键原则：

```text
一个 Component 的状态只由它所属的 Partition 修改。
```

这比到处加锁更容易维护。

## 5. Mailbox

`Mailbox` 是跨 Partition 事件传递通道。

```text
Mailbox
  +-- sourcePartition
  +-- targetPartition
  +-- remote events
```

Remote Event 至少需要：

```text
targetPartitionId
targetTime
phase
callback 或 message
sourcePartitionId
sourceSequence
```

更推荐长期使用 message，而不是直接跨线程传 callback。

原因：

```text
message 更容易序列化
message 更容易 trace
message 更容易做 deterministic replay
message 更容易跨进程或分布式扩展
```

第一版可以先用 callback 简化实现，但文档和接口要保留演进空间。

## 6. 时间正确性问题

并发仿真的难点是：

```text
Partition A 可能已经执行到 T=2000
Partition B 之后却发来一个 T=1500 的事件
```

这就是“向过去发送事件”，会破坏时间正确性。

因此并发仿真必须有同步策略。

## 7. 保守方案：Epoch Barrier

第一版推荐保守同步。

```text
Global Epoch
  +-- 所有 Partition 执行 [epochStart, epochEnd) 内的事件
  +-- 到达 epochEnd 后等待 barrier
  +-- 合并 Mailbox 中的 Remote Event
  +-- 进入下一个 epoch
```

例子：

```text
epoch size = 1000 ps

Thread 0:
  Partition 0 执行 [0, 1000)

Thread 1:
  Partition 1 执行 [0, 1000)

Thread 2:
  Shared Partition 执行 [0, 1000)

barrier:
  +-- 所有线程停止在 epoch 边界
  +-- 合并 remote events
  +-- 进入 [1000, 2000)
```

优点：

```text
实现简单
容易调试
容易保证确定性
适合作为第一版并发模型
```

代价：

```text
epoch 太小，barrier 开销大
epoch 太大，跨 Partition 事件延迟合并
负载不均衡时，快线程等待慢线程
```

## 8. Lookahead

`lookahead` 表示一个 Partition 能保证不会向过去发送事件的最小时间距离。

```text
lookahead = min remote event latency
```

如果所有跨 Partition 通信都有最小延迟：

```text
RemoteEvent.targetTime >= senderNow + lookahead
```

那么 Partition 可以更安全地向前推进。

例子：

```text
Core Partition -> NoC Partition 至少 2 cycles
NoC Partition -> L2 Partition 至少 4 cycles
L2 Partition -> Memory Partition 至少 20 cycles
```

lookahead 越大，并发空间越大。

## 9. 确定性合并

并发后仍要保证顺序稳定。

规则：

```text
每个 Partition 内部:
  +-- time / phase / priority / sequence

Mailbox 内部:
  +-- sourcePartitionId / sourceSequence

Barrier 合并:
  +-- targetTime / phase / priority / deterministicMergeKey
```

事件可以扩展：

```text
Event
  +-- sourcePartitionId
  +-- targetPartitionId
  +-- localSequence
  +-- deterministicMergeKey
```

不要依赖线程调度顺序，因为 OS 调度顺序不可控。

## 10. 并发落地路线

不要直接跳到多线程。

推荐：

```text
Step 1: 单线程 TimeEngine
  +-- 建立正确语义和测试

Step 2: 单线程多 Partition
  +-- 仍然一个线程执行
  +-- 但事件已经区分 local / remote
  +-- 验证分区模型

Step 3: 多线程 + Epoch Barrier
  +-- 每个 Partition 一个 local EventQueue
  +-- worker thread 执行自己的 epoch

Step 4: 性能调优
  +-- 调整 Partition 划分
  +-- 调整 epoch size
  +-- 批量传递 Mailbox
  +-- 减少 Remote Event 数量
```

暂时不推荐第一版做 optimistic simulation：

```text
需要 rollback
需要状态 checkpoint
实现复杂
调试困难
```
