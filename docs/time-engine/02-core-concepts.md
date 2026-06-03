# 02. 核心概念

## 1. 为什么需要这些概念

Time Engine 要解决的问题是：

```text
组件可以安排未来动作
未来动作要按时间顺序执行
同一时间点的动作也要有稳定顺序
组件要能表达 cycle 延迟
事件要能被取消
```

所以我们需要这些核心概念：

```text
SimTime
  +-- 描述仿真时间

Event
  +-- 描述未来动作

Phase
  +-- 描述同一时间点内的阶段顺序

EventQueue
  +-- 保存并排序未来动作

Scheduler
  +-- 给组件提供调度入口

ClockDomain
  +-- 把 cycle 转成 SimTime
```

## 2. SimTime

`SimTime` 是全局仿真时间。

```cpp
using SimTime = uint64_t;
using Duration = uint64_t;
```

推荐默认单位：

```text
1 tick = 1 ps
```

为什么不用“全局 cycle”：

```text
芯片里可能有多个 clock domain
CPU / NoC / DRAM 的频率可能不同
用 ps 这样的统一时间单位更容易描述跨域延迟
```

例子：

```text
CPU 1GHz      -> period = 1000 ps
NoC 2GHz      -> period = 500 ps
DRAM 800MHz   -> period = 1250 ps
```

引用关系：

```text
Event.time 使用 SimTime
ClockDomain.period 使用 SimTime
Scheduler 用 SimTime 创建事件
TimeEngine.now 返回 SimTime
```

## 3. Event

`Event` 是“未来某个时间点要执行的动作”。

```text
Event
  +-- id
  |     +-- 事件唯一编号，用于取消和调试
  |
  +-- time
  |     +-- 事件发生的 SimTime
  |
  +-- phase
  |     +-- 同一 time 内的阶段
  |
  +-- priority
  |     +-- 同一 phase 内的优先级
  |
  +-- sequence
  |     +-- 创建顺序，用于确定性排序
  |
  +-- callback
  |     +-- 到时间后执行的逻辑
  |
  +-- cancelled
        +-- 是否已经取消
```

一个 Event 不应该包含硬件策略。它只是包装一个未来动作。

例如：

```text
L1 Cache miss 之后访问 L2
  -> 是 Component 的策略

在 T=5000 ps 调用 sendToL2()
  -> 是 Event
```

## 4. Event 的排序

事件排序规则：

```text
time -> phase -> priority -> sequence
```

含义：

```text
time
  +-- 仿真时间早的先执行

phase
  +-- 同一时间点内，Input / Compute / Arbitrate / Update / Trace 有固定顺序

priority
  +-- 同一 phase 内，允许少量人为优先级

sequence
  +-- 最后兜底，保证同样输入下顺序稳定
```

为什么确定性重要：

```text
性能仿真需要可复现
同一个 workload 跑两次应该得到一样的统计结果
否则调试性能问题会非常困难
```

## 5. Phase

`Phase` 解决同一时间点内的顺序问题。

建议第一版：

```text
Input
  +-- 读取外部可见状态

Compute
  +-- 计算本地决策

Arbitrate
  +-- 解决资源竞争

Update
  +-- 提交状态变化

Trace
  +-- 记录最终状态和统计
```

例子：

```text
同一个 cycle:
  +-- pipeline 先读取 scoreboard
  +-- issue queue 再做选择
  +-- 资源仲裁决定谁发射
  +-- 最后更新状态
```

如果没有 Phase，很多组件会依赖隐式调用顺序，代码会变脆。

## 6. EventQueue

`EventQueue` 保存所有未执行的 Event。

```text
EventQueue
  +-- push(Event)
  +-- popNext()
  +-- peekNextTime()
  +-- empty()
```

第一版可以用 priority queue。

它只关心一件事：

```text
每次 pop 出当前最应该执行的 Event
```

取消事件建议第一版用 lazy cancel：

```text
cancel(id)
  +-- 找到事件状态
  +-- 标记 cancelled = true
  +-- 不立即从 priority queue 删除

popNext()
  +-- 如果 event.cancelled
        +-- 跳过
```

这样实现简单，也避免从堆中间删除的复杂度。

## 7. Scheduler

`Scheduler` 是组件和 Time Engine 之间的边界。

组件只应该调用：

```text
scheduleAt
scheduleAfter
scheduleCycles
cancel
```

组件不应该直接操作 EventQueue。

原因：

```text
EventId 应该统一生成
sequence 应该统一生成
不能向过去调度事件的检查应该统一做
trace hook 应该统一触发
并发版本中 local / remote event 应该由 Scheduler 判断
```

## 8. ClockDomain

`ClockDomain` 把“几个 cycle”转换成 `SimTime`。

```text
ClockDomain
  +-- name
  +-- period
  +-- nextEdge(now)
  +-- cyclesFromNow(now, cycles)
```

例子：

```text
CPU clock period = 1000 ps
当前 now = 2300 ps
nextEdge(now) = 3000 ps
scheduleCycles(cpuClock, 3)
  -> 3000 + 3 * 1000
  -> 6000 ps
```

为什么要对齐到 clock edge：

```text
cycle-level 模块通常在 clock edge 上观察和更新状态
如果当前事件发生在非边界时间，直接加 cycles 会产生不清晰的语义
```

## 9. 概念关系图

```text
Component
  +-- 调用 Scheduler
        +-- 创建 Event
              +-- 使用 SimTime
              +-- 使用 Phase
              +-- 写入 EventQueue
                    +-- 按 time / phase / priority / sequence 排序
                          +-- TimeEngine 执行 callback

ClockDomain
  +-- 被 Scheduler 使用
  +-- 把 cycles 转成 SimTime
```
