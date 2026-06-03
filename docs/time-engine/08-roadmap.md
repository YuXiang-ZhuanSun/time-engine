# 08. 实现路线

## 1. 路线原则

不要一开始就实现所有复杂能力。

推荐原则：

```text
先做单线程正确性
再做芯片建模便利性
再用小系统验证
最后做并发仿真
```

单线程版本不是临时玩具，它是并发版本的参考模型。

## 2. Milestone 1: 单线程 Time Engine Kernel

目标：建立最小正确内核。

实现内容：

```text
SimTime / Duration
EventId
Phase
Event
EventQueue
scheduleAt
scheduleAfter
cancel
runUntil
```

测试：

```text
事件按 time 排序
同一 time 按 phase 排序
同一 phase 按 priority 排序
最后按 sequence 保持确定性
cancel 后事件不会执行
不能向过去 schedule
```

完成标准：

```text
可以用几个 callback 串起一条时间线
同样输入多次运行结果一致
```

## 3. Milestone 2: 芯片仿真辅助能力

目标：让芯片组件更容易使用 Time Engine。

实现内容：

```text
ClockDomain
scheduleCycles
Component
TickedComponent helper
Trace hook
基础 stats counter
```

完成标准：

```text
Component 可以用 cycle 表达延迟
可以写 event-driven 和 tick-driven 两种组件
可以输出事件 trace
```

## 4. Milestone 3: Mini Memory System

目标：用小系统验证设计不是纸面方案。

系统：

```text
Core
  -> L1 Cache
    -> L2 Cache
      -> Memory
```

覆盖行为：

```text
L1 hit latency
L1 miss -> L2 hit latency
L2 miss -> Memory latency
Cache port contention
Memory queueing
Core stall / wakeup
```

完成标准：

```text
能跑一个简单请求序列
能输出每次请求的 latency
能输出资源利用率和队列长度
```

## 5. Milestone 4: 单线程多 Partition

目标：为并发做结构准备，但先不引入线程不确定性。

实现内容：

```text
Partition
Component -> Partition 绑定
local EventQueue
Remote Event
Mailbox
单线程轮流推进多个 Partition
```

完成标准：

```text
local event 和 remote event 语义清楚
跨 Partition 请求能正确返回
结果和单 Partition 模型一致
```

## 6. Milestone 5: 多线程 Epoch 并发

目标：真正用多个线程提升仿真速度。

实现内容：

```text
worker thread
epoch barrier
mailbox merge
deterministic merge key
parallel runUntil
```

测试：

```text
parallel vs single-thread 结果一致
不同线程调度顺序下结果一致
remote event 不会进入过去
barrier 合并顺序稳定
```

完成标准：

```text
在多 core / 多 cache workload 下有可测性能提升
同时保持确定性
```

## 7. Milestone 6: 性能优化

目标：根据真实瓶颈优化。

可能方向：

```text
调整 Partition 划分
调整 epoch size
批量处理 Mailbox
减少 callback 分配
优化 EventQueue 数据结构
按 workload 做 trace 采样
```

不要在没有 profile 数据前过早优化。
