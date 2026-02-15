// Copyright (c) 2025 dgliu
// SPDX-License-Identifier: MIT
//
// test_loghelper.cpp -- Catch2 v3 unit tests for loghelper.hpp

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <cstdio>
#include <cstring>
#include <functional>
#include <fstream>
#include <string>
#include <thread>
#include <unistd.h>
#include <vector>

#include "loghelper/loghelper.hpp"

// ============================================================================
// Helper: capture stderr output
// ============================================================================

static std::string CaptureStderr(std::function<void()> fn) {
  // Redirect stderr to a temp file
  char tmpname[] = "/tmp/loghelper_test_XXXXXX";
  int fd = mkstemp(tmpname);
  REQUIRE(fd >= 0);

  std::fflush(stderr);
  int saved = dup(fileno(stderr));
  dup2(fd, fileno(stderr));

  fn();

  std::fflush(stderr);
  dup2(saved, fileno(stderr));
  close(saved);
  close(fd);

  // Read captured output
  std::ifstream ifs(tmpname);
  std::string content((std::istreambuf_iterator<char>(ifs)),
                       std::istreambuf_iterator<char>());
  std::remove(tmpname);
  return content;
}

// ============================================================================
// LogConfig tests
// ============================================================================

TEST_CASE("LogConfig default values", "[config]") {
  loghelper::LogConfig cfg;
  CHECK(cfg.console_level == loghelper::kInfo);
  CHECK(cfg.file_level == loghelper::kDebug);
  CHECK(cfg.syslog_level == loghelper::kInfo);
  CHECK(cfg.file_max_size_mb == 100);
  CHECK(cfg.file_max_files == 5);
  CHECK(cfg.syslog_port == 514);
  CHECK(cfg.enable_console == true);
  CHECK(cfg.enable_file == true);
  CHECK(cfg.enable_syslog == false);
  CHECK(std::strcmp(cfg.file_path, "logs/app") == 0);
}

// ============================================================================
// Level tests
// ============================================================================

TEST_CASE("LevelToString", "[level]") {
  CHECK(std::strcmp(loghelper::LevelToString(loghelper::kTrace), "TRACE") == 0);
  CHECK(std::strcmp(loghelper::LevelToString(loghelper::kDebug), "DEBUG") == 0);
  CHECK(std::strcmp(loghelper::LevelToString(loghelper::kInfo), "INFO") == 0);
  CHECK(std::strcmp(loghelper::LevelToString(loghelper::kWarn), "WARN") == 0);
  CHECK(std::strcmp(loghelper::LevelToString(loghelper::kError), "ERROR") == 0);
  CHECK(std::strcmp(loghelper::LevelToString(loghelper::kFatal), "FATAL") == 0);
  CHECK(std::strcmp(loghelper::LevelToString(loghelper::kOff), "OFF") == 0);
  // Out of range
  CHECK(std::strcmp(loghelper::LevelToString(static_cast<loghelper::Level>(99)),
                    "?") == 0);
}

// ============================================================================
// INI parser tests
// ============================================================================

TEST_CASE("ParseIniFile -- valid file", "[ini]") {
  // Write a temp INI file
  char tmpname[] = "/tmp/loghelper_ini_XXXXXX";
  int fd = mkstemp(tmpname);
  REQUIRE(fd >= 0);

  const char* ini_content =
      "[Log]\n"
      "ConsoleLevel = 1\n"
      "FileLevel = 0\n"
      "SyslogLevel = 3\n"
      "FileMaxSizeMB = 200\n"
      "FileMaxFiles = 10\n"
      "FilePath = /var/log/myapp\n"
      "SyslogAddr = 10.0.0.1\n"
      "SyslogPort = 1514\n"
      "SyslogIdent = myapp\n"
      "EnableConsole = 1\n"
      "EnableFile = 0\n"
      "EnableSyslog = 1\n";
  write(fd, ini_content, std::strlen(ini_content));
  close(fd);

  loghelper::LogConfig cfg;
  bool ok = loghelper::detail::ParseIniFile(tmpname, cfg);
  CHECK(ok);
  CHECK(cfg.console_level == loghelper::kDebug);
  CHECK(cfg.file_level == loghelper::kTrace);
  CHECK(cfg.syslog_level == loghelper::kWarn);
  CHECK(cfg.file_max_size_mb == 200);
  CHECK(cfg.file_max_files == 10);
  CHECK(std::strcmp(cfg.file_path, "/var/log/myapp") == 0);
  CHECK(std::strcmp(cfg.syslog_addr, "10.0.0.1") == 0);
  CHECK(cfg.syslog_port == 1514);
  CHECK(std::strcmp(cfg.syslog_ident, "myapp") == 0);
  CHECK(cfg.enable_console == true);
  CHECK(cfg.enable_file == false);
  CHECK(cfg.enable_syslog == true);

  std::remove(tmpname);
}

TEST_CASE("ParseIniFile -- old key names (backward compat)", "[ini]") {
  char tmpname[] = "/tmp/loghelper_ini2_XXXXXX";
  int fd = mkstemp(tmpname);
  REQUIRE(fd >= 0);

  const char* ini_content =
      "[SysLog]\n"
      "ConsoleLogLevel = 4\n"
      "FileLogLevel = 3\n"
      "SysLogLevel = 2\n"
      "FilelogMaxSize = 50\n"
      "FilelogMinFreeSpace = 500\n"
      "SysLogAddr = 192.168.1.1\n"
      "SysLogPort = 514\n";
  write(fd, ini_content, std::strlen(ini_content));
  close(fd);

  loghelper::LogConfig cfg;
  bool ok = loghelper::detail::ParseIniFile(tmpname, cfg);
  CHECK(ok);
  CHECK(cfg.console_level == loghelper::kError);
  CHECK(cfg.file_level == loghelper::kWarn);
  CHECK(cfg.syslog_level == loghelper::kInfo);
  CHECK(cfg.file_max_size_mb == 50);
  CHECK(cfg.file_min_free_mb == 500);
  CHECK(std::strcmp(cfg.syslog_addr, "192.168.1.1") == 0);

  std::remove(tmpname);
}

TEST_CASE("ParseIniFile -- missing file returns false", "[ini]") {
  loghelper::LogConfig cfg;
  bool ok = loghelper::detail::ParseIniFile("/nonexistent/path.cfg", cfg);
  CHECK_FALSE(ok);
  // Config should retain defaults
  CHECK(cfg.console_level == loghelper::kInfo);
}

TEST_CASE("ParseIniFile -- comments and empty lines", "[ini]") {
  char tmpname[] = "/tmp/loghelper_ini3_XXXXXX";
  int fd = mkstemp(tmpname);
  REQUIRE(fd >= 0);

  const char* ini_content =
      "# This is a comment\n"
      "; Another comment\n"
      "\n"
      "[Section]\n"
      "ConsoleLevel = 0\n"
      "  # inline-ish comment line\n";
  write(fd, ini_content, std::strlen(ini_content));
  close(fd);

  loghelper::LogConfig cfg;
  bool ok = loghelper::detail::ParseIniFile(tmpname, cfg);
  CHECK(ok);
  CHECK(cfg.console_level == loghelper::kTrace);

  std::remove(tmpname);
}

// ============================================================================
// LogEngine tests
// ============================================================================

TEST_CASE("LogEngine::Init() with defaults", "[engine]") {
  loghelper::LogEngine::Init();
  CHECK(loghelper::LogEngine::IsInited());
}

TEST_CASE("LogEngine::Init(config)", "[engine]") {
  loghelper::LogConfig cfg;
  cfg.console_level = loghelper::kWarn;
  cfg.enable_file = false;
  loghelper::LogEngine::Init(cfg);
  CHECK(loghelper::LogEngine::IsInited());
  CHECK(loghelper::LogEngine::GetConfig().console_level == loghelper::kWarn);
}

TEST_CASE("LogEngine::Init(ini_path) -- missing file uses defaults", "[engine]") {
  loghelper::LogEngine::Init("/nonexistent.cfg");
  CHECK(loghelper::LogEngine::IsInited());
}

// ============================================================================
// Macro output tests (fallback backend captures to stderr)
// ============================================================================

#if LOGHELPER_BACKEND == LOGHELPER_BACKEND_FALLBACK

TEST_CASE("LOG_INFO outputs to stderr", "[macro][fallback]") {
  loghelper::LogConfig cfg;
  cfg.console_level = loghelper::kTrace;
  cfg.enable_file = false;
  loghelper::LogEngine::Init(cfg);

  auto output = CaptureStderr([] {
    LOG_INFO("hello %d", 42);
  });
  CHECK_THAT(output, Catch::Matchers::ContainsSubstring("INFO"));
  CHECK_THAT(output, Catch::Matchers::ContainsSubstring("hello 42"));
}

TEST_CASE("LOG_TAG_WARN includes tag", "[macro][fallback]") {
  loghelper::LogConfig cfg;
  cfg.console_level = loghelper::kTrace;
  cfg.enable_file = false;
  loghelper::LogEngine::Init(cfg);

  auto output = CaptureStderr([] {
    LOG_TAG_WARN("NET", "timeout %d ms", 500);
  });
  CHECK_THAT(output, Catch::Matchers::ContainsSubstring("[NET]"));
  CHECK_THAT(output, Catch::Matchers::ContainsSubstring("WARN"));
  CHECK_THAT(output, Catch::Matchers::ContainsSubstring("timeout 500 ms"));
}

TEST_CASE("LOG_DEBUG_IF conditional", "[macro][fallback]") {
  loghelper::LogConfig cfg;
  cfg.console_level = loghelper::kTrace;
  cfg.enable_file = false;
  loghelper::LogEngine::Init(cfg);

  auto output_true = CaptureStderr([] {
    LOG_DEBUG_IF(true, "visible");
  });
  CHECK_THAT(output_true, Catch::Matchers::ContainsSubstring("visible"));

  auto output_false = CaptureStderr([] {
    LOG_DEBUG_IF(false, "hidden");
  });
  CHECK(output_false.find("hidden") == std::string::npos);
}

TEST_CASE("Level filtering -- below threshold not output", "[macro][fallback]") {
  loghelper::LogConfig cfg;
  cfg.console_level = loghelper::kError;
  cfg.enable_file = false;
  loghelper::LogEngine::Init(cfg);

  auto output = CaptureStderr([] {
    LOG_INFO("should not appear");
    LOG_DEBUG("should not appear");
    LOG_ERROR("should appear");
  });
  CHECK(output.find("should not appear") == std::string::npos);
  CHECK_THAT(output, Catch::Matchers::ContainsSubstring("should appear"));
}

#endif  // LOGHELPER_BACKEND_FALLBACK

// ============================================================================
// Multi-thread safety test
// ============================================================================

TEST_CASE("Multi-thread logging does not crash", "[thread]") {
  loghelper::LogConfig cfg;
  cfg.console_level = loghelper::kOff;  // suppress output
  cfg.enable_file = false;
  loghelper::LogEngine::Init(cfg);

  constexpr int32_t kThreads = 4;
  constexpr int32_t kMsgsPerThread = 1000;

  std::vector<std::thread> threads;
  for (int32_t t = 0; t < kThreads; ++t) {
    threads.emplace_back([t]() {
      for (int32_t i = 0; i < kMsgsPerThread; ++i) {
        LOG_INFO("thread %d msg %d", t, i);
      }
    });
  }
  for (auto& th : threads) th.join();
  // If we get here without crash/hang, test passes
  CHECK(true);
}

// ============================================================================
// Timestamp format test
// ============================================================================

TEST_CASE("FormatTimestamp produces valid format", "[detail]") {
  char buf[64];
  int32_t n = loghelper::detail::FormatTimestamp(buf, sizeof(buf));
  CHECK(n > 0);
  // Should look like "2025-02-15 12:34:56.123456"
  CHECK(buf[4] == '-');
  CHECK(buf[7] == '-');
  CHECK(buf[10] == ' ');
  CHECK(buf[13] == ':');
  CHECK(buf[16] == ':');
  CHECK(buf[19] == '.');
}

// ============================================================================
// TrimInPlace test
// ============================================================================

TEST_CASE("TrimInPlace", "[detail]") {
  char s1[] = "  hello  ";
  loghelper::detail::TrimInPlace(s1);
  CHECK(std::strcmp(s1, "hello") == 0);

  char s2[] = "\t\tworld\r\n";
  loghelper::detail::TrimInPlace(s2);
  CHECK(std::strcmp(s2, "world") == 0);

  char s3[] = "nospace";
  loghelper::detail::TrimInPlace(s3);
  CHECK(std::strcmp(s3, "nospace") == 0);

  char s4[] = "";
  loghelper::detail::TrimInPlace(s4);
  CHECK(std::strcmp(s4, "") == 0);
}
