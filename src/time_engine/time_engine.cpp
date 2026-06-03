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
    const SimTime remainder = now % period_;
    if (remainder == 0) {
        return now;
    }
    return now + (period_ - remainder);
}

SimTime ClockDomain::cyclesFromNow(SimTime now, std::uint64_t cycles) const {
    const SimTime edge = nextEdge(now);
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

    auto event = std::make_shared<Event>();
    event->id = nextId_++;
    event->time = time;
    event->phase = phase;
    event->priority = priority;
    event->sequence = nextSequence_++;
    event->callback = std::move(callback);
    event->label = std::move(label);

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
        skipCancelled();
        if (queue_.empty()) {
            return;
        }

        const auto event = queue_.top();
        if (event->time > endTime) {
            return;
        }

        queue_.pop();
        pending_.erase(event->id);
        now_ = event->time;

        if (trace_) {
            trace_->onExecuted(viewOf(*event));
        }
        event->callback();
    }
}

bool TimeEngine::EventLater::operator()(
    const std::shared_ptr<Event>& lhs,
    const std::shared_ptr<Event>& rhs) const {
    if (lhs->time != rhs->time) {
        return lhs->time > rhs->time;
    }
    if (lhs->phase != rhs->phase) {
        return static_cast<std::uint8_t>(lhs->phase) > static_cast<std::uint8_t>(rhs->phase);
    }
    if (lhs->priority != rhs->priority) {
        return lhs->priority > rhs->priority;
    }
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

