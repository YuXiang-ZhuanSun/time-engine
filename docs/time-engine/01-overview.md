# 01. 总体方案

## 1. Time Engine 到底是什么

Time Engine 是一个离散事件仿真内核。

在芯片性能仿真里，我们关心的不是每根信号线的电平变化，而是系统级行为：

```text
一条指令什么时候发射？
一次 cache hit 需要几个 cycle？
一次 cache miss 什么时候访问 L2？
多个请求争用同一个端口时谁先执行？
DRAM 返回后什么时候唤醒 pipeline？
```

这些问题都可以归结成：

```text
在仿真时间线上，哪些事件会发生？
它们按什么顺序发生？
发生之后会触发哪些新事件？
```

Time Engine 负责管理这条时间线。

## 2. 它要做到什么

第一层能力：单线程正确性。

```text
统一仿真时间 SimTime
按时间顺序执行 Event
支持 scheduleAt / scheduleAfter
支持取消 Event
支持同一时间点内的稳定排序
支持 ClockDomain，把 cycle 转成时间
```

第二层能力：芯片建模友好。

```text
Component 可以表达延迟
Component 可以表达 stall / wakeup
Resource 可以表达端口占用和排队
Trace / Stats 可以记录事件和性能数据
```

第三层能力：并发仿真。

```text
系统有很多组件时，可以拆成多个 Partition
每个 Partition 有自己的 EventQueue
不同 Partition 可以由不同线程执行
跨 Partition 事件通过 Mailbox 传递
用 Epoch Barrier 或 Lookahead 保证时间正确性
```

## 3. 它不做什么

Time Engine 不应该变成一个“什么都管”的大对象。

它不负责：

```text
Cache 替换策略
Cache coherence 协议细节
NoC routing 算法
DRAM timing policy
Pipeline issue policy
Branch predictor 行为
```

这些属于组件和资源模型。

Time Engine 只提供时间能力，硬件策略由上层模型使用这些能力表达。

## 4. 总体结构

```text
TimeEngine
  +-- SimTime
  |     +-- 全局仿真时间
  |
  +-- Event
  |     +-- 一个未来要发生的动作
  |
  +-- EventQueue
  |     +-- 按时间顺序保存 Event
  |
  +-- Scheduler
  |     +-- Component 用它安排未来动作
  |
  +-- ClockDomain
  |     +-- 把 cycle 延迟转换成 SimTime
  |
  +-- Trace / Stats Hook
        +-- 记录事件和性能数据
```

上层模型：

```text
Component
  +-- 使用 Scheduler
  +-- 不直接操作 EventQueue

TimedResource
  +-- 使用 Scheduler
  +-- 表达资源占用、排队、仲裁

Parallel Runtime
  +-- 管理多个 Partition
  +-- 管理跨线程事件传递
```

## 5. 一个完整例子

假设 Core 发起一次 load：

```text
T=1000 ps
Core 发起 load
  -> 调用 L1 Cache

T=1000 ps
L1 Cache 接收请求
  -> 发现 miss
  -> scheduleAfter(4 cycles, 发送到 L2)

T=5000 ps
L1 Cache 向 L2 发请求
  -> L2 进入队列
  -> L2 port 资源仲裁

T=9000 ps
L2 处理完成
  -> scheduleAfter(memoryLatency, 返回数据)

T=50000 ps
数据返回 Core
  -> Core wakeup dependent instruction
```

Time Engine 本身并不知道什么是 cache miss。它只知道：

```text
某个组件在 T=1000 创建了一个 T=5000 的 Event
某个组件在 T=5000 创建了一个 T=9000 的 Event
某个组件在 T=9000 创建了一个 T=50000 的 Event
```

这就是 Time Engine 和硬件模型的边界。

## 6. 为什么用事件驱动

一个简单办法是每个 cycle 扫描所有组件：

```text
for each cycle:
  for each component:
    component.tick()
```

这个办法容易理解，但当系统很大、很多组件空闲时，会浪费大量时间。

事件驱动的方式是：

```text
只有未来真的有动作时，才创建 Event
仿真时间可以直接跳到下一个 Event
```

所以它更适合：

```text
大规模系统
长延迟事件
大量空闲组件
性能仿真而不是信号级仿真
```

但有些模块仍然适合周期推进，比如 pipeline。这个框架允许混合：

```text
Cache / NoC / DRAM
  -> event-driven

Pipeline / Issue Queue / Scoreboard
  -> tick-driven on top of Event
```
