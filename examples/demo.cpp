// Copyright (c) 2025 dgliu
// SPDX-License-Identifier: MIT
//
// demo.cpp -- loghelper usage demonstration

#include "loghelper/loghelper.hpp"

#include <cstdio>

#include <thread>

// Simulate a network module
static void NetworkTask() {
  LOG_TAG_INFO("NET", "Connecting to server...");
  std::this_thread::sleep_for(std::chrono::milliseconds(50));
  LOG_TAG_WARN("NET", "Connection timeout after %d ms", 5000);
  LOG_TAG_ERROR("NET", "Retry %d/%d failed", 3, 3);
}

// Simulate a sensor module
static void SensorTask() {
  LOG_TAG_DEBUG("SENSOR", "Initializing ADC...");
  for (int32_t i = 0; i < 3; ++i) {
    LOG_TAG_INFO("SENSOR", "Reading #%d: value=%d", i, 1024 + i * 10);
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
  }
  LOG_TAG_INFO("SENSOR", "Calibration complete");
}

int main(int argc, char* argv[]) {
  // ---- Init from config file or defaults ----
  if (argc > 1) {
    std::printf("Loading config: %s\n", argv[1]);
    loghelper::LogEngine::Init(argv[1]);
  } else {
    // Try default config, fallback to programmatic init
    loghelper::LogConfig cfg;
    cfg.console_level = loghelper::kTrace;
    cfg.enable_file = false;
    loghelper::LogEngine::Init(cfg);
  }

  // ---- Basic logging ----
  LOG_TRACE("Trace message: verbose detail");
  LOG_DEBUG("Debug message: x=%d y=%d", 10, 20);
  LOG_INFO("Application started, version %s", "2.0.0");
  LOG_WARN("Memory usage at %d%%", 85);
  LOG_ERROR("Failed to open file: %s", "/dev/sensor0");

  // ---- Conditional logging ----
  bool verbose = true;
  LOG_DEBUG_IF(verbose, "Verbose mode enabled, extra detail here");
  LOG_DEBUG_IF(!verbose, "This should NOT appear");

  // ---- Performance measurement ----
  LOG_PERF_START(sensor_read);
  std::this_thread::sleep_for(std::chrono::milliseconds(10));
  LOG_PERF_END(sensor_read);

  // ---- Tagged logging (multi-module) ----
  std::printf("\n--- Multi-module demo ---\n");
  std::thread t1(NetworkTask);
  std::thread t2(SensorTask);
  t1.join();
  t2.join();

  // ---- AMS-style fmt logging (spdlog backend only) ----
#if LOGHELPER_BACKEND == LOGHELPER_BACKEND_SPDLOG
  std::printf("\n--- AMS fmt-style demo (spdlog) ---\n");
  AMS_INFO("Server port={} workers={}", 8080, 4);
  AMS_WARN("Queue depth={} threshold={}", 95, 100);
  AMS_ERROR("Sensor {} read failed, code={}", "IMU", -1);
#endif

  // ---- Flush and shutdown ----
  loghelper::LogEngine::Flush();
  loghelper::LogEngine::Shutdown();

  std::printf("\nDemo complete.\n");
  return 0;
}
