# Time Engine 设计文档

## 框架定位

Time Engine 是一个用于芯片性能仿真的时序框架，也是整个仿真系统的时间调度内核。

芯片系统由大量组件组成，例如 Core、Cache、NoC、DRAM Controller、DMA 和各种队列/端口。性能仿真需要描述这些组件在仿真时间线上如何产生动作、等待延迟、争用资源、触发后续事件，并在多线程环境下保持稳定可复现的执行顺序。

```text
Time Engine 负责：
  +-- 用统一的 SimTime 描述仿真时间
  +-- 用 Event 描述未来要发生的动作
  +-- 用 EventQueue 保证事件按正确顺序执行
  +-- 用 Scheduler 支持组件安排未来动作
  +-- 用 ClockDomain 支持 cycle-level 建模
  +-- 用 Trace / Stats 支持调试和性能分析
  +-- 用 Partition / Mailbox / Epoch 支持并发仿真
```

## 文档树

目录：

- [01. 总体方案](01-overview.md)
- [02. 核心概念](02-core-concepts.md)
- [03. 执行语义](03-execution-flow.md)
- [04. Component 模型](04-component-model.md)
- [05. 资源模型](05-resource-model.md)
- [06. 并发仿真](06-parallel-simulation.md)
- [07. API 草案](07-api-sketch.md)
- [08. 实现路线](08-roadmap.md)
- [09. 学习路径](09-learning-path.md)

```text
time-engine/
  +-- README.md
  |     +-- 文档入口，解释 Time Engine 到底是什么
  |
  +-- 01-overview.md
  |     +-- 总体方案：目标、边界、能力、系统全景
  |
  +-- 02-core-concepts.md
  |     +-- 核心机制：SimTime / Event / Phase / EventQueue / Scheduler
  |
  +-- 03-execution-flow.md
  |     +-- 执行语义：事件如何驱动仿真时间推进
  |
  +-- 04-component-model.md
  |     +-- 组件模型：Core / Cache / NoC / DRAM 如何接入
  |
  +-- 05-resource-model.md
  |     +-- 资源模型：端口、队列、bank、bus 的竞争如何表达
  |
  +-- 06-parallel-simulation.md
  |     +-- 并发仿真：Partition / Mailbox / Epoch / Lookahead
  |
  +-- 07-api-sketch.md
  |     +-- API 草案：第一版代码接口长什么样
  |
  +-- 08-roadmap.md
  |     +-- 实现路线：从单线程内核到并发仿真的阶段规划
  |
  +-- 09-learning-path.md
        +-- 学习路径：按什么顺序理解和实现
```

## 推荐阅读方式

如果你是第一次理解这套框架：

```text
01-overview
  -> 02-core-concepts
  -> 03-execution-flow
```

如果你要写芯片组件模型：

```text
04-component-model
  -> 05-resource-model
```

如果你关心仿真速度和多线程：

```text
06-parallel-simulation
```

如果你要开始实现：

```text
07-api-sketch
  -> 08-roadmap
  -> 09-learning-path
```

## 总体关系图

```text
Chip Performance Simulation
  +-- Component Models
  |     +-- Core
  |     +-- Cache
  |     +-- NoC
  |     +-- DRAM
  |
  +-- Resource Models
  |     +-- Port
  |     +-- Queue
  |     +-- Link
  |     +-- Bank
  |
  +-- Time Engine
  |     +-- SimTime
  |     +-- Event
  |     +-- EventQueue
  |     +-- Scheduler
  |     +-- ClockDomain
  |
  +-- Parallel Runtime
        +-- Partition
        +-- Local EventQueue
        +-- Remote Event
        +-- Mailbox
        +-- Epoch Barrier
```

## 最重要的设计边界

```text
Time Engine 回答：
  +-- 什么时候执行？
  +-- 谁先执行？
  +-- 能不能取消？
  +-- 仿真时间推进到哪里？

Component 回答：
  +-- 硬件模块收到请求后怎么处理？
  +-- 是 hit 还是 miss？
  +-- 是否 stall？
  +-- 产生什么后续请求？

TimedResource 回答：
  +-- 资源是否空闲？
  +-- 多个请求谁先用？
  +-- 用多久？
  +-- 后续请求怎么排队？

Parallel Runtime 回答：
  +-- 哪些组件可以并行跑？
  +-- 跨线程事件怎么传递？
  +-- 多个线程如何保持时间正确性？
```
