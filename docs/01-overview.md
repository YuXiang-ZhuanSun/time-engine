# 01. 项目总览

## 项目定位

CA Chip Simulation Engine 是一个用于芯片性能仿真的时序框架。

它的第一阶段目标不是模拟完整芯片，也不是实现真实 cache、NoC、DRAM 策略，而是先建立一个确定、可测、可解释的时间内核，让后续硬件模型能在同一条仿真时间线上协作。

一句话概括：

```text
硬件模型决定发生什么，TimeEngine 决定它什么时候、按什么顺序发生。
```

## 为什么需要它

芯片性能仿真关心的是系统级时序行为：

```text
Core 什么时候发起 load？
L1 cache 多久判断出 miss？
L2 什么时候收到请求？
DRAM 资源忙不忙？
数据什么时候返回？
Core 什么时候 wakeup？
```

这些问题都可以抽象成事件：

```text
某个仿真时间点，执行某个动作。
这个动作可能更新状态，也可能安排新的未来动作。
```

所以第一版采用离散事件仿真，而不是每个 cycle 扫描所有组件。

## 为什么不用全系统 tick

简单的全系统 tick 写法是：

```text
for each cycle:
  for each component:
    component.tick()
```

它容易理解，但对性能仿真并不总是合适：

```text
大量组件大部分时间没有动作
长延迟事件中间不需要逐 cycle 扫描
CPU / NoC / DRAM 可能属于不同 clock domain
```

离散事件方式是：

```text
只有未来真的有动作时，才创建 Event。
TimeEngine 直接跳到下一个 Event 的时间。
```

如果当前时间是 `1000 ps`，下一个事件在 `50000 ps`，中间没有任何动作，仿真可以直接跳到 `50000 ps`。

## 当前版本做什么

当前 MVP 已实现：

```text
SimTime / Duration / EventId
Phase
ClockDomain
TimeEngine
scheduleAt / scheduleAfter / scheduleCycles
cancel
run / runUntil
TraceSink
TimedResource
mini memory system demo
time engine tests
```

## 当前版本不做什么

当前 MVP 暂不实现：

```text
真实 cache tag array
真实 cache replacement
cache coherence
NoC routing
DRAM timing policy
完整 pipeline
多线程 partition
复杂 stats 系统
```

这些不是被否定，而是要等时间内核和最小 demo 稳定后再逐步加入。

