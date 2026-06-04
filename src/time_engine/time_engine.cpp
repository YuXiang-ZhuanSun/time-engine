#include "time_engine/time_engine.hpp"

#include <limits>
#include <stdexcept>
#include <utility>

namespace ca::sim {

const char* phaseName(Phase phase) {
    switch (phase) {
    case Phase::Input:
        return "Input";
    case Phase::Compute:
        return "Compute";
    case Phase::Arbitrate:
        return "Arbitrate";
    case Phase::Update:
        return "Update";
    case Phase::Trace:
        return "Trace";
    }
    return "Unknown";
}

ClockDomain::ClockDomain(std::string name, SimTime period)
    : name_(std::move(name)), period_(period) {
    if (period_ == 0) {
        throw std::invalid_argument("ClockDomain period must be greater than zero");
    }
}

const std::string& ClockDomain::name() const {
    return name_;
}

SimTime ClockDomain::period() const {
    return period_;
}

SimTime ClockDomain::nextEdge(SimTime now) const {
    // 如果 now 已经对齐在时钟边界上，就允许事件发生在当前边界。
    // 否则向前推进到下一个边界，避免 cycle-level 模型在半个 cycle 上更新状态。
    const SimTime remainder = now % period_;
    if (remainder == 0) {
        return now;
    }
    return now + (period_ - remainder);
}

SimTime ClockDomain::cyclesFromNow(SimTime now, std::uint64_t cycles) const {
    const SimTime edge = nextEdge(now);

    // 防止 edge + cycles * period_ 超过 uint64_t。
    // 这里先做除法检查，避免乘法本身已经溢出。
    if (cycles > (std::numeric_limits<SimTime>::max() - edge) / period_) {
        throw std::overflow_error("cycle delay overflows SimTime");
    }
    return edge + cycles * period_;
}

TimeEngine::TimeEngine(TraceSink* trace) : trace_(trace) {}

SimTime TimeEngine::now() const {
    return now_;
}

bool TimeEngine::empty() const {
    return pending_.empty();
}

EventId TimeEngine::scheduleAt(
    SimTime time,
    Phase phase,
    Callback callback,
    int priority,
    std::string label) {
    if (time < now_) {
        throw std::invalid_argument("cannot schedule an event in the past");
    }
    if (!callback) {
        throw std::invalid_argument("event callback must be callable");
    }

    // EventId 用于取消和 trace；sequence 是最后的兜底排序键。
    // 即使 time/phase/priority 完全相同，sequence 也能保证执行顺序稳定。
    auto event = std::make_shared<Event>();
    event->id = nextId_++;
    event->time = time;
    event->phase = phase;
    event->priority = priority;
    event->sequence = nextSequence_++;
    event->callback = std::move(callback);
    event->label = std::move(label);

    // 同一个 Event 同时进入两个结构：
    //   queue_   负责按时间顺序执行
    //   pending_ 负责通过 EventId 找到并取消
    queue_.push(event);
    pending_[event->id] = event;

    if (trace_) {
        trace_->onScheduled(viewOf(*event));
    }
    return event->id;
}

EventId TimeEngine::scheduleAfter(
    Duration delay,
    Phase phase,
    Callback callback,
    int priority,
    std::string label) {
    if (delay > std::numeric_limits<SimTime>::max() - now_) {
        throw std::overflow_error("delay overflows SimTime");
    }
    return scheduleAt(now_ + delay, phase, std::move(callback), priority, std::move(label));
}

EventId TimeEngine::scheduleCycles(
    const ClockDomain& clock,
    std::uint64_t cycles,
    Phase phase,
    Callback callback,
    int priority,
    std::string label) {
    return scheduleAt(clock.cyclesFromNow(now_, cycles), phase, std::move(callback), priority, std::move(label));
}

bool TimeEngine::cancel(EventId id) {
    const auto it = pending_.find(id);
    if (it == pending_.end()) {
        return false;
    }

    // lazy cancel：不从 priority_queue 中间删除事件，只打取消标记。
    // priority_queue 不擅长删除堆中任意元素；延迟到 pop 前跳过更简单。
    it->second->cancelled = true;
    pending_.erase(it);
    if (trace_) {
        trace_->onCancelled(id);
    }
    return true;
}

void TimeEngine::run() {
    runUntil(std::numeric_limits<SimTime>::max());
}

void TimeEngine::runUntil(SimTime endTime) {
    while (true) {
        // 每轮先清掉堆顶已取消事件；非堆顶取消事件会在未来到达堆顶时清掉。
        skipCancelled();
        if (queue_.empty()) {
            return;
        }

        const auto event = queue_.top();
        if (event->time > endTime) {
            // 不能 pop 这个未来事件；它必须留在队列里，供下一次 run/runUntil 执行。
            return;
        }

        queue_.pop();
        pending_.erase(event->id);
        now_ = event->time;

        if (trace_) {
            trace_->onExecuted(viewOf(*event));
        }

        // callback 是硬件模型的行为入口。
        // 它可以更新模型状态，也可以继续调用 scheduleAt/scheduleAfter/scheduleCycles。
        event->callback();
    }
}

bool TimeEngine::EventLater::operator()(
    const std::shared_ptr<Event>& lhs,
    const std::shared_ptr<Event>& rhs) const {
    // std::priority_queue 会把比较器认为“最大”的元素放在 top。
    // 这里返回 true 表示 lhs 比 rhs 更晚，因此 rhs 应该排在前面。
    if (lhs->time != rhs->time) {
        return lhs->time > rhs->time;
    }
    if (lhs->phase != rhs->phase) {
        return static_cast<std::uint8_t>(lhs->phase) > static_cast<std::uint8_t>(rhs->phase);
    }
    if (lhs->priority != rhs->priority) {
        return lhs->priority > rhs->priority;
    }
    // sequence 是最终兜底。先创建的事件先执行，保证同样输入下结果可复现。
    return lhs->sequence > rhs->sequence;
}

EventView TimeEngine::viewOf(const Event& event) {
    return EventView{event.id, event.time, event.phase, event.priority, event.sequence, event.label};
}

void TimeEngine::skipCancelled() {
    while (!queue_.empty() && queue_.top()->cancelled) {
        queue_.pop();
    }
}

} // namespace ca::sim
