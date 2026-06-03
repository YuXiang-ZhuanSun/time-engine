# 05. 演进路线

## 演进原则

在扩展代码前，先阅读 [06-user-manual.md](06-user-manual.md)。说明书定义了用户写组件、请求对象、资源模型、trace 和 stats 的推荐方式。

```text
先让小例子跑准，再扩大模型。
先把边界讲清楚，再抽象接口。
先保护单线程确定性，再考虑并行。
```

## Step 1: 把 demo 变成组件类

当前 demo 使用 lambda 串联流程。下一步可以把它拆成薄组件：

```text
Core
  +-- issueLoad()
  +-- wakeup()

L1Cache
  +-- recvLoad()

L2Cache
  +-- recvMiss()

Memory
  +-- recvRequest()
  +-- TimedResource dramPort
```

完成标准：

```text
输出与当前 demo 时间线一致
每个组件只持有必要的下游引用
TimeEngine 仍然不知道 Core / Cache / DRAM 是什么
```

## Step 2: 增加两个请求，展示资源排队

目标是让 `TimedResource` 的 queue stats 真正起作用。

示例：

```text
load A reaches DRAM at 12000 ps
load B reaches DRAM at 13000 ps
DRAM service time = 50000 ps

load A completes at 62000 ps
load B waits until 62000 ps 才开始 service
load B completes at 112000 ps
```

完成标准：

```text
DRAM queued requests > 0
DRAM total wait > 0
两个 load latency 不同
trace 能解释为什么第二个更慢
```

## Step 3: 引入 Request / Response

建议增加：

```cpp
struct MemoryRequest {
    std::uint64_t id;
    std::uint64_t address;
    SimTime issueTime;
};
```

完成标准：

```text
trace 能打印 request id
stats 能按 request id 计算 latency
组件之间传递 request，而不是散落的局部变量
```

## Step 4: 加简单 cache hit / miss 策略

可以先用固定地址集合：

```text
部分地址 L1 hit
部分地址 L1 miss / L2 hit
部分地址 L2 miss / DRAM
```

完成标准：

```text
L1 hit 请求最快
L2 hit 请求中等
DRAM 请求最慢
```

## Step 5: 再考虑 partition 和并行

并行不要太早做。

第一步可以先做单线程多 partition：

```text
Partition 0: Core + L1
Partition 1: L2
Partition 2: Memory
```

先把 remote event 语义讲清楚，再引入 worker thread。
