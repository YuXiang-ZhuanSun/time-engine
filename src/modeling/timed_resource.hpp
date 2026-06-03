#pragma once

#include "time_engine/time_engine.hpp"

#include <algorithm>
#include <string>
#include <utility>

namespace ca::sim {

class TimedResource {
public:
    TimedResource(std::string name, Duration serviceTime)
        : name_(std::move(name)), serviceTime_(serviceTime) {}

    const std::string& name() const {
        return name_;
    }

    SimTime busyUntil() const {
        return busyUntil_;
    }

    std::uint64_t requests() const {
        return requests_;
    }

    std::uint64_t queuedRequests() const {
        return queuedRequests_;
    }

    SimTime totalWait() const {
        return totalWait_;
    }

    EventId request(TimeEngine& engine, Phase phase, Callback onComplete, std::string label = {}) {
        const SimTime start = std::max(engine.now(), busyUntil_);
        const SimTime wait = start - engine.now();
        const SimTime completion = start + serviceTime_;

        ++requests_;
        if (wait > 0) {
            ++queuedRequests_;
            totalWait_ += wait;
        }
        busyUntil_ = completion;

        return engine.scheduleAt(completion, phase, std::move(onComplete), 0, std::move(label));
    }

private:
    std::string name_;
    Duration serviceTime_;
    SimTime busyUntil_ = 0;
    std::uint64_t requests_ = 0;
    std::uint64_t queuedRequests_ = 0;
    SimTime totalWait_ = 0;
};

} // namespace ca::sim

