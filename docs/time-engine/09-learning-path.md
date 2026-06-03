# 09. 学习路径

## 1. 先建立心智模型

先理解三个边界：

```text
Time Engine
  +-- 管时间和事件顺序

Component
  +-- 管硬件行为

TimedResource
  +-- 管资源竞争
```

再理解并发边界：

```text
Partition
  +-- 管一组组件和本地事件队列

Mailbox
  +-- 管跨 Partition 事件
```

## 2. 推荐学习顺序

```text
第一步: 读 01-overview
  +-- 明白 Time Engine 是什么，不是什么

第二步: 读 02-core-concepts
  +-- 明白 SimTime / Event / Phase / Scheduler

第三步: 读 03-execution-flow
  +-- 明白事件如何驱动仿真时间

第四步: 读 04-component-model
  +-- 明白硬件模块如何接入

第五步: 读 05-resource-model
  +-- 明白端口、队列、bank 如何表达竞争

第六步: 读 06-parallel-simulation
  +-- 明白为什么要 Partition，而不是全局队列加锁

第七步: 读 07-api-sketch
  +-- 看第一版接口长什么样

第八步: 读 08-roadmap
  +-- 明白实现阶段和验收标准
```

## 3. 推荐实现顺序

```text
1. EventQueue
   +-- 先把排序做对

2. TimeEngine::runUntil
   +-- 让仿真时间能前进

3. scheduleAt / scheduleAfter
   +-- 让组件能创建未来动作

4. cancel
   +-- 支持 flush / reset / speculation

5. Phase
   +-- 解决同一时间点内的顺序

6. ClockDomain / scheduleCycles
   +-- 支持 cycle-level 表达

7. Component 示例
   +-- 写 Core / Cache 的最小交互

8. TimedResource 示例
   +-- 写一个 CachePort

9. Mini Memory System
   +-- 串起 Core -> L1 -> L2 -> Memory

10. Partition
    +-- 先单线程验证分区

11. Mailbox / Remote Event
    +-- 支持跨 Partition 通信

12. Epoch Barrier
    +-- 引入多线程执行
```

## 4. 判断自己是否理解了

你应该能回答：

```text
为什么 Time Engine 不应该知道 cache miss？
为什么 Event 需要 sequence？
为什么同一时间点需要 Phase？
为什么 Component 不应该直接操作 EventQueue？
为什么资源竞争应该放在 TimedResource？
为什么并发版本不推荐多个线程抢全局 EventQueue？
为什么要先做单线程多 Partition，再做多线程？
```
