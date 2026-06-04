# 04. 当前 MVP

## 文件结构

```text
CMakeLists.txt

src/time_engine/
  +-- time_engine.hpp
  +-- time_engine.cpp

src/modeling/
  +-- timed_resource.hpp
  +-- simple_cache.hpp

examples/
  +-- mini_memory_system.cpp
  +-- memory_hierarchy.cpp

tests/
  +-- time_engine_tests.cpp
  +-- memory_modeling_tests.cpp
```

## TimeEngine API

当前公开 API：

```cpp
SimTime now() const;
bool empty() const;

EventId scheduleAt(SimTime time, Phase phase, Callback callback, int priority = 0, std::string label = {});
EventId scheduleAfter(Duration delay, Phase phase, Callback callback, int priority = 0, std::string label = {});
EventId scheduleCycles(const ClockDomain& clock, std::uint64_t cycles, Phase phase, Callback callback, int priority = 0, std::string label = {});

bool cancel(EventId id);

void run();
void runUntil(SimTime endTime);
```

## 当前测试覆盖

测试文件：

```text
tests/time_engine_tests.cpp
tests/memory_modeling_tests.cpp
```

覆盖语义：

```text
事件按 time 排序
同一 time 按 phase 排序
同一 phase 按 priority 排序
同条件下按 sequence 保持确定性
cancel 后事件不执行
scheduleAfter 使用 now + delay
scheduleCycles 使用 ClockDomain 转换
不能向过去 schedule
runUntil 不会执行超过 endTime 的事件
runUntil 会保留未来事件
SimpleCache 按 cache line 判断 hit/miss
TimedResource 会串行化同一个资源上的多个请求
TimedResource 会统计排队请求数和总等待时间
```

## 构建和运行

```powershell
cmake -S . -B build
cmake --build build
ctest --test-dir build -C Debug --output-on-failure
.\build\Debug\mini_memory_system.exe
.\build\Debug\memory_hierarchy.exe
```

## 当前 demo

```text
examples/mini_memory_system.cpp
  最小闭环 demo。
  固定 L1 miss、固定 L2 miss，用最少代码展示 scheduleCycles、TimedResource、trace 和 stats。

examples/memory_hierarchy.cpp
  进阶 memory demo。
  请求带真实地址，L1/L2 能按 cache line 判断 hit/miss，DRAM port 能体现带宽和排队。
```

## mini_memory_system 预期输出

```text
Core issues load at 0 ps
L1 miss at 0 ps
L2 miss at 4000 ps
DRAM accepts miss at 12000 ps
Core wakeup at 62000 ps

Stats:
  load latency: 62000 ps
  DRAM requests: 1
  DRAM queued requests: 0
  DRAM total wait: 0 ps
```

## memory_hierarchy 预期行为

```text
load #1 addr=0x1000:
  L1 miss -> L2 miss -> DRAM
  返回后回填 L2 和 L1

load #2 addr=0x1008:
  和 0x1000 属于同一个 64B cache line
  因为 #1 已经回填 L1，所以 L1 hit

load #3 addr=0x4000:
  L1 miss
  L2 预先有这条 line，所以 L2 hit

load #4 / #5:
  同一时间发出两个不同 cache line 的 miss
  两个请求都会到 DRAM
  DRAM port 一次只能服务一个请求，所以 #5 会排队
```

## MVP 的意义

当前 MVP 的价值是证明框架闭环：

```text
组件可以创建未来事件
事件可以稳定排序
仿真时间可以跳跃推进
cycle 延迟可以转换为 ps
资源可以表达占用时间
trace 可以解释因果链
stats 可以输出性能结果
测试可以保护时序语义
```
