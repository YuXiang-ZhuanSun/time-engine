# CA Chip Simulation Engine

CA Chip Simulation Engine 是一个精简的芯片性能仿真时序框架。

当前版本先实现单线程离散事件 Time Engine，用一个 mini memory system demo 跑通：

```text
Core -> L1 -> L2 -> DRAM -> Core wakeup
```

## 快速开始

```powershell
cmake -S . -B build
cmake --build build
ctest --test-dir build -C Debug --output-on-failure
.\build\Debug\mini_memory_system.exe
```

## 文档入口

从 [docs/README.md](docs/README.md) 开始阅读。

推荐顺序：

```text
docs/01-overview.md
  -> docs/02-architecture.md
  -> docs/03-simulation-flow.md
  -> docs/04-current-mvp.md
  -> docs/06-user-manual.md
  -> docs/05-roadmap.md
```
