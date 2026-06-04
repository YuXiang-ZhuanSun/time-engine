#include "modeling/simple_cache.hpp"
#include "modeling/timed_resource.hpp"
#include "time_engine/time_engine.hpp"

#include <cstdint>
#include <iostream>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

using namespace ca::sim;

namespace {

std::string hexAddress(std::uint64_t address) {
    std::ostringstream out;
    out << "0x" << std::hex << address;
    return out.str();
}

// MemoryRequest 表示一条从 Core 发出的内存请求。
// 这里把 id、address、issueTime 放在一起，是为了 trace 和 stats 能追踪同一条请求。
struct MemoryRequest {
    std::uint64_t id;
    std::uint64_t address;
    SimTime issueTime;
};

struct CompletedLoad {
    std::uint64_t id;
    std::uint64_t address;
    SimTime issueTime;
    SimTime doneTime;
    std::string path;
};

// MemoryHierarchy 是这个 demo 的核心模型。
//
// 它不是通用 memory simulator，而是一个最小但完整的教学模型：
//   Core -> L1 -> L2 -> DRAM port -> Core
//
// L1/L2 用 SimpleCache 判断 hit/miss。
// DRAM port 用 TimedResource 表达带宽：同一时间只能服务一个 cache line。
class MemoryHierarchy {
public:
    MemoryHierarchy(TimeEngine& engine, ClockDomain& cpu)
        : engine_(engine),
          cpu_(cpu),
          l1_("l1", 64),
          l2_("l2", 64),
          dramPort_("dram-port", 50000) {}

    void prefillL2(std::uint64_t address) {
        l2_.fill(address);
    }

    void issueLoad(MemoryRequest request) {
        std::cout << "Core issues load #" << request.id << " addr=" << hexAddress(request.address)
                  << " at " << engine_.now() << " ps\n";

        // L1 tag/data 访问需要 4 个 CPU cycle。
        // 4 个 cycle 后，模型才知道这次访问是 hit 还是 miss。
        engine_.scheduleCycles(cpu_, 4, Phase::Update, [this, request] {
            accessL1(request);
        }, 0, "l1-access");
    }

    const SimpleCache& l1() const {
        return l1_;
    }

    const SimpleCache& l2() const {
        return l2_;
    }

    const TimedResource& dramPort() const {
        return dramPort_;
    }

    const std::vector<CompletedLoad>& completedLoads() const {
        return completed_;
    }

private:
    void accessL1(MemoryRequest request) {
        if (l1_.access(request.address)) {
            complete(request, "L1 hit");
            return;
        }

        std::cout << "  L1 miss for #" << request.id << " at " << engine_.now() << " ps\n";

        // L1 miss 后，把请求送到 L2。这里把 L2 访问延迟建模为 12 个 CPU cycle。
        engine_.scheduleCycles(cpu_, 12, Phase::Update, [this, request] {
            accessL2(request);
        }, 0, "l2-access");
    }

    void accessL2(MemoryRequest request) {
        if (l2_.access(request.address)) {
            std::cout << "  L2 hit for #" << request.id << " at " << engine_.now() << " ps\n";
            l1_.fill(request.address);
            complete(request, "L1 miss -> L2 hit");
            return;
        }

        std::cout << "  L2 miss for #" << request.id << " at " << engine_.now() << " ps\n";

        // L2 miss 后进入 DRAM。DRAM 的完成时间由 dramPort_ 决定：
        // 如果 port 空闲，请求立刻开始 service；如果 port 忙，请求排队。
        dramPort_.request(engine_, Phase::Update, [this, request] {
            completeFromDram(request);
        }, "dram-complete");
    }

    void completeFromDram(MemoryRequest request) {
        l2_.fill(request.address);
        l1_.fill(request.address);
        complete(request, "L1 miss -> L2 miss -> DRAM");
    }

    void complete(MemoryRequest request, std::string path) {
        const SimTime done = engine_.now();
        completed_.push_back(CompletedLoad{request.id, request.address, request.issueTime, done, std::move(path)});

        std::cout << "Core receives load #" << request.id << " at " << done << " ps"
                  << " latency=" << (done - request.issueTime) << " ps"
                  << " path=" << completed_.back().path << "\n";
    }

    TimeEngine& engine_;
    ClockDomain& cpu_;
    SimpleCache l1_;
    SimpleCache l2_;
    TimedResource dramPort_;
    std::vector<CompletedLoad> completed_;
};

} // namespace

int main() {
    TimeEngine engine;
    ClockDomain cpu("cpu", 1000);
    MemoryHierarchy memory(engine, cpu);

    // 预先让 L2 拥有 0x4000 这条 cache line，用来演示“L1 miss 但 L2 hit”。
    memory.prefillL2(0x4000);

    // 请求 1：第一次访问 0x1000，L1/L2 都没有，最终走 DRAM。
    engine.scheduleAt(0, Phase::Input, [&] {
        memory.issueLoad(MemoryRequest{1, 0x1000, engine.now()});
    }, 0, "core-load-1");

    // 请求 2：0x1008 和 0x1000 在同一个 64B cache line。
    // 请求 1 回填 L1 后，请求 2 会成为 L1 hit。
    engine.scheduleAt(70000, Phase::Input, [&] {
        memory.issueLoad(MemoryRequest{2, 0x1008, engine.now()});
    }, 0, "core-load-2");

    // 请求 3：L1 没有，但 L2 预先有 0x4000，所以会演示 L2 hit。
    engine.scheduleAt(80000, Phase::Input, [&] {
        memory.issueLoad(MemoryRequest{3, 0x4000, engine.now()});
    }, 0, "core-load-3");

    // 请求 4/5：同一时间发两个不同 cache line 的 miss。
    // 它们都会到 DRAM，第二个会因为 dramPort 带宽有限而排队。
    engine.scheduleAt(100000, Phase::Input, [&] {
        memory.issueLoad(MemoryRequest{4, 0x8000, engine.now()});
        memory.issueLoad(MemoryRequest{5, 0x9000, engine.now()});
    }, 0, "core-load-4-5");

    engine.run();

    std::cout << "\nCompleted loads:\n";
    for (const auto& load : memory.completedLoads()) {
        std::cout << "  #" << load.id << " addr=" << hexAddress(load.address)
                  << " latency=" << (load.doneTime - load.issueTime)
                  << " ps path=" << load.path << "\n";
    }

    std::cout << "\nCache stats:\n";
    std::cout << "  L1 accesses=" << memory.l1().accesses()
              << " hits=" << memory.l1().hits()
              << " misses=" << memory.l1().misses()
              << " lines=" << memory.l1().lines() << "\n";
    std::cout << "  L2 accesses=" << memory.l2().accesses()
              << " hits=" << memory.l2().hits()
              << " misses=" << memory.l2().misses()
              << " lines=" << memory.l2().lines() << "\n";

    std::cout << "\nDRAM bandwidth stats:\n";
    std::cout << "  requests=" << memory.dramPort().requests() << "\n";
    std::cout << "  queued requests=" << memory.dramPort().queuedRequests() << "\n";
    std::cout << "  total wait=" << memory.dramPort().totalWait() << " ps\n";

    return 0;
}
