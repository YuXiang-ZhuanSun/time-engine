# 05. 用户说明书：开发芯片时序模型

这份说明书面向 Time Engine 的使用者。目标不是讲抽象架构，而是让你读完后能开始写自己的芯片时序模型。

建议按顺序读。前半部分先解释框架思想和关键概念，后半部分再讲如何写组件、请求、资源、trace 和 stats。

## 0. 先认识这几个词

后面会反复出现一些概念。先用最短的话说明它们，避免读到一半才猜意思。

```text
仿真时间 SimTime:
  框架里的全局时间。当前约定单位是 ps。

事件 Event:
  某个未来时间点要执行的一段动作。

回调 Callback:
  事件到时间后真正执行的 C++ 函数对象，通常是 lambda。

事件队列 EventQueue:
  TimeEngine 内部保存未来事件的队列。用户不要直接操作它。

阶段 Phase:
  同一时间点内的执行通道，用来规定 Input / Compute / Arbitrate / Update / Trace 的先后。

时钟域 ClockDomain:
  把“几个 cycle 后”转换成全局 SimTime 的对象。

资源 TimedResource:
  一个会被请求占用一段时间的东西，例如 cache port、NoC link、DRAM port。

trace:
  事件发生过程的流水账，用来解释仿真为什么这样执行。
  它不是芯片模型逻辑，只是调试日志。

stats:
  性能统计，例如 latency、请求数、排队等待时间。
```

这份说明书的阅读方式是：

```text
先看这些对象如何合作
再理解概念和 API
再看组件模板
最后照着 demo 写自己的模型
```

## 1. 先看这些东西如何合作

如果一上来就看二十个接口，会很难记住它们各自干什么。先看一张协作图。

![Time Engine 对象协作图](assets/time-engine-collaboration.png)

```text
你的芯片模型代码
  +-- 创建 TimeEngine
  +-- 创建 ClockDomain
  +-- 创建 TimedResource
  +-- 创建初始请求
  |
  v
TimeEngine
  +-- scheduleAt / scheduleAfter / scheduleCycles
  +-- 把未来动作放进 EventQueue
  +-- run() 时按时间顺序执行 callback
  |
  v
callback
  +-- 回到你的组件模型
  +-- 组件判断 hit / miss / stall / wakeup
  +-- 组件继续 schedule 下一个未来动作
  |
  v
TraceSink / stats
  +-- 记录事件流水账
  +-- 统计 latency、请求数、排队等待
```

用一句话说：

```text
组件决定“要发生什么”，TimeEngine 决定“什么时候发生”。
```

## 2. 一条 load 请求如何走完整条链路

下面这条链路是当前 demo 的核心：

![Mini memory system load 时间线](assets/mini-memory-timeline.png)

```text
Core 发起 load
  -> L1 固定 miss
    -> 4 个 CPU cycle 后访问 L2
      -> L2 固定 miss
        -> 8 个 CPU cycle 后访问 DRAM
          -> DRAM port service 50000 ps
            -> Core wakeup
```

它背后的接口合作关系是：

```text
1. ClockDomain cpu("cpu", 1000)
   +-- 定义 CPU 一个 cycle 是 1000 ps

2. TimeEngine engine(&trace)
   +-- 创建时间内核，并挂上可选 trace 日志

3. TimedResource dramPort("dram-port", 50000)
   +-- 表达 DRAM port 一次 service 要 50000 ps

4. engine.scheduleAt(0, Phase::Input, accessL1, ..., "core-load")
   +-- 把初始 load 请求放到 T=0

5. accessL1 callback 执行
   +-- 这是你的 L1 模型逻辑
   +-- 它调用 scheduleCycles(cpu, 4, ..., accessL2)

6. accessL2 callback 执行
   +-- 这是你的 L2 模型逻辑
   +-- 它调用 scheduleCycles(cpu, 8, ..., accessDram)

7. accessDram callback 执行
   +-- 这是你的 DRAM 入口逻辑
   +-- 它调用 dramPort.request(...)

8. TimedResource 内部调用 engine.scheduleAt(completion, ..., wakeCore)
   +-- 资源 service 完成后，安排 Core wakeup

9. wakeCore callback 执行
   +-- 请求完成，计算 latency
```

所以这些接口不是散的：

```text
ClockDomain
  +-- 帮 scheduleCycles 把 cycle 转成 SimTime

TimeEngine
  +-- 保存和执行未来 callback

TimedResource
  +-- 用 TimeEngine 安排资源完成事件

TraceSink
  +-- 观察 TimeEngine 的 schedule / execute，不参与模型决策

stats
  +-- 你的模型在请求开始和完成时自己统计
```

读后面的 API 时，请一直把这条链路放在脑子里。

## 3. Time Engine 是什么

Time Engine 是一个**离散事件时序内核**。

它不模拟信号电平，也不负责 cache replacement、DRAM policy、NoC routing 这些硬件策略。它只负责一件事：

```text
管理仿真时间线上未来要发生的动作。
```

在芯片性能仿真中，我们经常要表达：

```text
Core 在 T=0 发起 load
L1 在 4 cycles 后发现 miss
L2 在 8 cycles 后把请求送到 DRAM
DRAM 在 50000 ps 后返回数据
Core 被 wakeup
```

这里先解释两个新词。

```text
事件:
  对未来动作的描述。例如“4000 ps 后访问 L2”。

callback:
  事件到时间后执行的代码。例如真正调用 L2 的函数。
```

上面的动作都可以变成事件：

```text
某个时间点，执行某个 callback。
callback 执行后，可以继续安排新的未来事件。
```

Time Engine 的工作就是保存这些事件，按正确顺序执行它们，并把当前仿真时间推进到事件发生的时间。

## 4. Time Engine 的设计思想

### 4.1 时间不连续流动，而是跳到下一个事件

普通逐周期仿真可能会这样写：

```text
for each cycle:
  for each component:
    component.tick()
```

这种方式简单，但当很多模块长时间没有动作时，会浪费大量仿真开销。

Time Engine 采用事件驱动：

```text
如果 T=1000 ps 到 T=50000 ps 之间没有事件，
TimeEngine 不会逐 ps 或逐 cycle 扫描，
而是直接把 now 跳到 50000 ps。
```

这适合芯片性能仿真，因为性能模型通常关心请求、延迟、资源竞争和返回时间，而不是每根信号线每个周期的值。

### 4.2 时间内核和硬件策略分离

Time Engine 负责：

```text
当前仿真时间是多少？
未来有哪些事件？
哪个事件先执行？
事件能不能取消？
如何把 cycle 延迟转换成统一时间？
```

硬件模型负责：

```text
L1 是 hit 还是 miss？
DRAM 要服务多久？
NoC packet 走哪条 link？
Core 是否 stall？
资源竞争时谁先拿到 port？
```

这个边界非常重要。Time Engine 不应该知道什么是 cache miss；cache 模型也不应该直接操作 EventQueue。

## 5. Time Engine 的内部结构

这一节会出现 `EventQueue`、`pending_` 和 `sequence`。先解释一下。

```text
EventQueue:
  TimeEngine 内部的未来事件队列。它负责找出下一个应该执行的事件。

pending_:
  TimeEngine 内部的 EventId 到 Event 的索引。它让 cancel(id) 能快速找到事件。

sequence:
  事件创建顺序。用于在其他排序字段完全相同时保持确定性。
```

当前 MVP 的核心对象关系如下：

```text
TimeEngine
  +-- now_
  |     +-- 当前仿真时间
  |
  +-- EventQueue
  |     +-- priority_queue，保存未来事件
  |
  +-- pending_
  |     +-- EventId -> Event，用于取消事件
  |
  +-- sequence generator
  |     +-- 给事件分配创建顺序，保证稳定排序
  |
  +-- TraceSink
        +-- 可选，用于记录 scheduled / cancelled / executed
```

事件进入 TimeEngine 后，会被放进 EventQueue。EventQueue 会用一个固定的“挑选下一个事件”的规则。

这个规则不是让模型开发者每天手动管理四个字段，而是 TimeEngine 内部为了保证可复现性使用的排序键：

```text
time -> phase -> priority -> sequence
```

可以把它读成一句话：

```text
先看事件发生时间；
同一时间点内，再看它属于哪个通道；
同一通道里，除非有特殊需要，否则保持默认优先级；
最后用创建顺序兜底，保证每次运行结果稳定。
```

对普通模型开发者来说，最重要的是：

```text
大多数事件使用 Phase::Update。
外部输入或初始请求使用 Phase::Input。
priority 保持默认 0。
sequence 由 TimeEngine 自动生成，不需要用户管理。
```

执行时：

```text
1. 从 EventQueue 取出最早事件
2. 如果事件已取消，跳过
3. now_ = event.time
4. 通知 TraceSink：事件被执行
5. 执行 event.callback()
6. callback 可以继续 schedule 新事件
```

代码对应：

```text
src/time_engine/time_engine.hpp
src/time_engine/time_engine.cpp
```

## 6. 关键类型

### 6.1 `SimTime`

```cpp
using SimTime = std::uint64_t;
```

`SimTime` 是全局仿真时间。当前约定按 ps 理解。

例如：

```text
T=0       表示仿真开始
T=4000    表示 4000 ps
T=62000   表示 62000 ps
```

为什么不用全局 cycle？

因为芯片里可能有多个 clock domain：

```text
CPU: 1 GHz   -> 1 cycle = 1000 ps
NoC: 2 GHz   -> 1 cycle = 500 ps
DRAM: 800 MHz -> 1 cycle = 1250 ps
```

最终所有事件都要放到同一条时间线上排序，所以 TimeEngine 使用统一的 `SimTime`。

### 6.2 `Duration`

```cpp
using Duration = std::uint64_t;
```

`Duration` 表示一段时间长度，也按 ps 理解。

例如：

```cpp
engine.scheduleAfter(200, Phase::Update, callback);
```

表示从当前 `now()` 开始，200 ps 后执行 callback。

### 6.3 `EventId`

```cpp
using EventId = std::uint64_t;
```

每个事件创建后都会得到一个 `EventId`。

用途：

```text
取消未来事件
调试事件
trace 里识别事件
```

例如：

```cpp
EventId id = engine.scheduleAt(1000, Phase::Update, callback);
engine.cancel(id);
```

### 6.4 `Callback`

```cpp
using Callback = std::function<void()>;
```

事件真正执行的动作就是 callback。

callback 可以：

```text
更新组件状态
调用下游组件
统计 latency
安排新的未来事件
```

callback 不应该：

```text
把事件安排到过去
直接操作 EventQueue
依赖不稳定的外部对象生命周期
```

## 7. `ClockDomain` 是什么

先解释“时钟域”。

```text
时钟域:
  使用同一个 clock period 的一组模块。
  例如 CPU 可能每 1000 ps 一个 cycle，NoC 可能每 500 ps 一个 cycle。
```

`ClockDomain` 表示一个时钟域。

芯片里的模块不一定都跑在同一个频率下。例如：

```text
CPU clock: 1000 ps / cycle
NoC clock: 500 ps / cycle
DRAM clock: 1250 ps / cycle
```

当你写硬件模型时，很自然会说：

```text
L1 hit latency 是 4 个 CPU cycle
NoC hop latency 是 2 个 NoC cycle
DRAM command 延迟是若干 DRAM cycle
```

但是 TimeEngine 需要统一的 `SimTime`。`ClockDomain` 的作用就是：

```text
把某个时钟域里的 cycle 数转换成全局 SimTime。
```

## 8. `ClockDomain` 接口格式

当前接口：

```cpp
class ClockDomain {
public:
    ClockDomain(std::string name, SimTime period);

    const std::string& name() const;
    SimTime period() const;
    SimTime nextEdge(SimTime now) const;
    SimTime cyclesFromNow(SimTime now, std::uint64_t cycles) const;
};
```

### 8.1 构造函数

```cpp
ClockDomain cpu("cpu", 1000);
```

含义：

```text
name = "cpu"
period = 1000 ps
```

也就是 CPU 时钟域一个 cycle 是 1000 ps。

`period` 不能是 0。当前实现会在构造时检查，传 0 会抛出异常。

### 8.2 `nextEdge(now)`

```cpp
SimTime edge = cpu.nextEdge(2300);
```

如果 CPU period 是 1000 ps，那么：

```text
当前 now = 2300 ps
下一个 CPU clock edge = 3000 ps
```

如果当前已经在 edge 上：

```text
nextEdge(3000) = 3000
```

### 8.3 `cyclesFromNow(now, cycles)`

```cpp
SimTime t = cpu.cyclesFromNow(2300, 3);
```

计算方式：

```text
先对齐到下一个 edge:
  nextEdge(2300) = 3000

再加 3 个 cycle:
  3000 + 3 * 1000 = 6000
```

所以结果是：

```text
t = 6000 ps
```

这就是 `scheduleCycles` 背后的时间转换逻辑。

## 9. `Phase` 是什么

先解释为什么需要它。

```text
如果两个事件都发生在 T=1000 ps，仅靠时间已经分不出谁先执行。
Phase 就是在同一个时间点里再划分几条固定执行通道。
```

`Phase` 是**同一仿真时间点内的事件通道**。

它不是硬件里的真实流水级，也不是要求每个模型开发者都精细管理的优先级系统。

可以把它想成 TimeEngine 在同一个时间点里划出的几条固定车道：

```text
T=1000 ps
  Input lane
  Compute lane
  Arbitrate lane
  Update lane
  Trace lane
```

如果两个事件都发生在 `T=1000 ps`，TimeEngine 会先执行前面车道里的事件，再执行后面车道里的事件。

为什么需要它？

假设多个事件都发生在 `T=1000 ps`。如果只按时间排序，它们的时间完全相同。那谁先执行？

如果没有明确规则，执行顺序可能隐含依赖事件创建顺序，模型会变得难调试。

所以我们把同一时间点拆成几个通道：

```cpp
enum class Phase : std::uint8_t {
    Input = 0,
    Compute = 1,
    Arbitrate = 2,
    Update = 3,
    Trace = 4,
};
```

推荐理解：

```text
Input:
  外部输入进入系统。例如 workload 在 T=0 注入一个 load。

Compute:
  组件做本地计算。例如判断 hit / miss、计算路由、估算延迟。

Arbitrate:
  资源拥有者解决竞争。例如多个请求抢一个 port。

Update:
  提交结果。例如更新状态、完成请求、唤醒上游。

Trace:
  记录最终状态和统计。普通模型代码通常不用这个 phase。
```

同一时间点内，执行顺序固定为：

```text
Input -> Compute -> Arbitrate -> Update -> Trace
```

### 9.1 模型开发者应该怎么选 Phase

第一版请按下面这个简单规则用：

```text
外部 driver / workload 注入初始请求:
  用 Phase::Input

普通延迟完成、下游调用、上游 wakeup:
  用 Phase::Update

资源对象内部做仲裁:
  资源对象自己用 Phase::Arbitrate

trace / stats 系统记录最终状态:
  trace / stats 系统自己用 Phase::Trace
```

也就是说，普通组件开发者大多数时候只需要记住：

```text
不知道选什么，就用 Phase::Update。
只有“外部请求进入仿真”的入口事件，用 Phase::Input。
```

`Compute` 和 `Arbitrate` 是后续更复杂模型使用的工具，不是 MVP 阶段的日常负担。

### 9.2 Phase 不是 priority

`Phase` 解决的是“同一时间点内，不同类型动作的大顺序”。

`priority` 解决的是“同一时间点、同一 phase 内，极少数事件需要显式插队”。

两者不要混用。

推荐策略：

```text
普通模型:
  priority 永远保持默认 0

资源模型或框架内部:
  如果确实需要稳定插队，再使用 priority

如果你发现很多业务逻辑都依赖 priority:
  说明模型边界可能有问题，应该重新设计 phase 或资源仲裁逻辑
```

## 10. 调度接口的共同含义

先解释“调度”。

```text
调度 schedule:
  不是立刻执行一个动作，而是把动作登记到未来某个仿真时间点。
```

TimeEngine 有三个主要调度接口：

```cpp
scheduleAt(...)
scheduleAfter(...)
scheduleCycles(...)
```

它们的共同含义是：

```text
创建一个未来事件，放入 TimeEngine 的 EventQueue。
等仿真时间推进到该事件时间时，执行 callback。
```

当前完整签名：

```cpp
EventId scheduleAt(
    SimTime time,
    Phase phase,
    Callback callback,
    int priority = 0,
    std::string label = {});

EventId scheduleAfter(
    Duration delay,
    Phase phase,
    Callback callback,
    int priority = 0,
    std::string label = {});

EventId scheduleCycles(
    const ClockDomain& clock,
    std::uint64_t cycles,
    Phase phase,
    Callback callback,
    int priority = 0,
    std::string label = {});
```

参数解释：

```text
time / delay / cycles:
  决定事件什么时候发生

phase:
  决定同一时间点内属于哪个阶段

callback:
  到时间后真正执行的动作

priority:
  同一 time、同一 phase 内的显式优先级，数值越小越先执行。
  普通模型开发者不要主动管理它，保持默认 0。

label:
  给 trace 和调试用的名字，不影响执行逻辑
```

返回值：

```text
EventId
  +-- 用于后续 cancel
  +-- 也可以用于 trace/debug
```

## 11. `scheduleAt` 的内涵

`scheduleAt` 用于**绝对时间调度**。

```cpp
engine.scheduleAt(5000, Phase::Update, callback, 0, "event-name");
```

含义：

```text
在全局仿真时间 T=5000 ps，
以 Update phase 执行 callback。
```

它适合：

```text
提交初始 workload
外部 driver 在某个固定时间注入请求
资源已经算出了明确 completion time
```

例子：

```cpp
engine.scheduleAt(0, Phase::Input, [&] {
    core.issueLoad(0x1000);
}, 0, "core-issue-load");
```

语义：

```text
仿真开始 T=0 时，让 Core 发起一个 load。
```

约束：

```text
不能 schedule 到过去。
如果 time < engine.now()，当前实现会抛出 std::invalid_argument。
```

## 12. `scheduleAfter` 的内涵

`scheduleAfter` 用于**相对时间调度**。

```cpp
engine.scheduleAfter(200, Phase::Update, callback, 0, "link-arrive");
```

含义：

```text
从当前 now() 开始，200 ps 后执行 callback。
```

等价于：

```cpp
engine.scheduleAt(engine.now() + 200, Phase::Update, callback, 0, "link-arrive");
```

它适合：

```text
固定 ps 延迟
链路传播延迟
组合逻辑延迟
已经用 ps 表达的 service time
```

## 13. `scheduleCycles` 的内涵

`scheduleCycles` 用于**按时钟域 cycle 调度**。

```cpp
ClockDomain cpu("cpu", 1000);

engine.scheduleCycles(cpu, 4, Phase::Update, callback, 0, "l1-hit-complete");
```

含义：

```text
在 cpu clock domain 中，4 个 cycle 后执行 callback。
```

但它不是简单地做：

```text
now + 4 * period
```

而是：

```text
clock.cyclesFromNow(now, 4)
  = clock.nextEdge(now) + 4 * clock.period()
```

例如：

```text
cpu period = 1000 ps
now = 2300 ps
cycles = 3

nextEdge(2300) = 3000
target = 3000 + 3 * 1000 = 6000 ps
```

这样可以让 cycle-level 模型在 clock edge 上更新状态，语义更清楚。

它适合：

```text
cache hit latency
pipeline stage latency
issue / writeback latency
以 cycle 表达的 NoC hop latency
```

## 14. `run` 和 `runUntil`

### `run`

```cpp
engine.run();
```

含义：

```text
一直执行事件，直到 EventQueue 为空。
```

适合：

```text
小 demo
有限 workload
测试
```

### `runUntil`

```cpp
engine.runUntil(100000);
```

含义：

```text
执行所有 time <= 100000 ps 的事件。
超过这个时间的事件保留在队列中，不执行。
```

适合：

```text
运行固定仿真窗口
分阶段推进仿真
调试某个时间范围内的行为
```

## 15. `cancel` 的内涵

```cpp
EventId id = engine.scheduleAt(1000, Phase::Update, callback, 0, "speculative-event");
bool ok = engine.cancel(id);
```

含义：

```text
如果该事件还没有执行，就取消它。
取消成功返回 true。
如果事件不存在、已经执行、或已经取消，返回 false。
```

当前实现使用 lazy cancel：

```text
cancel 时标记事件取消，并从 pending_ 移除。
事件未来从队列弹出时，如果发现 cancelled，就跳过。
```

适合表达：

```text
branch misprediction 后取消 speculative event
pipeline flush
timeout 提前满足
reset
```

## 16. 最小使用流程

一个最小模型通常按这个顺序写：

```text
1. 创建 TraceSink，可选
2. 创建 TimeEngine
3. 创建 ClockDomain
4. 创建组件或资源
5. 用 scheduleAt 提交初始事件
6. 在 callback 里继续 schedule 后续事件
7. 调用 run 或 runUntil
8. 查看 trace / stats
```

最小代码：

```cpp
#include "time_engine/time_engine.hpp"

using namespace ca::sim;

int main() {
    TimeEngine engine;
    ClockDomain cpu("cpu", 1000);

    engine.scheduleAt(0, Phase::Input, [&] {
        engine.scheduleCycles(cpu, 4, Phase::Update, [&] {
            // 这里写 4 个 CPU cycle 后要发生的动作
        }, 0, "finish-after-4-cycles");
    }, 0, "start");

    engine.run();
    return 0;
}
```

执行逻辑：

```text
T=0:
  执行 start
  start 安排 finish-after-4-cycles

T=4000:
  执行 finish-after-4-cycles
```

## 17. 如何写一个芯片组件

当前 MVP 没有强制 `Component` 基类。推荐先写普通 C++ 类，把 `TimeEngine`、`ClockDomain` 和下游组件作为依赖传入。

示例：一个固定 miss 的 L1 cache。

```cpp
class L2Cache;

class L1Cache {
public:
    L1Cache(TimeEngine& engine, ClockDomain& clock, L2Cache& l2)
        : engine_(engine), clock_(clock), l2_(l2) {}

    void recvLoad(std::uint64_t address) {
        engine_.scheduleCycles(clock_, 4, Phase::Update, [this, address] {
            sendMissToL2(address);
        }, 0, "l1-send-miss-to-l2");
    }

private:
    void sendMissToL2(std::uint64_t address);

    TimeEngine& engine_;
    ClockDomain& clock_;
    L2Cache& l2_;
};
```

设计原则：

```text
组件负责硬件策略。
组件通过 TimeEngine 安排未来动作。
组件不要直接操作 EventQueue。
组件不要修改别的组件内部状态。
```

## 18. 如何定义请求对象

随着模型变复杂，不要只传 address。建议定义请求对象。

```cpp
struct MemoryRequest {
    std::uint64_t id;
    std::uint64_t address;
    SimTime issueTime;
};
```

组件之间传递 `MemoryRequest`：

```cpp
void recvLoad(MemoryRequest request);
void recvMiss(MemoryRequest request);
void recvMemoryRequest(MemoryRequest request);
```

好处：

```text
trace 可以打印 request id
stats 可以按 request 计算 latency
调试时能追踪一条请求的完整路径
未来可以加入 type / size / qos / source 字段
```

lambda 捕获 request 时，第一版建议按值捕获：

```cpp
engine.scheduleCycles(clock, 4, Phase::Update, [this, request] {
    next_.recvMiss(request);
}, 0, "send-request");
```

这样 callback 执行时 request 数据仍然有效。

## 19. 如何表达资源占用

先解释“资源占用”。

```text
资源占用:
  某个请求在一段时间内独占或使用一个硬件资源。
  如果后续请求到来时资源还没释放，后续请求就要等待。
```

简单资源可以用 `TimedResource`。

```cpp
TimedResource dramPort("dram-port", 50000);

dramPort.request(engine, Phase::Update, [&] {
    // request complete
}, "dram-complete");
```

它的语义：

```text
start = max(engine.now(), busyUntil)
wait = start - engine.now()
completion = start + serviceTime
busyUntil = completion
schedule completion callback
```

也就是说：

```text
如果资源空闲，请求立刻开始 service。
如果资源忙，请求等到 busyUntil 后开始 service。
completion 时执行 callback。
```

它会统计：

```text
requests
queuedRequests
totalWait
busyUntil
```

适合第一版表达：

```text
单端口 cache access
单条 NoC link
简单 DRAM service slot
简单 functional unit
```

## 20. 如何写 trace

先把 trace 理解成“事件日志”。

```text
没有 trace:
  你只知道最终 latency 是多少。

有 trace:
  你能看到每个事件什么时候被安排、什么时候被执行。
```

trace 不会改变仿真结果。它只是帮助你读懂时间线。

继承 `TraceSink`：

```cpp
class MyTrace : public TraceSink {
public:
    void onScheduled(const EventView& event) override {
        // event.time / event.phase / event.label
    }

    void onCancelled(EventId id) override {
        // cancelled event id
    }

    void onExecuted(const EventView& event) override {
        // executed event
    }
};
```

创建 engine 时传入：

```cpp
MyTrace trace;
TimeEngine engine(&trace);
```

建议每个事件都写清楚 `label`：

```cpp
engine.scheduleCycles(cpu, 4, Phase::Update, callback, 0, "l1-send-to-l2");
```

好的 label 应该能回答：

```text
这是哪个组件安排的？
这个事件要做什么？
它属于哪条请求路径？
```

## 21. 如何写 stats

当前 MVP 没有统一 stats 框架。建议先在组件里维护少量直接统计。

示例：

```cpp
struct CoreStats {
    std::uint64_t loads = 0;
    SimTime totalLatency = 0;
};
```

在请求开始时记录：

```cpp
request.issueTime = engine.now();
```

在请求完成时统计：

```cpp
stats.loads += 1;
stats.totalLatency += engine.now() - request.issueTime;
```

第一阶段优先统计：

```text
request count
total latency
average latency
resource requests
queued requests
total wait
```

## 22. 推荐开发模板

开发一个新组件时，可以按这个顺序：

```text
1. 写请求结构
2. 写组件类构造函数，注入 TimeEngine / ClockDomain / 下游组件
3. 写 recvXxx 入口函数
4. 在 recvXxx 中决定延迟和后续动作
5. 用 scheduleCycles / scheduleAfter / scheduleAt 安排 callback
6. 在 callback 中调用下游组件或唤醒上游
7. 加 label
8. 加最小 stats
9. 写 demo
10. 写测试
```

组件骨架：

```cpp
class MyComponent {
public:
    MyComponent(TimeEngine& engine, ClockDomain& clock)
        : engine_(engine), clock_(clock) {}

    void recvRequest(MyRequest request) {
        engine_.scheduleCycles(clock_, latencyCycles_, Phase::Update, [this, request] {
            completeRequest(request);
        }, 0, "my-component-complete");
    }

private:
    void completeRequest(MyRequest request) {
        // update stats or call next component
    }

    TimeEngine& engine_;
    ClockDomain& clock_;
    std::uint64_t latencyCycles_ = 1;
};
```

## 23. 常见建模模式

### 固定延迟

```cpp
engine.scheduleCycles(clock, 4, Phase::Update, callback, 0, "fixed-latency");
```

适合：

```text
L1 hit
pipeline stage
固定处理延迟
```

### 长延迟服务

```cpp
resource.request(engine, Phase::Update, callback, "resource-complete");
```

适合：

```text
DRAM access
NoC link
functional unit
```

### 上游 stall / wakeup

```text
Core 发请求
  +-- 记录 outstanding request
  +-- 不继续执行依赖指令

Memory 返回
  +-- callback 调用 core.wakeup(request)
```

TimeEngine 不理解 stall。stall 是 Core 模型自己的状态。

### 取消未来事件

```cpp
EventId id = engine.scheduleAt(time, Phase::Update, callback, 0, "speculative-event");
engine.cancel(id);
```

适合：

```text
branch misprediction
pipeline flush
speculative request 撤销
timeout 提前满足
```

## 24. 常见错误

### 错误 1：组件直接改下游内部状态

不要这样：

```text
Core 直接修改 L1 的内部 queue
```

推荐：

```text
Core 调用 L1.recvLoad(request)
L1 自己决定如何排队、延迟、返回
```

### 错误 2：TimeEngine 里写硬件策略

不要把 hit / miss、bank policy、routing 写进 TimeEngine。

TimeEngine 应该完全不知道这些概念。

### 错误 3：所有东西都 tick

不要一开始就给所有组件加 `tick()`。

事件驱动组件空闲时不应该有执行开销。只有 pipeline、issue queue 这类确实每 cycle 需要检查状态的模型，才需要 tick-like 行为。

### 错误 4：事件 label 太模糊

不要写：

```text
"event"
"callback"
"done"
```

推荐：

```text
"core-load"
"l1-send-to-l2"
"dram-complete"
```

## 25. 如何判断模型写得是否合理

一个好的时序模型应该满足：

```text
读 trace 能看懂请求路径
每个组件只负责自己的硬件行为
TimeEngine 只负责时间和顺序
stats 能解释 latency 来源
同样输入多次运行结果一致
demo 足够小，可以手算预期时间
```

如果 demo 的时间线不能手算，说明模型可能已经太复杂，需要拆小。

## 26. 当前最佳学习路径

建议按这个顺序读：

```text
1. docs/03-simulation-flow.md
2. docs/05-user-manual.md
3. src/time_engine/time_engine.hpp
4. tests/time_engine_tests.cpp
5. src/modeling/timed_resource.hpp
6. examples/mini_memory_system.cpp
```

读完后，建议做一个小练习：

```text
让 Core 连续发两个 load。
两个 load 都 miss 到 DRAM。
观察第二个 load 因为 DRAM port 忙而排队。
输出两个 load 的不同 latency。
```

这个练习完成后，就已经具备开发简单芯片时序模型的基本能力。
