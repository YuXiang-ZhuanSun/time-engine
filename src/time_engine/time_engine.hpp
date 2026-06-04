#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <queue>
#include <string>
#include <unordered_map>
#include <vector>

namespace ca::sim {

// 全局仿真时间。当前约定 1 个单位表示 1 ps。
// 所有 clock domain 的 cycle 延迟，最终都会转换成 SimTime 后进入事件队列。
using SimTime = std::uint64_t;

// 一段时间长度，也按 ps 理解。
// scheduleAfter(200, ...) 表示从当前 now() 开始 200 ps 后执行。
using Duration = std::uint64_t;

// 事件唯一编号，用于取消未来事件，也用于 trace/debug 输出。
using EventId = std::uint64_t;

// 事件到达目标时间后真正执行的动作。
// 芯片模型通常在 callback 里更新本组件状态、调用下游组件，或安排新的未来事件。
using Callback = std::function<void()>;

// Phase 负责给同一个 SimTime 内的事件排序。
// MVP 阶段，模型开发者通常只需要：
//   Phase::Input  - 外部 workload/driver 注入初始请求
//   Phase::Update - 完成事件、状态更新、wakeup、下游调用
enum class Phase : std::uint8_t {
    Input = 0,
    Compute = 1,
    Arbitrate = 2,
    Update = 3,
    Trace = 4,
};

const char* phaseName(Phase phase);

// 一个时钟域，例如 CPU、NoC 或 DRAM。
// TimeEngine 内部只存 SimTime；ClockDomain 负责把 cycle 延迟转换成全局 ps 时间点。
class ClockDomain {
public:
    // period 是该时钟域一个 cycle 的长度，单位是 ps。
    // 例如 ClockDomain("cpu", 1000) 表示 CPU 一个 cycle 是 1000 ps。
    ClockDomain(std::string name, SimTime period);

    const std::string& name() const;
    SimTime period() const;

    // 返回 >= now 的下一个 clock edge。
    // 如果 now 已经在 edge 上，则直接返回 now。
    SimTime nextEdge(SimTime now) const;

    // 把“从 now 开始的 cycles 个周期后”转换成 SimTime。
    // 语义是 nextEdge(now) + cycles * period，而不是简单的 now + cycles * period。
    SimTime cyclesFromNow(SimTime now, std::uint64_t cycles) const;

private:
    std::string name_;
    SimTime period_;
};

// 暴露给 TraceSink 的只读事件视图。
// 它故意不包含 callback，避免 trace 代码拿到并执行或修改事件行为。
struct EventView {
    EventId id;
    SimTime time;
    Phase phase;
    int priority;
    std::uint64_t sequence;
    std::string label;
};

// 可选观测接口。
// TraceSink 可以观察事件被安排、取消和执行，但不应该影响仿真顺序。
class TraceSink {
public:
    virtual ~TraceSink() = default;
    virtual void onScheduled(const EventView&) {}
    virtual void onCancelled(EventId) {}
    virtual void onExecuted(const EventView&) {}
};

// 单线程离散事件时序内核。
// 它维护当前仿真时间、保存未来事件，并按确定性顺序执行 callback。
// 硬件模型只应该通过这个 API 调度事件，不应该直接操作 EventQueue。
class TimeEngine {
public:
    // trace 可以为空；为空时不记录 scheduled/cancelled/executed 事件。
    explicit TimeEngine(TraceSink* trace = nullptr);

    // 当前仿真时间。只有执行事件时，now 才会跳到事件发生的时间。
    SimTime now() const;

    // 当前是否没有任何待执行的未来事件。
    bool empty() const;

    // 在绝对 SimTime 上安排一个事件。
    // time 不能早于 now()；否则抛出 std::invalid_argument。
    // 同一 time 和 phase 内，priority 数值越小越先执行。
    // label 只用于 trace/debug，不参与事件排序。
    EventId scheduleAt(
        SimTime time,
        Phase phase,
        Callback callback,
        int priority = 0,
        std::string label = {});

    // 在 now() + delay 上安排一个事件。
    // 适合已经用 ps 表达的固定延迟。
    EventId scheduleAfter(
        Duration delay,
        Phase phase,
        Callback callback,
        int priority = 0,
        std::string label = {});

    // 在指定 clock domain 的若干 cycle 后安排事件。
    // 适合 cache hit latency、pipeline stage latency 等 cycle-level 延迟。
    EventId scheduleCycles(
        const ClockDomain& clock,
        std::uint64_t cycles,
        Phase phase,
        Callback callback,
        int priority = 0,
        std::string label = {});

    // 取消一个尚未执行的事件。
    // 如果事件不存在、已经执行或已经取消，返回 false。
    bool cancel(EventId id);

    // 执行事件，直到没有未来事件。
    void run();

    // 执行所有 time <= endTime 的事件。
    // 更晚的事件会留在队列里，之后可以继续 run() 或 runUntil()。
    void runUntil(SimTime endTime);

private:
    // 内部可执行事件。EventView 是暴露给 trace 的只读投影。
    struct Event {
        EventId id;
        SimTime time;
        Phase phase;
        int priority;
        std::uint64_t sequence;
        Callback callback;
        bool cancelled = false;
        std::string label;
    };

    // std::priority_queue 默认把“最大”的元素放在 top。
    // 这个比较器定义“谁更晚”，从而让 top 变成最早应该执行的事件。
    struct EventLater {
        bool operator()(const std::shared_ptr<Event>& lhs, const std::shared_ptr<Event>& rhs) const;
    };

    // 从内部 Event 构造只读 trace 视图。
    static EventView viewOf(const Event& event);

    // lazy cancel 的清理点：取消事件到达堆顶时才跳过。
    void skipCancelled();

    SimTime now_ = 0;
    EventId nextId_ = 1;
    std::uint64_t nextSequence_ = 1;
    TraceSink* trace_ = nullptr;

    // 未来事件队列。排序规则由 EventLater 定义：
    // time -> phase -> priority -> sequence.
    std::priority_queue<std::shared_ptr<Event>, std::vector<std::shared_ptr<Event>>, EventLater> queue_;

    // 尚未执行且尚未取消的事件索引，用于 cancel() 快速查找。
    std::unordered_map<EventId, std::shared_ptr<Event>> pending_;
};

} // namespace ca::sim
