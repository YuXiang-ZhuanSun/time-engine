#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <queue>
#include <string>
#include <unordered_map>
#include <vector>

namespace ca::sim {

using SimTime = std::uint64_t;
using Duration = std::uint64_t;
using EventId = std::uint64_t;
using Callback = std::function<void()>;

enum class Phase : std::uint8_t {
    Input = 0,
    Compute = 1,
    Arbitrate = 2,
    Update = 3,
    Trace = 4,
};

const char* phaseName(Phase phase);

class ClockDomain {
public:
    ClockDomain(std::string name, SimTime period);

    const std::string& name() const;
    SimTime period() const;
    SimTime nextEdge(SimTime now) const;
    SimTime cyclesFromNow(SimTime now, std::uint64_t cycles) const;

private:
    std::string name_;
    SimTime period_;
};

struct EventView {
    EventId id;
    SimTime time;
    Phase phase;
    int priority;
    std::uint64_t sequence;
    std::string label;
};

class TraceSink {
public:
    virtual ~TraceSink() = default;
    virtual void onScheduled(const EventView&) {}
    virtual void onCancelled(EventId) {}
    virtual void onExecuted(const EventView&) {}
};

class TimeEngine {
public:
    explicit TimeEngine(TraceSink* trace = nullptr);

    SimTime now() const;
    bool empty() const;

    EventId scheduleAt(
        SimTime time,
        Phase phase,
        Callback callback,
        int priority = 0,
        std::string label = {});

    EventId scheduleAfter(
        Duration delay,
        Phase phase,
        Callback callback,
        int priority = 0,
        std::string label = {});

    EventId scheduleCycles(
        const ClockDomain& clock,
        std::uint64_t cycles,
        Phase phase,
        Callback callback,
        int priority = 0,
        std::string label = {});

    bool cancel(EventId id);

    void run();
    void runUntil(SimTime endTime);

private:
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

    struct EventLater {
        bool operator()(const std::shared_ptr<Event>& lhs, const std::shared_ptr<Event>& rhs) const;
    };

    static EventView viewOf(const Event& event);
    void skipCancelled();

    SimTime now_ = 0;
    EventId nextId_ = 1;
    std::uint64_t nextSequence_ = 1;
    TraceSink* trace_ = nullptr;
    std::priority_queue<std::shared_ptr<Event>, std::vector<std::shared_ptr<Event>>, EventLater> queue_;
    std::unordered_map<EventId, std::shared_ptr<Event>> pending_;
};

} // namespace ca::sim

