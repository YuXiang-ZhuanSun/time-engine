# 03. 执行语义

## 1. Time Engine 怎么让时间前进

Time Engine 不像真实时间那样一秒一秒流动。

它的时间推进方式是：

```text
直接跳到下一个 Event 的时间
```

如果当前时间是 `1000 ps`，下一个事件在 `5000 ps`，中间没有任何事件，那么仿真时间可以直接跳到 `5000 ps`。

这就是事件驱动仿真的性能优势。

## 2. 主循环

语义上的主循环：

```text
runUntil(endTime)
  +-- while EventQueue 不为空
        +-- event = popNext()

        +-- 如果 event.cancelled
              +-- 跳过

        +-- 如果 event.time > endTime
              +-- 放回或保留
              +-- 停止

        +-- now = event.time
        +-- currentPhase = event.phase
        +-- 执行 event.callback
```

这里最重要的点：

```text
TimeEngine 只推进到事件发生的时间
callback 执行时可以继续 schedule 新事件
新事件进入 EventQueue，等待后续执行
```

## 3. 调度闭环

```text
Component 收到请求
  -> 根据自己的硬件策略决定延迟
    -> 调用 Scheduler
      -> Scheduler 创建 Event
        -> EventQueue 保存 Event
          -> TimeEngine 在未来执行 Event
            -> callback 回到 Component
```

例子：

```text
Core 发起 load
  -> L1 Cache 判断 miss
    -> scheduleAfter(l1MissDetectLatency, sendToL2)
      -> TimeEngine 未来执行 sendToL2
```

Time Engine 不知道什么是 load，也不知道什么是 miss。它只执行未来动作。

## 4. 同一时间点如何处理

如果多个事件发生在同一 `SimTime`，执行顺序由：

```text
phase -> priority -> sequence
```

决定。

例子：

```text
T=1000 ps:
  Input:
    +-- 读取输入状态

  Compute:
    +-- 计算本地决策

  Arbitrate:
    +-- 端口仲裁

  Update:
    +-- 更新状态

  Trace:
    +-- 记录统计
```

这个顺序让模型更容易写出“同周期一致”的行为。

## 5. 事件取消

取消事件用于表达：

```text
pipeline flush
branch misprediction
speculative request 被撤销
reset
timeout 被提前满足
```

第一版建议：

```text
cancel(eventId)
  -> 标记事件取消
  -> pop 时跳过
```

不要一开始就做复杂的堆内删除。

## 6. 错误语义

必须明确哪些行为非法。

第一版建议：

```text
不能向过去 schedule
  +-- event.time < now 是硬错误

ClockDomain.period 不能为 0
  +-- 构造时检查

Event callback 中可以 schedule 新事件
  +-- 但新事件时间不能早于 now

cancel 未知 EventId
  +-- 可以选择返回 false，或 debug 模式报错
```

## 7. 和并发版本的关系

单线程执行语义是并发版本的基准。

并发版本必须满足：

```text
同样输入下
单线程执行结果
  == 多线程执行结果
```

所以单线程 Time Engine 不只是第一阶段实现，也是后续并发正确性的参考模型。
