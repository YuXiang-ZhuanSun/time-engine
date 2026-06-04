# 03. 仿真执行流程

## 主循环

TimeEngine 的主循环可以理解为：

```text
while EventQueue is not empty:
  取出排序最早的事件
  如果事件已取消，跳过
  如果事件超过 runUntil 的结束时间，停止
  now = event.time
  记录 execute trace
  执行 callback
```

callback 执行时可以继续 schedule 新事件。

## 事件排序

事件按稳定规则排序。这个规则是 TimeEngine 内部用来挑选“下一个要执行的事件”的，不是要求模型开发者手工管理所有字段：

```text
time -> phase -> priority -> sequence
```

含义：

```text
time:
  仿真时间早的先执行

phase:
  同一时间点内按事件通道执行。
  普通模型主要用 Input 和 Update。

priority:
  同一 phase 内允许显式优先级。
  普通模型保持默认 0，不建议主动管理。

sequence:
  同条件下按创建顺序执行，保证确定性
```

当前 phase 可以理解成同一时间点内的几条固定通道：

```text
Input
Compute
Arbitrate
Update
Trace
```

MVP demo 主要用 `Input` 和 `Update`。

使用规则：

```text
外部 workload 注入请求:
  Phase::Input

普通完成事件、下游调用、上游 wakeup:
  Phase::Update

资源对象内部仲裁:
  Phase::Arbitrate

trace / stats 系统记录:
  Phase::Trace
```

如果不知道该选哪个 phase，就先选 `Phase::Update`。不要用 `priority` 去弥补模型边界不清的问题。

## ClockDomain

TimeEngine 使用统一 `SimTime`，默认按 ps 理解。

硬件模型可以用 cycle 表达延迟：

```cpp
ClockDomain cpu("cpu", 1000);
engine.scheduleCycles(cpu, 4, Phase::Update, callback);
```

含义是：

```text
CPU period = 1000 ps
4 cycles = 4000 ps
```

如果当前时间不在 clock edge 上，ClockDomain 会先对齐到下一个 edge，再加 cycle 延迟。

## Mini Memory System 时间线

当前 demo：

```text
Core -> L1 -> L2 -> DRAM -> Core wakeup
```

执行过程：

```text
T=0
  Core 提交 core-load 事件

T=0
  L1 callback 执行
  L1 固定 miss
  schedule send-to-l2 at 4000 ps

T=4000
  L2 callback 执行
  L2 固定 miss
  schedule send-to-dram at 12000 ps

T=12000
  DRAM callback 执行
  TimedResource 接收请求
  serviceTime = 50000 ps
  schedule dram-complete at 62000 ps

T=62000
  dram-complete callback 执行
  Core wakeup
```

最终 latency：

```text
62000 ps - 0 ps = 62000 ps
```

## Trace 和 Stats

demo 会输出 schedule 和 execute：

```text
schedule T=4000 phase=Update label=send-to-l2
execute  T=4000 phase=Update label=send-to-l2
```

二者一起可以解释完整因果链：

```text
谁安排了未来事件
未来事件是否真的执行
每个事件在什么时间执行
```

stats 当前输出：

```text
load latency
DRAM requests
DRAM queued requests
DRAM total wait
```
