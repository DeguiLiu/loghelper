# loghelper 设计文档

## 1. 目标

C++14 header-only 日志库，支持多后端编译期切换。

## 2. 约束

- C++14 标准，GCC/Clang
- 支持 spdlog / zlog 后端，可编译期切换
- 内置 fallback 后端（零依赖，stderr 输出）
- INI 配置文件支持

## 3. 后端对比

| 特性 | spdlog | zlog | NanoLog | fallback |
|------|--------|------|---------|----------|
| 语言 | C++11 header-only | Pure C | C++17 | C++14 |
| 延迟 | ~100ns | ~50ns | ~7ns | ~200ns |
| 文件轮转 | 内置 | 内置 | 无 | 无 |
| Syslog | 内置 | 内置 | 无 | 无 |
| Async | 内置 | 内置 | 仅 async | 无 |
| 格式化 | fmt | printf | printf | printf |
| 获取方式 | FetchContent | 需编译安装 | FetchContent | 内置 |
| C++14 兼容 | 是 (v1.x) | 是 (C库) | 否 (C++17) | 是 |

结论:
- 默认后端: spdlog（最成熟，header-only，FetchContent 友好）
- 备选后端: zlog（纯 C 高性能，适合极端场景）
- 排除 NanoLog: 需要 C++17，binary log 不适合嵌入式调试
- 内置 fallback: 零依赖 stderr 输出

## 4. 架构

```
loghelper.hpp (统一入口)
├── LogConfig        -- 配置结构 (POD)
├── LogEngine        -- 单例引擎 (初始化/销毁)
├── 宏层             -- LOG_TRACE/DEBUG/INFO/WARN/ERROR/FATAL
│   ├── LOG_*        -- 基础日志 (printf 风格)
│   ├── LOG_TAG_*    -- 带 channel tag
│   ├── LOG_*_IF     -- 条件日志
│   └── LOG_PERF_*   -- 性能测量
└── 后端适配层
    ├── LOGHELPER_BACKEND_SPDLOG  -- spdlog 适配 (默认)
    ├── LOGHELPER_BACKEND_ZLOG    -- zlog 适配
    └── LOGHELPER_BACKEND_FALLBACK -- 内置 stderr (零依赖)
```

## 5. 后端切换机制

编译期宏选择:
```cpp
// CMake 传入，或用户 #define
#define LOGHELPER_BACKEND_SPDLOG   1  // 默认
#define LOGHELPER_BACKEND_ZLOG     2
#define LOGHELPER_BACKEND_FALLBACK 3

#ifndef LOGHELPER_BACKEND
#define LOGHELPER_BACKEND LOGHELPER_BACKEND_SPDLOG
#endif
```

## 6. API 设计

### 6.1 初始化

```cpp
// 方式1: INI 配置文件
loghelper::LogEngine::Init("logger.cfg");

// 方式2: 编程式配置
loghelper::LogConfig cfg;
cfg.console_level = loghelper::kInfo;
cfg.file_level = loghelper::kDebug;
cfg.file_max_size_mb = 100;
cfg.file_path = "logs/app";
loghelper::LogEngine::Init(cfg);

// 方式3: 默认配置 (console only, INFO level)
loghelper::LogEngine::Init();
```

### 6.2 日志宏

```cpp
// printf 风格 (所有后端统一)
LOG_INFO("Server started on port %d", 8080);
LOG_ERROR("Connection failed: %s", strerror(errno));

// 带 tag
LOG_TAG_INFO("NET", "Received %zu bytes", len);

// 条件日志
LOG_DEBUG_IF(verbose, "Detail: x=%d y=%d", x, y);

// 性能测量
LOG_PERF_START(db_query);
// ... work ...
LOG_PERF_END(db_query);  // 输出: [PERF] db_query: 1234 us
```

### 6.3 AMS 风格

```cpp
// AMS 风格 {} 占位符 (基于 fmt，仅 spdlog)
AMS_INFO("value {} name {}", 42, "test");
```

## 7. 配置文件格式

INI 格式，内置轻量解析器 (~60 行):

```ini
[Log]
ConsoleLevel = 2
FileLevel = 1
FileMaxSizeMB = 100
FileMinFreeSpaceMB = 2000
FilePath = logs/app
SyslogAddr = 192.168.10.199
SyslogPort = 514
SyslogLevel = 2
```

INI 解析: 内置简单解析器 (~60 行), 不依赖外部库。

## 8. 性能基准测试

测试项:
1. 单线程吞吐量 (msg/s)
2. 多线程吞吐量 (4 threads)
3. 单条日志延迟 (avg/min/max)
4. 编译期过滤开销 (应为零)
5. 运行时过滤开销 (sink 丢弃)

详见 `docs/benchmark_report.md`。

## 9. 文件结构

```
loghelper/
├── include/
│   └── loghelper/
│       └── loghelper.hpp      -- 统一入口 (header-only)
├── tests/
│   ├── test_loghelper.cpp     -- 功能测试
│   └── bench_loghelper.cpp    -- 性能基准
├── examples/
│   └── demo.cpp               -- 使用示例
├── conf/
│   └── logger.cfg             -- 示例配置
├── docs/
│   ├── design_zh.md           -- 本文档
│   └── benchmark_report.md    -- 性能报告
├── CMakeLists.txt             -- 根 CMake
└── README.md
```

## 10. CMake 选项

```cmake
option(LOGHELPER_BACKEND "Log backend: spdlog|zlog|fallback" "spdlog")
option(LOGHELPER_BUILD_TESTS "Build tests" ON)
option(LOGHELPER_BUILD_BENCH "Build benchmarks" ON)
option(LOGHELPER_BUILD_EXAMPLES "Build examples" ON)
```

spdlog 通过 FetchContent 自动获取，zlog 需要系统安装。
