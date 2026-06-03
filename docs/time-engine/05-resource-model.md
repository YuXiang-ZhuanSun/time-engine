# 05. 资源模型

## 1. 为什么资源竞争不能塞进 Time Engine

芯片性能仿真最重要的内容之一是资源竞争：

```text
多个请求抢 cache port
多个 packet 抢 NoC link
多个 memory request 抢 DRAM bank
多条指令抢 issue slot
```

但这些竞争规则属于硬件模型，不属于时间内核。

Time Engine 只回答：

```text
事件什么时候发生？
事件按什么顺序发生？
事件是否取消？
```

资源模型回答：

```text
资源现在能不能用？
哪个请求先用？
资源会被占用多久？
排队请求什么时候被唤醒？
```

## 2. TimedResource 是什么

`TimedResource` 是建立在 Scheduler 之上的资源抽象。

```text
TimedResource
  +-- resource state
  +-- waiting queue
  +-- arbitration policy
  +-- service time
  +-- completion scheduling
```

引用关系：

```text
Component 使用 TimedResource
TimedResource 使用 Scheduler
Scheduler 创建 Event
EventQueue 执行 Event
```

所以 TimedResource 不需要直接操作 EventQueue。

## 3. 一个 Port 例子

假设一个 Cache 只有一个 read port，每次访问占用 2 cycles。

```text
CacheReadPort
  +-- busyUntil
  +-- waitingQueue
```

请求进入：

```text
request arrives
  +-- 如果 now >= busyUntil
        +-- 立即占用 port
        +-- busyUntil = now + 2 cycles
        +-- schedule completion

  +-- 如果 now < busyUntil
        +-- request 入队
        +-- 等当前请求完成后再仲裁
```

这样可以自然表达：

```text
端口带宽
排队延迟
利用率
平均等待时间
```

## 4. 典型资源类型

```text
CacheResource
  +-- tag port
  +-- data port
  +-- miss status holding register

NoCResource
  +-- input buffer
  +-- output link
  +-- router crossbar

DRAMResource
  +-- bank
  +-- row buffer
  +-- command bus
  +-- data bus

CoreResource
  +-- issue slot
  +-- functional unit
  +-- ROB entry
  +-- load/store queue entry
```

每种资源都有自己的策略，但都可以用 Scheduler 表达“未来完成”。

## 5. 资源模型和 Phase

资源仲裁通常适合放在 `Arbitrate` phase。

```text
Input
  +-- 收集请求

Compute
  +-- 计算请求属性

Arbitrate
  +-- 决定谁获得资源

Update
  +-- 更新 busyUntil / queue 状态

Trace
  +-- 记录利用率和队列长度
```

这能避免“谁先调用谁就赢”的隐式行为。

## 6. 资源模型和并发

并发仿真时，资源最好有明确归属。

```text
Core-local resource
  +-- 属于 core 的 Partition

Shared resource
  +-- 例如 L2 / NoC / Memory
  +-- 可以放到 Shared Partition
```

跨 Partition 请求共享资源时：

```text
发送 Remote Event 到资源所在 Partition
资源在自己的 Partition 内仲裁
完成后发送 Remote Event 返回请求方
```

这样可以避免多个线程直接修改同一个资源状态。
