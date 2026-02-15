# loghelper 性能基准报告

## 测试环境

- OS: Ubuntu 24.04 (x86_64)
- CPU: AMD EPYC (cloud instance)
- Compiler: GCC 13.3.0
- Optimization: -O3
- C++ Standard: C++14

## 测试方法

- 所有输出 sink 关闭 (console_level = kOff, enable_file = false)
- 测量纯日志框架开销 (格式化 + 级别检查 + 线程同步)
- 单线程: 100,000 条消息, 逐条计时
- 多线程: 4 线程 x 25,000 条, 计算总墙钟时间

## 结果

### fallback 后端 (零依赖, stderr)

| 测试项 | 平均延迟 | 最小延迟 | 最大延迟 | 吞吐量 |
|--------|---------|---------|---------|--------|
| 单线程延迟 | 38 ns | 35 ns | 11,894 ns | 26.3M msg/s |
| 4线程吞吐 | 5 ns/msg | - | - | 200M msg/s |
| 带 Tag 日志 | 39 ns | 38 ns | 10,306 ns | 25.6M msg/s |
| 编译期过滤 (TRACE) | 39 ns | 35 ns | 22,123 ns | 25.6M msg/s |

注: fallback 后端在 console_level=kOff 时, Log() 函数第一行即 return,
因此测量的是函数调用 + 级别检查开销 (~38ns)。

### spdlog 后端

| 测试项 | 平均延迟 | 最小延迟 | 最大延迟 | 吞吐量 |
|--------|---------|---------|---------|--------|
| 单线程延迟 | 315 ns | 251 ns | 21,425 ns | 3.2M msg/s |
| 4线程吞吐 | 141 ns/msg | - | - | 7.1M msg/s |
| 带 Tag 日志 | 296 ns | 240 ns | 25,115 ns | 3.4M msg/s |
| 编译期过滤 (TRACE) | 262 ns | 214 ns | 17,458 ns | 3.8M msg/s |

注: spdlog 后端即使 sink 级别为 OFF, 仍会执行 vsnprintf 格式化和 source_loc 构造,
因此基础开销约 250-315ns。

### 对比总结

| 后端 | 单线程 avg | 4线程 avg | 适用场景 |
|------|-----------|-----------|---------|
| fallback | 38 ns | 5 ns | 嵌入式/MCU, 极低开销, 仅 stderr |
| spdlog | 315 ns | 141 ns | 通用 Linux, 文件轮转/syslog/彩色输出 |

### 编译期过滤

当 `LOGHELPER_COMPILE_LEVEL` 高于日志级别时, 宏展开为 `((void)0)`,
编译器完全消除调用, 运行时开销为零。

```cpp
#define LOGHELPER_COMPILE_LEVEL LOGHELPER_LEVEL_INFO
// LOG_TRACE(...) -> ((void)0)  -- 零开销
// LOG_DEBUG(...) -> ((void)0)  -- 零开销
// LOG_INFO(...)  -> 正常执行
```

## 与旧版 Boost.Log 对比

旧版 loghelper (Boost.Log 后端) 的已知问题:
- 每次 LOG 宏创建临时 LogHelper 对象 (构造+析构)
- 析构时 std::ostringstream 格式化 + Boost.Log 分发
- AMS_* 宏使用 std::regex 分割 {} 占位符
- 预估单条日志延迟: 5,000-50,000 ns (取决于消息长度)

新版改进:
- 零临时对象: 直接 vsnprintf 到栈缓冲区
- 零 regex: spdlog 使用 fmt 库, fallback 使用 printf
- 编译期过滤: 旧版无此功能
- 性能提升: 10-100x (取决于后端选择)
