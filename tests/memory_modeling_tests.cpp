#include "modeling/simple_cache.hpp"
#include "modeling/timed_resource.hpp"
#include "time_engine/time_engine.hpp"

#include <stdexcept>

using namespace ca::sim;

namespace {

void expect(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

void test_simple_cache_uses_cache_line_address() {
    SimpleCache cache("l1", 64);

    expect(!cache.contains(0x1000), "cache should start empty");
    cache.fill(0x1000);

    expect(cache.contains(0x1000), "filled address should hit");
    expect(cache.contains(0x1008), "same cache line should hit");
    expect(!cache.contains(0x1040), "different cache line should miss");
    expect(cache.lines() == 1, "same line should be stored once");
}

void test_timed_resource_serializes_requests() {
    TimeEngine engine;
    TimedResource dramPort("dram-port", 50);

    SimTime firstDone = 0;
    SimTime secondDone = 0;

    engine.scheduleAt(0, Phase::Input, [&] {
        dramPort.request(engine, Phase::Update, [&] { firstDone = engine.now(); }, "dram-first");
        dramPort.request(engine, Phase::Update, [&] { secondDone = engine.now(); }, "dram-second");
    });

    engine.run();

    expect(firstDone == 50, "first DRAM request should finish after one service time");
    expect(secondDone == 100, "second DRAM request should wait for bandwidth");
    expect(dramPort.requests() == 2, "DRAM should record two requests");
    expect(dramPort.queuedRequests() == 1, "second DRAM request should be queued");
    expect(dramPort.totalWait() == 50, "queued request should wait one service time");
}

} // namespace

int main() {
    test_simple_cache_uses_cache_line_address();
    test_timed_resource_serializes_requests();
    return 0;
}
