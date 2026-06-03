#include "time_engine/time_engine.hpp"

#include <cassert>
#include <stdexcept>
#include <string>
#include <vector>

using namespace ca::sim;

namespace {

void expect(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

void test_events_run_in_deterministic_order() {
    TimeEngine engine;
    std::vector<std::string> order;

    engine.scheduleAt(10, Phase::Update, [&] { order.push_back("sequence-1"); });
    engine.scheduleAt(5, Phase::Update, [&] { order.push_back("time-first"); });
    engine.scheduleAt(10, Phase::Input, [&] { order.push_back("phase-first"); });
    engine.scheduleAt(10, Phase::Update, [&] { order.push_back("priority-first"); }, -1);
    engine.scheduleAt(10, Phase::Update, [&] { order.push_back("sequence-2"); });

    engine.run();

    const std::vector<std::string> expected = {
        "time-first",
        "phase-first",
        "priority-first",
        "sequence-1",
        "sequence-2",
    };
    expect(order == expected, "events did not run in deterministic order");
}

void test_cancelled_event_does_not_run() {
    TimeEngine engine;
    std::vector<std::string> order;

    const EventId id = engine.scheduleAt(10, Phase::Update, [&] { order.push_back("cancelled"); });
    expect(engine.cancel(id), "expected cancel to return true for pending event");
    engine.scheduleAt(10, Phase::Update, [&] { order.push_back("kept"); });

    engine.run();

    expect(order.size() == 1 && order[0] == "kept", "cancelled event executed");
    expect(!engine.cancel(id), "executed or cancelled event should not be cancellable again");
}

void test_schedule_after_and_schedule_cycles() {
    TimeEngine engine;
    ClockDomain cpu("cpu", 1000);
    std::vector<SimTime> times;

    engine.scheduleAt(250, Phase::Update, [&] {
        times.push_back(engine.now());
        engine.scheduleAfter(125, Phase::Update, [&] { times.push_back(engine.now()); });
        engine.scheduleCycles(cpu, 3, Phase::Update, [&] { times.push_back(engine.now()); });
    });

    engine.run();

    const std::vector<SimTime> expected = {250, 375, 4000};
    expect(times == expected, "scheduleAfter or scheduleCycles produced wrong time");
}

void test_schedule_in_past_is_rejected() {
    TimeEngine engine;
    bool rejected = false;

    engine.scheduleAt(100, Phase::Update, [&] {
        try {
            engine.scheduleAt(99, Phase::Update, [] {});
        } catch (const std::invalid_argument&) {
            rejected = true;
        }
    });

    engine.run();
    expect(rejected, "scheduleAt allowed an event in the past");
}

void test_run_until_leaves_future_event_pending() {
    TimeEngine engine;
    std::vector<std::string> order;

    engine.scheduleAt(10, Phase::Update, [&] { order.push_back("first"); });
    engine.scheduleAt(20, Phase::Update, [&] { order.push_back("second"); });

    engine.runUntil(10);
    expect(order.size() == 1 && order[0] == "first", "runUntil executed beyond end time");
    expect(engine.now() == 10, "runUntil should leave now at last executed event time");

    engine.run();
    expect(order.size() == 2 && order[1] == "second", "future event was not preserved");
}

} // namespace

int main() {
    test_events_run_in_deterministic_order();
    test_cancelled_event_does_not_run();
    test_schedule_after_and_schedule_cycles();
    test_schedule_in_past_is_rejected();
    test_run_until_leaves_future_event_pending();
    return 0;
}

