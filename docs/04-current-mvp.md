# 04. 当前 MVP

## 文件结构

```text
CMakeLists.txt

src/time_engine/
  +-- time_engine.hpp
  +-- time_engine.cpp

src/modeling/
  +-- timed_resource.hpp

examples/
  +-- mini_memory_system.cpp

tests/
  +-- time_engine_tests.cpp
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
```

## 构建和运行

```powershell
cmake -S . -B build
cmake --build build
ctest --test-dir build -C Debug --output-on-failure
.\build\Debug\mini_memory_system.exe
```

## 预期 demo 输出

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

