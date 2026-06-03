#include "modeling/timed_resource.hpp"
#include "time_engine/time_engine.hpp"

#include <iostream>
#include <string>
#include <vector>

using namespace ca::sim;

namespace {

class PrintingTrace : public TraceSink {
public:
    void onScheduled(const EventView& event) override {
        lines.push_back("schedule T=" + std::to_string(event.time) + " phase=" + phaseName(event.phase) +
                        " label=" + event.label);
    }

    void onExecuted(const EventView& event) override {
        lines.push_back("execute  T=" + std::to_string(event.time) + " phase=" + phaseName(event.phase) +
                        " label=" + event.label);
    }

    std::vector<std::string> lines;
};

} // namespace

int main() {
    PrintingTrace trace;
    TimeEngine engine(&trace);
    ClockDomain cpu("cpu", 1000);
    TimedResource dramPort("dram-port", 50000);

    const SimTime issueTime = engine.now();
    SimTime wakeupTime = 0;

    auto wakeCore = [&] {
        wakeupTime = engine.now();
        std::cout << "Core wakeup at " << wakeupTime << " ps\n";
    };

    auto accessDram = [&] {
        std::cout << "DRAM accepts miss at " << engine.now() << " ps\n";
        dramPort.request(engine, Phase::Update, wakeCore, "dram-complete");
    };

    auto accessL2 = [&] {
        std::cout << "L2 miss at " << engine.now() << " ps\n";
        engine.scheduleCycles(cpu, 8, Phase::Update, accessDram, 0, "send-to-dram");
    };

    auto accessL1 = [&] {
        std::cout << "L1 miss at " << engine.now() << " ps\n";
        engine.scheduleCycles(cpu, 4, Phase::Update, accessL2, 0, "send-to-l2");
    };

    std::cout << "Core issues load at " << issueTime << " ps\n";
    engine.scheduleAt(issueTime, Phase::Input, accessL1, 0, "core-load");
    engine.run();

    std::cout << "\nEvent trace:\n";
    for (const auto& line : trace.lines) {
        std::cout << "  " << line << "\n";
    }

    std::cout << "\nStats:\n";
    std::cout << "  load latency: " << (wakeupTime - issueTime) << " ps\n";
    std::cout << "  DRAM requests: " << dramPort.requests() << "\n";
    std::cout << "  DRAM queued requests: " << dramPort.queuedRequests() << "\n";
    std::cout << "  DRAM total wait: " << dramPort.totalWait() << " ps\n";

    return 0;
}

