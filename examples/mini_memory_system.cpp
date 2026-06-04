#include "modeling/timed_resource.hpp"
#include "time_engine/time_engine.hpp"

#include <iostream>
#include <string>
#include <vector>

using namespace ca::sim;

namespace {

// demo 用的最小 trace sink。
// 它记录事件被安排和执行的时刻，帮助用户从输出里还原事件因果链。
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

    // demo 假设 CPU clock 是 1 GHz，也就是 1000 ps / cycle。
    // L1/L2 的延迟用 CPU cycles 表示，再由 ClockDomain 转成 SimTime。
    ClockDomain cpu("cpu", 1000);

    // DRAM port 是一个最小资源模型：一次服务一个请求，每个请求占用 50000 ps。
    TimedResource dramPort("dram-port", 50000);

    const SimTime issueTime = engine.now();
    SimTime wakeupTime = 0;

    // Core wakeup 表示这次 load 的数据已经返回。
    // 这里记录 wakeupTime，用于最后计算 load latency。
    auto wakeCore = [&] {
        wakeupTime = engine.now();
        std::cout << "Core wakeup at " << wakeupTime << " ps\n";
    };

    // L2 miss 后，请求到达 DRAM。
    // DRAM 的完成时间不由这个 lambda 直接决定，而由 TimedResource 根据 busyUntil_ 决定。
    auto accessDram = [&] {
        std::cout << "DRAM accepts miss at " << engine.now() << " ps\n";
        dramPort.request(engine, Phase::Update, wakeCore, "dram-complete");
    };

    // 请求到达 L2。当前 demo 固定 L2 miss。
    // 8 个 CPU cycle 后把请求送到 DRAM。
    auto accessL2 = [&] {
        std::cout << "L2 miss at " << engine.now() << " ps\n";
        engine.scheduleCycles(cpu, 8, Phase::Update, accessDram, 0, "send-to-dram");
    };

    // 请求到达 L1。当前 demo 固定 L1 miss。
    // 4 个 CPU cycle 后把请求送到 L2。
    auto accessL1 = [&] {
        std::cout << "L1 miss at " << engine.now() << " ps\n";
        engine.scheduleCycles(cpu, 4, Phase::Update, accessL2, 0, "send-to-l2");
    };

    // 初始事件使用 Input phase，表示 workload/driver 从外部把请求注入仿真。
    std::cout << "Core issues load at " << issueTime << " ps\n";
    engine.scheduleAt(issueTime, Phase::Input, accessL1, 0, "core-load");

    // run 会一直执行事件，直到没有未来事件。
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
