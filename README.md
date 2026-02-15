# loghelper

C++14 header-only logging library with pluggable backends.

## Features

- Header-only, single file `loghelper.hpp`
- Compile-time backend selection: spdlog / zlog / fallback (zero dependency)
- Compile-time log level filtering (zero overhead when disabled)
- printf-style API with automatic file/line/function info
- Tagged logging for multi-module systems
- Conditional logging (`LOG_*_IF`)
- Performance measurement macros (`LOG_PERF_START/END`)
- INI config file support (backward compatible with old format)
- Thread-safe output
- spdlog backend: fmt-style `{}` placeholders via `AMS_*` macros

## Quick Start

```cpp
#include "loghelper/loghelper.hpp"

int main() {
    loghelper::LogEngine::Init();  // defaults: console, INFO level

    LOG_INFO("Server started on port %d", 8080);
    LOG_TAG_WARN("NET", "Timeout after %d ms", 500);
    LOG_ERROR("Failed: %s", strerror(errno));

    LOG_PERF_START(task);
    // ... work ...
    LOG_PERF_END(task);

    loghelper::LogEngine::Shutdown();
}
```

## Build

```bash
mkdir build && cd build

# spdlog backend (default)
cmake .. -DLOGHELPER_BACKEND=spdlog

# fallback backend (zero dependency)
cmake .. -DLOGHELPER_BACKEND=fallback

# zlog backend (requires system-installed zlog)
cmake .. -DLOGHELPER_BACKEND=zlog

make -j$(nproc)
ctest
```

## CMake Options

| Option | Default | Description |
|--------|---------|-------------|
| `LOGHELPER_BACKEND` | `spdlog` | Backend: `spdlog`, `zlog`, `fallback` |
| `LOGHELPER_BUILD_TESTS` | `ON` | Build Catch2 unit tests |
| `LOGHELPER_BUILD_BENCH` | `ON` | Build performance benchmarks |
| `LOGHELPER_BUILD_EXAMPLES` | `ON` | Build demo examples |

## API Reference

### Initialization

```cpp
// From INI config file
loghelper::LogEngine::Init("logger.cfg");

// Programmatic config
loghelper::LogConfig cfg;
cfg.console_level = loghelper::kDebug;
cfg.enable_file = true;
cfg.file_path = "logs/myapp";
loghelper::LogEngine::Init(cfg);

// Defaults (console only, INFO level)
loghelper::LogEngine::Init();
```

### Log Macros

| Macro | Description |
|-------|-------------|
| `LOG_TRACE(fmt, ...)` | Trace level |
| `LOG_DEBUG(fmt, ...)` | Debug level |
| `LOG_INFO(fmt, ...)` | Info level |
| `LOG_WARN(fmt, ...)` | Warning level |
| `LOG_ERROR(fmt, ...)` | Error level |
| `LOG_FATAL(fmt, ...)` | Fatal level |
| `LOG_TAG_*(tag, fmt, ...)` | With channel tag |
| `LOG_*_IF(cond, fmt, ...)` | Conditional |
| `LOG_PERF_START(name)` | Start timer |
| `LOG_PERF_END(name)` | End timer, print duration |
| `LOG_ASSERT(cond, fmt, ...)` | Debug-only assert |

### Compile-Time Control

```cpp
// Set before #include to filter at compile time (zero overhead)
#define LOGHELPER_COMPILE_LEVEL LOGHELPER_LEVEL_INFO
#include "loghelper/loghelper.hpp"
// LOG_TRACE and LOG_DEBUG are now completely removed
```

## Configuration File (INI)

```ini
[Log]
ConsoleLevel = 1        ; 0=TRACE 1=DEBUG 2=INFO 3=WARN 4=ERROR 5=FATAL
FileLevel = 0
FilePath = logs/app
FileMaxSizeMB = 100
FileMaxFiles = 5
EnableConsole = 1
EnableFile = 1
EnableSyslog = 0
```

## Performance

See `docs/benchmark_report.md` for full results.

| Metric | fallback | spdlog |
|--------|----------|--------|
| Single-thread avg latency | ~38 ns | ~315 ns |
| Single-thread throughput | ~26M msg/s | ~3.2M msg/s |
| 4-thread throughput | ~200M msg/s | ~7.1M msg/s |

## License

MIT
