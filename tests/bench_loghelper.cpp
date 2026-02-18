// Copyright (c) 2025 dgliu
// SPDX-License-Identifier: MIT
//
// bench_loghelper.cpp -- Performance benchmark for loghelper backends

#include "loghelper/loghelper.hpp"

#include <cstdint>
#include <cstdio>

#include <chrono>
#include <thread>
#include <vector>

// ============================================================================
// Benchmark helpers
// ============================================================================

struct BenchResult {
  const char* name;
  int64_t total_ns;
  int32_t count;
  int64_t min_ns;
  int64_t max_ns;
};

static void PrintResult(const BenchResult& r) {
  int64_t avg_ns = r.total_ns / r.count;
  double throughput = 1e9 / static_cast<double>(avg_ns);
  std::printf(
      "  %-40s  avg=%6ld ns  min=%6ld ns  max=%8ld ns  "
      "throughput=%.0f msg/s\n",
      r.name, static_cast<long>(avg_ns), static_cast<long>(r.min_ns), static_cast<long>(r.max_ns), throughput);
}

static void PrintSeparator() {
  std::printf("  %s\n",
              "------------------------------------------------------------"
              "-----------------------------");
}

// ============================================================================
// Single-thread latency benchmark
// ============================================================================

static BenchResult BenchSingleThread(const char* name, int32_t count) {
  BenchResult result{name, 0, count, INT64_MAX, 0};

  // Warmup
  for (int32_t i = 0; i < 100; ++i) {
    LOG_INFO("warmup %d", i);
  }

  for (int32_t i = 0; i < count; ++i) {
    auto start = std::chrono::high_resolution_clock::now();
    LOG_INFO("bench single-thread msg %d value=%d", i, i * 42);
    auto end = std::chrono::high_resolution_clock::now();
    int64_t ns = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
    result.total_ns += ns;
    if (ns < result.min_ns)
      result.min_ns = ns;
    if (ns > result.max_ns)
      result.max_ns = ns;
  }
  return result;
}

// ============================================================================
// Multi-thread throughput benchmark
// ============================================================================

static BenchResult BenchMultiThread(const char* name, int32_t threads, int32_t msgs_per_thread) {
  int32_t total = threads * msgs_per_thread;
  BenchResult result{name, 0, total, 0, 0};

  auto wall_start = std::chrono::high_resolution_clock::now();

  std::vector<std::thread> pool;
  for (int32_t t = 0; t < threads; ++t) {
    pool.emplace_back([t, msgs_per_thread]() {
      for (int32_t i = 0; i < msgs_per_thread; ++i) {
        LOG_INFO("bench mt thread=%d msg=%d", t, i);
      }
    });
  }
  for (auto& th : pool)
    th.join();

  auto wall_end = std::chrono::high_resolution_clock::now();
  result.total_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(wall_end - wall_start).count();
  result.min_ns = result.total_ns / total;  // approximate
  result.max_ns = result.total_ns / total;
  return result;
}

// ============================================================================
// Compile-time filtering benchmark (should be ~0 ns)
// ============================================================================

// Use a macro that is guaranteed to be compiled out (level > FATAL)
#define BENCH_COMPILED_OUT_LOG(fmt, ...) ((void)0)

static BenchResult BenchCompileTimeFilter(const char* name, int32_t count) {
  BenchResult result{name, 0, count, INT64_MAX, 0};

  for (int32_t i = 0; i < count; ++i) {
    auto start = std::chrono::high_resolution_clock::now();
    BENCH_COMPILED_OUT_LOG("this is compiled out %d", i);
    auto end = std::chrono::high_resolution_clock::now();
    int64_t ns = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
    result.total_ns += ns;
    if (ns < result.min_ns)
      result.min_ns = ns;
    if (ns > result.max_ns)
      result.max_ns = ns;
  }
  return result;
}

// Runtime-filtered: message formatted but sink drops it
static BenchResult BenchRuntimeFilter(const char* name, int32_t count) {
  BenchResult result{name, 0, count, INT64_MAX, 0};

  for (int32_t i = 0; i < count; ++i) {
    auto start = std::chrono::high_resolution_clock::now();
    LOG_TRACE("runtime filtered msg %d", i);
    auto end = std::chrono::high_resolution_clock::now();
    int64_t ns = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
    result.total_ns += ns;
    if (ns < result.min_ns)
      result.min_ns = ns;
    if (ns > result.max_ns)
      result.max_ns = ns;
  }
  return result;
}

// ============================================================================
// Tagged logging benchmark
// ============================================================================

static BenchResult BenchTaggedLog(const char* name, int32_t count) {
  BenchResult result{name, 0, count, INT64_MAX, 0};

  for (int32_t i = 0; i < count; ++i) {
    auto start = std::chrono::high_resolution_clock::now();
    LOG_TAG_INFO("BENCH", "tagged msg %d", i);
    auto end = std::chrono::high_resolution_clock::now();
    int64_t ns = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
    result.total_ns += ns;
    if (ns < result.min_ns)
      result.min_ns = ns;
    if (ns > result.max_ns)
      result.max_ns = ns;
  }
  return result;
}

// ============================================================================
// main
// ============================================================================

int main() {
  // Init with console output suppressed (measure pure logging overhead)
  loghelper::LogConfig cfg;
  cfg.console_level = loghelper::kOff;
  cfg.enable_file = false;
  cfg.enable_syslog = false;
  loghelper::LogEngine::Init(cfg);

  const char* backend_name = "unknown";
#if LOGHELPER_BACKEND == LOGHELPER_BACKEND_SPDLOG
  backend_name = "spdlog";
#elif LOGHELPER_BACKEND == LOGHELPER_BACKEND_ZLOG
  backend_name = "zlog";
#else
  backend_name = "fallback";
#endif

  std::printf("\n=== loghelper Benchmark (backend: %s) ===\n\n", backend_name);
  PrintSeparator();

  constexpr int32_t kSingleCount = 100000;
  constexpr int32_t kMultiThreads = 4;
  constexpr int32_t kMultiPerThread = 25000;

  // 1. Single-thread latency
  auto r1 = BenchSingleThread("Single-thread latency", kSingleCount);
  PrintResult(r1);

  // 2. Multi-thread throughput (4 threads)
  auto r2 = BenchMultiThread("Multi-thread (4T) throughput", kMultiThreads, kMultiPerThread);
  PrintResult(r2);

  // 3. Tagged logging
  auto r3 = BenchTaggedLog("Tagged logging latency", kSingleCount);
  PrintResult(r3);

  // 4. Compile-time filter (should be ~0 ns)
  auto r4 = BenchCompileTimeFilter("Compile-time filtered (noop)", kSingleCount);
  PrintResult(r4);

  // 5. Runtime filter (formatted but dropped by sink)
  auto r5 = BenchRuntimeFilter("Runtime filtered (TRACE, sink=OFF)", kSingleCount);
  PrintResult(r5);

  PrintSeparator();

  // Summary
  std::printf("\n  Total messages: %d\n", kSingleCount * 3 + kMultiThreads * kMultiPerThread);
  std::printf("  Backend: %s\n\n", backend_name);

  loghelper::LogEngine::Shutdown();
  return 0;
}
