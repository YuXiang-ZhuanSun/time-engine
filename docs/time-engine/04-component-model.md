# 04. Component 模型

## 1. 为什么需要 Component

Time Engine 本身不应该知道芯片里有哪些模块。

它只提供时间能力：

```text
schedule
cancel
run
trace
```

真正的硬件行为应该放在 Component 里：

```text
Core 怎么发射指令
Cache 怎么判断 hit / miss
NoC 怎么转发 packet
DRAM Controller 怎么安排命令
```

因此 Component 是“硬件模块的仿真对象”。

## 2. Component 和 Time Engine 的关系

```text
Component
  +-- 持有或访问 Scheduler
  +-- 收到请求
  +-- 根据硬件策略决定延迟和后续动作
  +-- 调用 Scheduler 安排未来 callback
```

示例：

```text
L1 Cache 收到 load
  +-- 查 tag
  +-- 如果 hit
        +-- scheduleCycles(cpuClock, 4, returnData)

  +-- 如果 miss
        +-- scheduleCycles(cpuClock, 4, sendMissToL2)
```

Time Engine 不关心 hit / miss。它只关心 `returnData` 或 `sendMissToL2` 在什么时候执行。

## 3. 基础接口

第一版 Component 可以很轻：

```cpp
class Component {
public:
    virtual ~Component() = default;
    virtual void reset() {}
    virtual void start() {}
};
```

不要一开始就强制所有 Component 实现 `tick()`。

原因：

```text
很多模块大部分时间没有动作
强制 tick 会让模型退化成全系统逐周期扫描
```

## 4. Event-driven Component

适合事件驱动的模块：

```text
Cache
NoC
DRAM Controller
DMA
Interrupt Controller
```

特点：

```text
有请求时才创建事件
长延迟操作可以直接 schedule 到完成时间
空闲时没有执行开销
```

例子：

```text
DRAM Controller 收到 request
  +-- 进入 request queue
  +-- 仲裁 bank / command bus
  +-- scheduleAfter(tCAS + busLatency, complete)
```

## 5. Tick-driven Component

有些模块适合周期推进：

```text
Pipeline
Issue Queue
Scoreboard
ROB
Load Store Queue
```

这些模块通常每个 cycle 都要检查状态。

但它们仍然可以建立在 Event 之上：

```text
TickedComponent
  +-- tick()
  +-- 每个 clock edge 由 TimeEngine schedule 下一次 tick
```

也就是说：

```text
tick-driven 是 event-driven 的一种使用方式
```

## 6. Component 之间如何通信

组件通信不应该直接调用对方内部状态。

推荐方式：

```text
请求对象 Request
响应对象 Response
端口或接口 Port
回调 callback
```

简单例子：

```text
Core
  -> 发送 LoadRequest 给 L1

L1
  -> 未来调用 Core 的 wakeup callback
```

更复杂系统中，可以引入端口：

```text
CorePort
CachePort
NoCPort
MemoryPort
```

端口本身可以使用 [05-resource-model.md](05-resource-model.md) 中的资源模型表达带宽和排队。

## 7. Component 的分区归属

为了并发仿真，Component 未来需要属于某个 Partition。

```text
Component
  +-- partitionId
  +-- name
  +-- scheduler view
```

同一 Partition 内通信：

```text
local event
```

跨 Partition 通信：

```text
remote event
mailbox
```

详细设计见 [06-parallel-simulation.md](06-parallel-simulation.md)。
