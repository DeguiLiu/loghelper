// Copyright (c) 2025 dgliu
// SPDX-License-Identifier: MIT
//
// loghelper.hpp -- C++14 header-only logging library with pluggable backends
//
// Backends (compile-time selection via LOGHELPER_BACKEND):
//   LOGHELPER_BACKEND_SPDLOG   (1) -- spdlog (default)
//   LOGHELPER_BACKEND_ZLOG     (2) -- zlog (pure C)
//   LOGHELPER_BACKEND_FALLBACK (3) -- built-in stderr, zero dependency
//
// Usage:
//   #define LOGHELPER_BACKEND LOGHELPER_BACKEND_SPDLOG
//   #include "loghelper/loghelper.hpp"
//
//   loghelper::LogEngine::Init("logger.cfg");
//   LOG_INFO("Server started on port %d", 8080);
//   LOG_TAG_WARN("NET", "Timeout after %d ms", timeout);

#ifndef LOGHELPER_LOGHELPER_HPP_
#define LOGHELPER_LOGHELPER_HPP_

#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>

#include <atomic>
#include <chrono>
#include <mutex>

// ============================================================================
// Backend IDs
// ============================================================================

#define LOGHELPER_BACKEND_SPDLOG   1
#define LOGHELPER_BACKEND_ZLOG     2
#define LOGHELPER_BACKEND_FALLBACK 3

#ifndef LOGHELPER_BACKEND
#define LOGHELPER_BACKEND LOGHELPER_BACKEND_SPDLOG
#endif

// ============================================================================
// Compile-time log level filter
// ============================================================================

#define LOGHELPER_LEVEL_TRACE 0
#define LOGHELPER_LEVEL_DEBUG 1
#define LOGHELPER_LEVEL_INFO  2
#define LOGHELPER_LEVEL_WARN  3
#define LOGHELPER_LEVEL_ERROR 4
#define LOGHELPER_LEVEL_FATAL 5
#define LOGHELPER_LEVEL_OFF   6

#ifndef LOGHELPER_COMPILE_LEVEL
#define LOGHELPER_COMPILE_LEVEL LOGHELPER_LEVEL_TRACE
#endif

// ============================================================================
// Backend-specific includes
// ============================================================================

#if LOGHELPER_BACKEND == LOGHELPER_BACKEND_SPDLOG
#include "spdlog/spdlog.h"                       // NOLINT
#include "spdlog/sinks/basic_file_sink.h"        // NOLINT
#include "spdlog/sinks/rotating_file_sink.h"     // NOLINT
#include "spdlog/sinks/stdout_color_sinks.h"     // NOLINT
#ifdef __linux__
#include "spdlog/sinks/syslog_sink.h"            // NOLINT
#endif
// syslog.h defines LOG_DEBUG/LOG_INFO/LOG_ERR etc. -- undef to avoid clash
#ifdef LOG_DEBUG
#undef LOG_DEBUG
#endif
#ifdef LOG_INFO
#undef LOG_INFO
#endif
#ifdef LOG_ERR
#undef LOG_ERR
#endif
#ifdef LOG_WARNING
#undef LOG_WARNING
#endif
#elif LOGHELPER_BACKEND == LOGHELPER_BACKEND_ZLOG
#include <zlog.h>                                 // NOLINT
#endif

// ============================================================================
// Portable __FILENAME__ macro
// ============================================================================

#ifndef LOGHELPER_FILENAME
#define LOGHELPER_FILENAME \
  (std::strrchr(__FILE__, '/') ? std::strrchr(__FILE__, '/') + 1 : __FILE__)
#endif

// Max log message length (stack buffer)
#ifndef LOGHELPER_MAX_MSG_LEN
#define LOGHELPER_MAX_MSG_LEN 2048
#endif

namespace loghelper {

// ============================================================================
// Log Level
// ============================================================================

enum Level : int32_t {
  kTrace = 0,
  kDebug = 1,
  kInfo  = 2,
  kWarn  = 3,
  kError = 4,
  kFatal = 5,
  kOff   = 6
};

/// Convert level to human-readable string.
inline const char* LevelToString(Level lv) noexcept {
  static const char* const kNames[] = {
      "TRACE", "DEBUG", "INFO", "WARN", "ERROR", "FATAL", "OFF"};
  int32_t idx = static_cast<int32_t>(lv);
  if (idx >= 0 && idx <= 6) return kNames[idx];
  return "?";
}

// ============================================================================
// LogConfig -- configuration (char arrays, no std::string on hot path)
// ============================================================================

struct LogConfig {
  Level   console_level    = kInfo;
  Level   file_level       = kDebug;
  Level   syslog_level     = kInfo;
  int32_t file_max_size_mb = 100;
  int32_t file_max_files   = 5;
  int32_t file_min_free_mb = 2000;
  char    file_path[256]   = "logs/app";
  char    syslog_addr[64]  = "";
  int32_t syslog_port      = 514;
  char    syslog_ident[64] = "loghelper";
  bool    enable_console   = true;
  bool    enable_file      = true;
  bool    enable_syslog    = false;
};

// ============================================================================
// detail -- internal helpers
// ============================================================================

namespace detail {

/// Trim leading/trailing whitespace in-place.
inline void TrimInPlace(char* s) noexcept {
  char* p = s;
  while (*p == ' ' || *p == '\t') ++p;
  if (p != s) std::memmove(s, p, std::strlen(p) + 1);
  size_t len = std::strlen(s);
  while (len > 0 && (s[len - 1] == ' ' || s[len - 1] == '\t' ||
                      s[len - 1] == '\r' || s[len - 1] == '\n')) {
    s[--len] = '\0';
  }
}

/// Lightweight INI parser -- no external dependency.
inline bool ParseIniFile(const char* path, LogConfig& cfg) noexcept {
  std::FILE* f = std::fopen(path, "r");
  if (!f) return false;

  char line[512];
  while (std::fgets(line, static_cast<int>(sizeof(line)), f)) {
    TrimInPlace(line);
    if (line[0] == '\0' || line[0] == '#' || line[0] == ';' ||
        line[0] == '[') {
      continue;
    }
    char* eq = std::strchr(line, '=');
    if (!eq) continue;
    *eq = '\0';
    char* key = line;
    char* val = eq + 1;
    TrimInPlace(key);
    TrimInPlace(val);

    // Console level (support both old and new key names)
    if (std::strcmp(key, "ConsoleLevel") == 0 ||
        std::strcmp(key, "ConsoleLogLevel") == 0) {
      cfg.console_level = static_cast<Level>(std::atoi(val));
    } else if (std::strcmp(key, "FileLevel") == 0 ||
               std::strcmp(key, "FileLogLevel") == 0) {
      cfg.file_level = static_cast<Level>(std::atoi(val));
    } else if (std::strcmp(key, "SyslogLevel") == 0 ||
               std::strcmp(key, "SysLogLevel") == 0) {
      cfg.syslog_level = static_cast<Level>(std::atoi(val));
    } else if (std::strcmp(key, "FileMaxSizeMB") == 0 ||
               std::strcmp(key, "FilelogMaxSize") == 0) {
      cfg.file_max_size_mb = std::atoi(val);
    } else if (std::strcmp(key, "FileMaxFiles") == 0) {
      cfg.file_max_files = std::atoi(val);
    } else if (std::strcmp(key, "FileMinFreeSpaceMB") == 0 ||
               std::strcmp(key, "FilelogMinFreeSpace") == 0) {
      cfg.file_min_free_mb = std::atoi(val);
    } else if (std::strcmp(key, "FilePath") == 0) {
      std::snprintf(cfg.file_path, sizeof(cfg.file_path), "%s", val);
    } else if (std::strcmp(key, "SyslogAddr") == 0 ||
               std::strcmp(key, "SysLogAddr") == 0) {
      std::snprintf(cfg.syslog_addr, sizeof(cfg.syslog_addr), "%s", val);
      cfg.enable_syslog = (val[0] != '\0');
    } else if (std::strcmp(key, "SyslogPort") == 0 ||
               std::strcmp(key, "SysLogPort") == 0) {
      cfg.syslog_port = std::atoi(val);
    } else if (std::strcmp(key, "SyslogIdent") == 0) {
      std::snprintf(cfg.syslog_ident, sizeof(cfg.syslog_ident), "%s", val);
    } else if (std::strcmp(key, "EnableConsole") == 0) {
      cfg.enable_console = (std::atoi(val) != 0);
    } else if (std::strcmp(key, "EnableFile") == 0) {
      cfg.enable_file = (std::atoi(val) != 0);
    } else if (std::strcmp(key, "EnableSyslog") == 0) {
      cfg.enable_syslog = (std::atoi(val) != 0);
    }
  }
  std::fclose(f);
  return true;
}

/// Format timestamp: "YYYY-MM-DD HH:MM:SS.uuuuuu"
inline int32_t FormatTimestamp(char* buf, size_t cap) noexcept {
  auto now = std::chrono::system_clock::now();
  auto tt = std::chrono::system_clock::to_time_t(now);
  auto us = std::chrono::duration_cast<std::chrono::microseconds>(
                now.time_since_epoch())
                .count() %
            1000000;
  struct tm tm_buf;
  localtime_r(&tt, &tm_buf);
  int32_t n = static_cast<int32_t>(
      std::strftime(buf, cap, "%Y-%m-%d %H:%M:%S", &tm_buf));
  n += std::snprintf(buf + n, cap - static_cast<size_t>(n), ".%06ld",
                     static_cast<long>(us));
  return n;
}

}  // namespace detail

// ============================================================================
// Fallback Backend (zero dependency, stderr output)
// ============================================================================

namespace fallback {

class Backend {
 public:
  static Backend& Instance() noexcept {
    static Backend inst;
    return inst;
  }

  void Init(const LogConfig& cfg) noexcept {
    cfg_ = cfg;
    inited_.store(true, std::memory_order_release);
  }

  bool IsInited() const noexcept {
    return inited_.load(std::memory_order_acquire);
  }

  const LogConfig& GetConfig() const noexcept { return cfg_; }

  void Log(Level lv, const char* tag, const char* file,
           int32_t line, const char* func,
           const char* fmt, va_list args) noexcept {
    if (lv < cfg_.console_level) return;

    char msg[LOGHELPER_MAX_MSG_LEN];
    std::vsnprintf(msg, sizeof(msg), fmt, args);

    char ts[32];
    detail::FormatTimestamp(ts, sizeof(ts));

    std::lock_guard<std::mutex> lock(mtx_);
    if (tag != nullptr && tag[0] != '\0') {
      std::fprintf(stderr, "[%s] [%-5s] [%s] [%s:%d:%s] %s\n",
                   ts, LevelToString(lv), tag, file, line, func, msg);
    } else {
      std::fprintf(stderr, "[%s] [%-5s] [%s:%d:%s] %s\n",
                   ts, LevelToString(lv), file, line, func, msg);
    }
    std::fflush(stderr);
  }

  void Flush() noexcept { std::fflush(stderr); }

  void Shutdown() noexcept {
    inited_.store(false, std::memory_order_release);
  }

 private:
  Backend() noexcept : inited_(false) {}
  Backend(const Backend&) = delete;
  Backend& operator=(const Backend&) = delete;

  LogConfig cfg_;
  std::mutex mtx_;
  std::atomic<bool> inited_;
};

}  // namespace fallback

// ============================================================================
// spdlog Backend
// ============================================================================

#if LOGHELPER_BACKEND == LOGHELPER_BACKEND_SPDLOG

namespace spdlog_backend {

inline ::spdlog::level::level_enum ToSpdLevel(Level lv) noexcept {
  switch (lv) {
    case kTrace: return ::spdlog::level::trace;
    case kDebug: return ::spdlog::level::debug;
    case kInfo:  return ::spdlog::level::info;
    case kWarn:  return ::spdlog::level::warn;
    case kError: return ::spdlog::level::err;
    case kFatal: return ::spdlog::level::critical;
    default:     return ::spdlog::level::off;
  }
}

class Backend {
 public:
  static Backend& Instance() noexcept {
    static Backend inst;
    return inst;
  }

  void Init(const LogConfig& cfg) noexcept {
    cfg_ = cfg;

    // spdlog init uses heap (std::vector, shared_ptr) -- acceptable,
    // this is a one-time setup path, not hot path.
    std::vector<::spdlog::sink_ptr> sinks;

    if (cfg.enable_console) {
      auto console =
          std::make_shared<::spdlog::sinks::stderr_color_sink_mt>();
      console->set_level(ToSpdLevel(cfg.console_level));
      console->set_pattern(
          "[%Y-%m-%d %H:%M:%S.%f] [%^%-5l%$] [%s:%#:%!] %v");
      sinks.push_back(console);
    }

    if (cfg.enable_file && cfg.file_path[0] != '\0') {
      char path[300];
      std::snprintf(path, sizeof(path), "%s.log", cfg.file_path);
      auto file =
          std::make_shared<::spdlog::sinks::rotating_file_sink_mt>(
              path,
              static_cast<size_t>(cfg.file_max_size_mb) * 1024 * 1024,
              static_cast<size_t>(cfg.file_max_files));
      file->set_level(ToSpdLevel(cfg.file_level));
      file->set_pattern("[%Y-%m-%d %H:%M:%S.%f] [%-5l] [%s:%#:%!] %v");
      sinks.push_back(file);
    }

#ifdef __linux__
    if (cfg.enable_syslog) {
      auto syslog_sink =
          std::make_shared<::spdlog::sinks::syslog_sink_mt>(
              cfg.syslog_ident, LOG_PID, LOG_USER, false);
      syslog_sink->set_level(ToSpdLevel(cfg.syslog_level));
      sinks.push_back(syslog_sink);
    }
#endif

    auto logger = std::make_shared<::spdlog::logger>(
        "default", sinks.begin(), sinks.end());
    logger->set_level(::spdlog::level::trace);  // let sinks filter
    logger->flush_on(ToSpdLevel(kWarn));
    ::spdlog::set_default_logger(logger);

    inited_.store(true, std::memory_order_release);
  }

  bool IsInited() const noexcept {
    return inited_.load(std::memory_order_acquire);
  }

  const LogConfig& GetConfig() const noexcept { return cfg_; }

  void Log(Level lv, const char* tag, const char* file,
           int32_t line, const char* func,
           const char* fmt, va_list args) noexcept {
    char msg[LOGHELPER_MAX_MSG_LEN];
    std::vsnprintf(msg, sizeof(msg), fmt, args);

    char tagged[LOGHELPER_MAX_MSG_LEN + 64];
    if (tag != nullptr && tag[0] != '\0') {
      std::snprintf(tagged, sizeof(tagged), "[%s] %s", tag, msg);
    } else {
      std::snprintf(tagged, sizeof(tagged), "%s", msg);
    }

    ::spdlog::source_loc src{file, line, func};
    ::spdlog::default_logger_raw()->log(src, ToSpdLevel(lv), tagged);
  }

  /// fmt-style logging for AMS_* macros ({} placeholders).
  template <typename... Args>
  void LogFmt(Level lv, const char* tag, const char* file,
              int32_t line, const char* func,
              const char* fmt_str, Args&&... args) noexcept {
    ::spdlog::source_loc src{file, line, func};
    if (tag != nullptr && tag[0] != '\0') {
      std::string prefixed = std::string("[") + tag + "] " + fmt_str;
      ::spdlog::default_logger_raw()->log(
          src, ToSpdLevel(lv), prefixed.c_str(),
          std::forward<Args>(args)...);
    } else {
      ::spdlog::default_logger_raw()->log(
          src, ToSpdLevel(lv), fmt_str,
          std::forward<Args>(args)...);
    }
  }

  void Flush() noexcept {
    if (inited_.load(std::memory_order_acquire)) {
      ::spdlog::default_logger_raw()->flush();
    }
  }

  void Shutdown() noexcept {
    if (inited_.load(std::memory_order_acquire)) {
      ::spdlog::shutdown();
      inited_.store(false, std::memory_order_release);
    }
  }

 private:
  Backend() noexcept : inited_(false) {}
  Backend(const Backend&) = delete;
  Backend& operator=(const Backend&) = delete;

  LogConfig cfg_;
  std::atomic<bool> inited_;
};

}  // namespace spdlog_backend

#endif  // LOGHELPER_BACKEND_SPDLOG

// ============================================================================
// zlog Backend
// ============================================================================

#if LOGHELPER_BACKEND == LOGHELPER_BACKEND_ZLOG

namespace zlog_backend {

class Backend {
 public:
  static Backend& Instance() noexcept {
    static Backend inst;
    return inst;
  }

  void Init(const LogConfig& cfg) noexcept {
    cfg_ = cfg;
    int32_t rc = zlog_init("/etc/zlog.conf");
    if (rc != 0) {
      rc = zlog_init("zlog.conf");
    }
    if (rc == 0) {
      cat_ = zlog_get_category("default");
      inited_.store(cat_ != nullptr, std::memory_order_release);
    }
  }

  bool IsInited() const noexcept {
    return inited_.load(std::memory_order_acquire);
  }

  const LogConfig& GetConfig() const noexcept { return cfg_; }

  void Log(Level lv, const char* tag, const char* file,
           int32_t line, const char* func,
           const char* fmt, va_list args) noexcept {
    if (cat_ == nullptr) return;

    char msg[LOGHELPER_MAX_MSG_LEN];
    std::vsnprintf(msg, sizeof(msg), fmt, args);

    char tagged[LOGHELPER_MAX_MSG_LEN + 128];
    if (tag != nullptr && tag[0] != '\0') {
      std::snprintf(tagged, sizeof(tagged), "[%s] [%s:%d:%s] %s",
                    tag, file, line, func, msg);
    } else {
      std::snprintf(tagged, sizeof(tagged), "[%s:%d:%s] %s",
                    file, line, func, msg);
    }

    switch (lv) {
      case kTrace: zlog_debug(cat_, "%s", tagged); break;
      case kDebug: zlog_debug(cat_, "%s", tagged); break;
      case kInfo:  zlog_info(cat_, "%s", tagged);  break;
      case kWarn:  zlog_warn(cat_, "%s", tagged);  break;
      case kError: zlog_error(cat_, "%s", tagged); break;
      case kFatal: zlog_fatal(cat_, "%s", tagged); break;
      default: break;
    }
  }

  void Flush() noexcept {}

  void Shutdown() noexcept {
    if (inited_.load(std::memory_order_acquire)) {
      zlog_fini();
      cat_ = nullptr;
      inited_.store(false, std::memory_order_release);
    }
  }

 private:
  Backend() noexcept : inited_(false), cat_(nullptr) {}
  Backend(const Backend&) = delete;
  Backend& operator=(const Backend&) = delete;

  LogConfig cfg_;
  std::atomic<bool> inited_;
  zlog_category_t* cat_;
};

}  // namespace zlog_backend

#endif  // LOGHELPER_BACKEND_ZLOG

// ============================================================================
// Backend type alias + LogEngine
// ============================================================================

#if LOGHELPER_BACKEND == LOGHELPER_BACKEND_SPDLOG
using ActiveBackend = spdlog_backend::Backend;
#elif LOGHELPER_BACKEND == LOGHELPER_BACKEND_ZLOG
using ActiveBackend = zlog_backend::Backend;
#else
using ActiveBackend = fallback::Backend;
#endif

/// Unified initialization facade.
class LogEngine {
 public:
  /// Init with config struct.
  static void Init(const LogConfig& cfg) noexcept {
    ActiveBackend::Instance().Init(cfg);
    fallback::Backend::Instance().Init(cfg);  // safety net
  }

  /// Init from INI config file.
  static bool Init(const char* ini_path) noexcept {
    LogConfig cfg;
    if (!detail::ParseIniFile(ini_path, cfg)) {
      std::fprintf(stderr,
                   "[loghelper] Config not found: %s, using defaults\n",
                   ini_path);
    }
    Init(cfg);
    return true;
  }

  /// Init with defaults (console only, INFO level).
  static void Init() noexcept {
    LogConfig cfg;
    cfg.enable_file = false;
    Init(cfg);
  }

  static bool IsInited() noexcept {
    return ActiveBackend::Instance().IsInited();
  }

  static const LogConfig& GetConfig() noexcept {
    return ActiveBackend::Instance().GetConfig();
  }

  static void Flush() noexcept {
    ActiveBackend::Instance().Flush();
  }

  static void Shutdown() noexcept {
    ActiveBackend::Instance().Shutdown();
  }
};

// ============================================================================
// Core dispatch (printf-style, variadic)
// ============================================================================

namespace detail {

inline void LogDispatch(Level lv, const char* tag, const char* file,
                        int32_t line, const char* func,
                        const char* fmt, ...) noexcept {
  if (!LogEngine::IsInited()) {
    LogEngine::Init();
  }
  va_list args;
  va_start(args, fmt);
  ActiveBackend::Instance().Log(lv, tag, file, line, func, fmt, args);
  va_end(args);
}

}  // namespace detail

}  // namespace loghelper

// ============================================================================
// LOG_* macros (printf-style, compile-time filtered)
// ============================================================================

#if LOGHELPER_COMPILE_LEVEL <= LOGHELPER_LEVEL_TRACE
#define LOG_TRACE(fmt, ...) \
  loghelper::detail::LogDispatch(loghelper::kTrace, nullptr, \
      LOGHELPER_FILENAME, __LINE__, __func__, fmt, ##__VA_ARGS__)
#else
#define LOG_TRACE(fmt, ...) ((void)0)
#endif

#if LOGHELPER_COMPILE_LEVEL <= LOGHELPER_LEVEL_DEBUG
#define LOG_DEBUG(fmt, ...) \
  loghelper::detail::LogDispatch(loghelper::kDebug, nullptr, \
      LOGHELPER_FILENAME, __LINE__, __func__, fmt, ##__VA_ARGS__)
#else
#define LOG_DEBUG(fmt, ...) ((void)0)
#endif

#if LOGHELPER_COMPILE_LEVEL <= LOGHELPER_LEVEL_INFO
#define LOG_INFO(fmt, ...) \
  loghelper::detail::LogDispatch(loghelper::kInfo, nullptr, \
      LOGHELPER_FILENAME, __LINE__, __func__, fmt, ##__VA_ARGS__)
#else
#define LOG_INFO(fmt, ...) ((void)0)
#endif

#if LOGHELPER_COMPILE_LEVEL <= LOGHELPER_LEVEL_WARN
#define LOG_WARN(fmt, ...) \
  loghelper::detail::LogDispatch(loghelper::kWarn, nullptr, \
      LOGHELPER_FILENAME, __LINE__, __func__, fmt, ##__VA_ARGS__)
#else
#define LOG_WARN(fmt, ...) ((void)0)
#endif

#if LOGHELPER_COMPILE_LEVEL <= LOGHELPER_LEVEL_ERROR
#define LOG_ERROR(fmt, ...) \
  loghelper::detail::LogDispatch(loghelper::kError, nullptr, \
      LOGHELPER_FILENAME, __LINE__, __func__, fmt, ##__VA_ARGS__)
#else
#define LOG_ERROR(fmt, ...) ((void)0)
#endif

#if LOGHELPER_COMPILE_LEVEL <= LOGHELPER_LEVEL_FATAL
#define LOG_FATAL(fmt, ...) \
  do { \
    loghelper::detail::LogDispatch(loghelper::kFatal, nullptr, \
        LOGHELPER_FILENAME, __LINE__, __func__, fmt, ##__VA_ARGS__); \
  } while (0)
#else
#define LOG_FATAL(fmt, ...) ((void)0)
#endif

// ============================================================================
// LOG_TAG_* macros (with channel tag)
// ============================================================================

#if LOGHELPER_COMPILE_LEVEL <= LOGHELPER_LEVEL_TRACE
#define LOG_TAG_TRACE(tag, fmt, ...) \
  loghelper::detail::LogDispatch(loghelper::kTrace, tag, \
      LOGHELPER_FILENAME, __LINE__, __func__, fmt, ##__VA_ARGS__)
#else
#define LOG_TAG_TRACE(tag, fmt, ...) ((void)0)
#endif

#if LOGHELPER_COMPILE_LEVEL <= LOGHELPER_LEVEL_DEBUG
#define LOG_TAG_DEBUG(tag, fmt, ...) \
  loghelper::detail::LogDispatch(loghelper::kDebug, tag, \
      LOGHELPER_FILENAME, __LINE__, __func__, fmt, ##__VA_ARGS__)
#else
#define LOG_TAG_DEBUG(tag, fmt, ...) ((void)0)
#endif

#if LOGHELPER_COMPILE_LEVEL <= LOGHELPER_LEVEL_INFO
#define LOG_TAG_INFO(tag, fmt, ...) \
  loghelper::detail::LogDispatch(loghelper::kInfo, tag, \
      LOGHELPER_FILENAME, __LINE__, __func__, fmt, ##__VA_ARGS__)
#else
#define LOG_TAG_INFO(tag, fmt, ...) ((void)0)
#endif

#if LOGHELPER_COMPILE_LEVEL <= LOGHELPER_LEVEL_WARN
#define LOG_TAG_WARN(tag, fmt, ...) \
  loghelper::detail::LogDispatch(loghelper::kWarn, tag, \
      LOGHELPER_FILENAME, __LINE__, __func__, fmt, ##__VA_ARGS__)
#else
#define LOG_TAG_WARN(tag, fmt, ...) ((void)0)
#endif

#if LOGHELPER_COMPILE_LEVEL <= LOGHELPER_LEVEL_ERROR
#define LOG_TAG_ERROR(tag, fmt, ...) \
  loghelper::detail::LogDispatch(loghelper::kError, tag, \
      LOGHELPER_FILENAME, __LINE__, __func__, fmt, ##__VA_ARGS__)
#else
#define LOG_TAG_ERROR(tag, fmt, ...) ((void)0)
#endif

#if LOGHELPER_COMPILE_LEVEL <= LOGHELPER_LEVEL_FATAL
#define LOG_TAG_FATAL(tag, fmt, ...) \
  loghelper::detail::LogDispatch(loghelper::kFatal, tag, \
      LOGHELPER_FILENAME, __LINE__, __func__, fmt, ##__VA_ARGS__)
#else
#define LOG_TAG_FATAL(tag, fmt, ...) ((void)0)
#endif

// ============================================================================
// LOG_*_IF macros (conditional logging)
// ============================================================================

#if LOGHELPER_COMPILE_LEVEL <= LOGHELPER_LEVEL_TRACE
#define LOG_TRACE_IF(cond, fmt, ...) \
  do { if (cond) LOG_TRACE(fmt, ##__VA_ARGS__); } while (0)
#else
#define LOG_TRACE_IF(cond, fmt, ...) ((void)0)
#endif

#if LOGHELPER_COMPILE_LEVEL <= LOGHELPER_LEVEL_DEBUG
#define LOG_DEBUG_IF(cond, fmt, ...) \
  do { if (cond) LOG_DEBUG(fmt, ##__VA_ARGS__); } while (0)
#else
#define LOG_DEBUG_IF(cond, fmt, ...) ((void)0)
#endif

#if LOGHELPER_COMPILE_LEVEL <= LOGHELPER_LEVEL_INFO
#define LOG_INFO_IF(cond, fmt, ...) \
  do { if (cond) LOG_INFO(fmt, ##__VA_ARGS__); } while (0)
#else
#define LOG_INFO_IF(cond, fmt, ...) ((void)0)
#endif

#if LOGHELPER_COMPILE_LEVEL <= LOGHELPER_LEVEL_WARN
#define LOG_WARN_IF(cond, fmt, ...) \
  do { if (cond) LOG_WARN(fmt, ##__VA_ARGS__); } while (0)
#else
#define LOG_WARN_IF(cond, fmt, ...) ((void)0)
#endif

#if LOGHELPER_COMPILE_LEVEL <= LOGHELPER_LEVEL_ERROR
#define LOG_ERROR_IF(cond, fmt, ...) \
  do { if (cond) LOG_ERROR(fmt, ##__VA_ARGS__); } while (0)
#else
#define LOG_ERROR_IF(cond, fmt, ...) ((void)0)
#endif

// ============================================================================
// Performance measurement macros
// ============================================================================

#if LOGHELPER_COMPILE_LEVEL <= LOGHELPER_LEVEL_DEBUG
#define LOG_PERF_START(name) \
  auto loghelper_perf_start_##name = std::chrono::high_resolution_clock::now()

#define LOG_PERF_END(name) \
  do { \
    auto loghelper_perf_end_ = std::chrono::high_resolution_clock::now(); \
    auto loghelper_perf_us_ = std::chrono::duration_cast< \
        std::chrono::microseconds>( \
        loghelper_perf_end_ - loghelper_perf_start_##name).count(); \
    LOG_DEBUG("[PERF] %s: %ld us", #name, \
              static_cast<long>(loghelper_perf_us_)); \
  } while (0)
#else
#define LOG_PERF_START(name) ((void)0)
#define LOG_PERF_END(name) ((void)0)
#endif

// ============================================================================
// Assert macro (debug builds only)
// ============================================================================

#ifndef NDEBUG
#define LOG_ASSERT(cond, fmt, ...) \
  do { \
    if (!(cond)) { \
      loghelper::detail::LogDispatch(loghelper::kFatal, nullptr, \
          LOGHELPER_FILENAME, __LINE__, __func__, \
          "ASSERT FAILED: " #cond " -- " fmt, ##__VA_ARGS__); \
      std::abort(); \
    } \
  } while (0)
#else
#define LOG_ASSERT(cond, fmt, ...) ((void)0)
#endif

// ============================================================================
// AMS_* macros -- spdlog: fmt-style {}, others: printf fallback
// ============================================================================

#if LOGHELPER_BACKEND == LOGHELPER_BACKEND_SPDLOG

#define AMS_DEBUG(fmt, ...) \
  loghelper::spdlog_backend::Backend::Instance().LogFmt( \
      loghelper::kDebug, nullptr, \
      LOGHELPER_FILENAME, __LINE__, __func__, fmt, ##__VA_ARGS__)
#define AMS_INFO(fmt, ...) \
  loghelper::spdlog_backend::Backend::Instance().LogFmt( \
      loghelper::kInfo, nullptr, \
      LOGHELPER_FILENAME, __LINE__, __func__, fmt, ##__VA_ARGS__)
#define AMS_WARN(fmt, ...) \
  loghelper::spdlog_backend::Backend::Instance().LogFmt( \
      loghelper::kWarn, nullptr, \
      LOGHELPER_FILENAME, __LINE__, __func__, fmt, ##__VA_ARGS__)
#define AMS_ERROR(fmt, ...) \
  loghelper::spdlog_backend::Backend::Instance().LogFmt( \
      loghelper::kError, nullptr, \
      LOGHELPER_FILENAME, __LINE__, __func__, fmt, ##__VA_ARGS__)

#else

#define AMS_DEBUG(fmt, ...) LOG_DEBUG(fmt, ##__VA_ARGS__)
#define AMS_INFO(fmt, ...)  LOG_INFO(fmt, ##__VA_ARGS__)
#define AMS_WARN(fmt, ...)  LOG_WARN(fmt, ##__VA_ARGS__)
#define AMS_ERROR(fmt, ...) LOG_ERROR(fmt, ##__VA_ARGS__)

#endif

#endif  // LOGHELPER_LOGHELPER_HPP_
