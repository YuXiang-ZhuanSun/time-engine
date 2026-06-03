# 02. 架构设计

## 总体分层

当前框架分成四层：

```text
Application / Demo
  +-- 组装一个小芯片系统，提交初始请求

Modeling Layer
  +-- 描述硬件行为和资源竞争

Time Engine
  +-- 管理仿真时间、事件顺序、取消和 clock domain

Observability
  +-- 记录 trace 和 stats，解释仿真结果
```

## Time Engine 层

代码目录：

```text
src/time_engine/
```

这一层负责：

```text
SimTime
EventId
Phase
ClockDomain
Event ordering
scheduleAt / scheduleAfter / scheduleCycles
cancel
run / runUntil
TraceSink hook
```

这一层不负责：

```text
cache hit / miss 策略
DRAM 调度策略
NoC 路由策略
pipeline issue 策略
资源仲裁策略
```

TimeEngine 的设计原则是小而稳定。它只维护时间语义，不理解硬件语义。

## Modeling 层

代码目录：

```text
src/modeling/
```

这一层负责：

```text
组件行为
请求和响应
资源占用
stall / wakeup
latency 选择
```

当前只有一个最小 helper：

```text
TimedResource
  +-- 表达资源 service time
  +-- 记录 busyUntil
  +-- 统计 requests / queuedRequests / totalWait
  +-- 通过 TimeEngine 安排完成事件
```

## Observability 层

当前通过 `TraceSink` 预留观测接口。

它可以记录：

```text
event scheduled
event cancelled
event executed
```

观测层只能解释发生了什么，不能改变仿真顺序。

## 目录职责

```text
src/time_engine/
  +-- 可复用时序内核

src/modeling/
  +-- 建模辅助，不放具体 demo 逻辑

examples/
  +-- 小而完整的仿真例子

tests/
  +-- 时序语义测试

docs/
  +-- 当前设计、执行流程、MVP 状态和路线
```

## 核心边界

最重要的边界是：

```text
TimeEngine 不知道什么是 Core、Cache、NoC、DRAM。
Core、Cache、NoC、DRAM 也不直接操作 EventQueue。
组件只能通过 TimeEngine 的调度 API 安排未来动作。
```

这样后续即使硬件模型变复杂，时间内核也能保持简单。

