#pragma once

#include "time_engine/time_engine.hpp"

#include <algorithm>
#include <string>
#include <utility>

namespace ca::sim {

// 最小资源占用模型：表示“一次只能服务一个请求”的资源。
// 例如单端口 cache、简单 NoC link、DRAM service slot 或 functional unit。
//
// 当前 MVP 不做复杂仲裁。请求到来时：
//   1. 如果资源空闲，从 engine.now() 开始 service
//   2. 如果资源忙，从 busyUntil_ 开始 service
//   3. 在 start + serviceTime_ 时安排完成 callback
class TimedResource {
public:
    // 每个请求占用资源的固定 service time，单位是 ps。
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

    // 请求使用该资源，并在资源 service 完成时执行 onComplete。
    // phase 决定 completion callback 在目标时间点的哪个阶段执行。
    // label 只用于 trace/debug，不影响资源调度。
    EventId request(TimeEngine& engine, Phase phase, Callback onComplete, std::string label = {}) {
        // start 是该请求真正开始占用资源的时间。
        // 如果 engine.now() 早于 busyUntil_，说明前一个请求还没完成，本请求需要排队。
        const SimTime start = std::max(engine.now(), busyUntil_);
        const SimTime wait = start - engine.now();
        const SimTime completion = start + serviceTime_;

        // 这些统计帮助用户判断资源竞争是否造成额外 latency。
        ++requests_;
        if (wait > 0) {
            ++queuedRequests_;
            totalWait_ += wait;
        }

        // 更新资源下一次可用时间，再安排 completion event。
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
