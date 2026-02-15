# loghelper 性能基准报告

## 测试环境

- OS: Ubuntu 24.04 (x86_64)
- CPU: 服务器级 x86_64 (CI 环境)
- Compiler: GCC 13.3.0
- Build: Release (-O3)
- C++ Standard: C++14

## 测试方法

- 所有输出 sink 关闭 (console_level = kOff, enable_file = false)
- 测量纯日志框架开销 (格式化 + 级别检查 + 线程同步)
- 单线程: 100,000 条消息, 逐条计时 (含 warmup)
- 多线程: 4 线程 x 25,000 条, 计算总墙钟时间
- 编译期过滤: 宏展开为 `((void)0)`, 测量纯 clock 噪声

## 结果

### spdlog 后端 (Release -O3)

| 测试项 | 平均延迟 | 最小延迟 | 最大延迟 | 吞吐量 |
|--------|---------|---------|---------|--------|
| 单线程日志 | 249 ns | 206 ns | 38,174 ns | 4.0M msg/s |
| 带 Tag 日志 | 213 ns | 173 ns | 17,673 ns | 4.7M msg/s |
| 多线程 (4T) | 99 ns/msg | - | - | 10.1M msg/s |
| 运行时过滤 (TRACE, sink=OFF) | 183 ns | 147 ns | 20,061 ns | 5.5M msg/s |
| 编译期过滤 (noop) | 31 ns | 27 ns | 17,055 ns | 32.3M msg/s |

注: spdlog 后端即使 sink 级别为 OFF, 仍会执行 vsnprintf 格式化和 source_loc 构造,
因此运行时过滤开销约 183ns。编译期过滤的 ~31ns 是纯 clock_gettime 测量噪声。

### fallback 后端 (Release -O3)

| 测试项 | 平均延迟 | 最小延迟 | 最大延迟 | 吞吐量 |
|--------|---------|---------|---------|--------|
| 单线程日志 | 39 ns | 37 ns | 8,103 ns | 25.6M msg/s |
| 带 Tag 日志 | 41 ns | 37 ns | 9,555 ns | 24.4M msg/s |
| 多线程 (4T) | 6 ns/msg | - | - | 166.7M msg/s |
| 运行时过滤 (TRACE, sink=OFF) | 40 ns | 35 ns | 16,842 ns | 25.0M msg/s |
| 编译期过滤 (noop) | 30 ns | 27 ns | 25,642 ns | 33.3M msg/s |

注: fallback 后端在 console_level=kOff 时, Log() 函数第一行即 return,
因此测量的是函数调用 + 级别检查开销 (~39ns)。

### spdlog 后端 (Debug)

| 测试项 | 平均延迟 | 吞吐量 |
|--------|---------|--------|
| 单线程日志 | 321 ns | 3.1M msg/s |
| 多线程 (4T) | 136 ns/msg | 7.4M msg/s |

### 对比总结

| 后端 | 单线程 avg | 4T 吞吐 | 编译期过滤 | 适用场景 |
|------|-----------|---------|-----------|---------|
| spdlog | 249 ns | 10.1M msg/s | 31 ns (noop) | 通用 Linux, 文件轮转/syslog/彩色输出 |
| fallback | 39 ns | 166.7M msg/s | 30 ns (noop) | 嵌入式/极简, 零依赖, 仅 stderr |

### 编译期过滤

当 `LOGHELPER_COMPILE_LEVEL` 高于日志级别时, 宏展开为 `((void)0)`,
编译器完全消除调用, 运行时开销为零。

```cpp
#define LOGHELPER_COMPILE_LEVEL LOGHELPER_LEVEL_INFO
// LOG_TRACE(...) -> ((void)0)  -- 零开销
// LOG_DEBUG(...) -> ((void)0)  -- 零开销
// LOG_INFO(...)  -> 正常执行
```

## 建议

- 生产环境: spdlog 后端 + `LOGHELPER_COMPILE_LEVEL=LOGHELPER_LEVEL_INFO`
- 极简/调试: fallback 后端, 零依赖
- 热路径: 编译期过滤优于运行时过滤 (183ns vs 0ns)
- ARM 嵌入式: 预期延迟约为 x86 的 2-5x, 仍在 1us 以内
