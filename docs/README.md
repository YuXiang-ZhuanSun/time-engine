# 文档入口

这套文档介绍 CA Chip Simulation Engine 的当前方案。目标是让你能从问题背景开始，一步步读懂：

```text
为什么需要 Time Engine
Time Engine 负责什么
硬件模型负责什么
一次仿真如何推进
当前 MVP 已经实现了什么
后续应该如何扩展
```

## 推荐阅读顺序

```text
01-overview
  +-- 项目定位、目标、非目标

02-architecture
  +-- 分层架构、职责边界、目录关系

03-simulation-flow
  +-- 一次 load 请求如何沿着时间线执行

04-current-mvp
  +-- 当前代码、API、测试和 demo 对照

05-user-manual
  +-- 面向使用者的模型开发说明书

06-roadmap
  +-- 后续演进路线
```

## 当前代码结构

```text
src/
  +-- time_engine/
  |     +-- TimeEngine 内核
  |
  +-- modeling/
        +-- TimedResource 等建模辅助

examples/
  +-- mini_memory_system.cpp

tests/
  +-- time_engine_tests.cpp

docs/
  +-- 01-overview.md
  +-- 02-architecture.md
  +-- 03-simulation-flow.md
  +-- 04-current-mvp.md
  +-- 05-user-manual.md
  +-- 06-roadmap.md
```
