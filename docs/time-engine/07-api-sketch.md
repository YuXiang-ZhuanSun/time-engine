# 07. API 草案

## 1. API 设计目标

第一版 API 要服务两个目标：

```text
让 Component 能清晰表达未来动作
让 Time Engine 能保持确定性和可调试性
```

因此 API 不追求一开始就很全，而是先把核心语义做稳。

## 2. 基础类型

```cpp
using SimTime = uint64_t;
using Duration = uint64_t;
using EventId = uint64_t;
```

```cpp
enum class Phase : uint8_t {
    Input = 0,
    Compute = 1,
    Arbitrate = 2,
    Update = 3,
    Trace = 4,
};
```

## 3. TimeEngine

```cpp
class TimeEngine {
public:
    SimTime now() const;

    EventId scheduleAt(
        SimTime time,
        Phase phase,
        Callback callback,
        uint32_t priority = 0);

    EventId scheduleAfter(
        Duration delay,
        Phase phase,
        Callback callback,
        uint32_t priority = 0);

    EventId scheduleCycles(
        const ClockDomain& clock,
        uint64_t cycles,
        Phase phase,
        Callback callback,
        uint32_t priority = 0);

    bool cancel(EventId id);

    void run();
    void runUntil(SimTime endTime);
};
```

语义：

```text
scheduleAt
  +-- 在绝对时间创建 Event

scheduleAfter
  +-- 在 now + delay 创建 Event

scheduleCycles
  +-- 通过 ClockDomain 把 cycles 转成 SimTime

cancel
  +-- 取消未执行事件

runUntil
  +-- 执行到指定仿真时间
```

## 4. ClockDomain

```cpp
class ClockDomain {
public:
    ClockDomain(std::string name, SimTime period);

    const std::string& name() const;
    SimTime period() const;

    SimTime nextEdge(SimTime now) const;
    SimTime cyclesFromNow(SimTime now, uint64_t cycles) const;
};
```

建议语义：

```text
nextEdge(now)
  +-- 返回 >= now 的下一个 clock edge

cyclesFromNow(now, cycles)
  +-- nextEdge(now) + cycles * period
```

## 5. Component

```cpp
class Component {
public:
    virtual ~Component() = default;
    virtual void reset() {}
    virtual void start() {}
};
```

后续可以加入：

```cpp
class TickedComponent : public Component {
public:
    virtual void tick() = 0;
};
```

但第一版不要求所有组件都是 ticked。

## 6. Trace Hook

第一版可以预留轻量 hook：

```cpp
class EventTraceSink {
public:
    virtual void onEventScheduled(const Event& event) {}
    virtual void onEventCancelled(EventId id) {}
    virtual void onEventExecuted(const Event& event) {}
};
```

trace 不应该污染核心逻辑。关闭 trace 时开销要低。

## 7. 并发扩展 API

并发 API 不建议第一阶段实现，但可以提前规划。

```cpp
using PartitionId = uint32_t;

class ParallelTimeEngine {
public:
    PartitionId createPartition(std::string name);

    void bindComponent(
        Component& component,
        PartitionId partition);

    EventId scheduleRemote(
        PartitionId targetPartition,
        SimTime targetTime,
        Phase phase,
        Callback callback,
        uint32_t priority = 0);

    void runParallelUntil(SimTime endTime);
};
```

长期看，`scheduleRemote` 更适合传 message，而不是直接传 callback。

```text
第一版:
  +-- callback 简化实现

长期:
  +-- message 更利于 trace / replay / 分布式扩展
```
