/*
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 2026 nILS Podewski
 *
 * This file is part of the picoTracker Boot Manager
 */

#include "bootlog.h"

#include "Adapters/picoTracker/sdcard/sdcard.h"
#include "Externals/SdFat/src/SdFat.h"
#include <cstdarg>
#include <cstdio>
#include <cstring>

namespace {

constexpr const char *kBootlogPath = "/bootloader.log";
constexpr size_t kBootlogBufferSize = 256;

static SdFs g_log_sd;
static bool g_log_sd_ready = false;
static bool g_log_busy = false;

bool mount_sd_for_log() {
  if (g_log_sd_ready) {
    return true;
  }

  if (g_log_sd.begin(SD_CONFIG)) {
    g_log_sd_ready = true;
    return true;
  }

  if (!g_log_sd.card() || g_log_sd.sdErrorCode() != 0) {
    return false;
  }

  if (!static_cast<FsVolume *>(&g_log_sd)->begin(g_log_sd.card(), true, 0)) {
    return false;
  }

  g_log_sd_ready = true;
  return true;
}

void append_line_to_log(const char *line) {
  if (!mount_sd_for_log()) {
    return;
  }

  FsFile log_file;
  if (!log_file.open(kBootlogPath, O_WRONLY | O_CREAT | O_APPEND)) {
    return;
  }

  const size_t len = std::strlen(line);
  if (len > 0) {
    (void)log_file.write(line, len);
  }
  if (len == 0 || line[len - 1] != '\n') {
    (void)log_file.write("\n", 1);
  }
  log_file.sync();
  log_file.close();
}

} // namespace

void bootlog(const char *fmt, ...) {
  if (fmt == nullptr || g_log_busy) {
    return;
  }

  g_log_busy = true;

  char buffer[kBootlogBufferSize];
  va_list args;
  va_start(args, fmt);
  const int written = std::vsnprintf(buffer, sizeof(buffer), fmt, args);
  va_end(args);

  if (written > 0) {
    buffer[sizeof(buffer) - 1] = '\0';
    append_line_to_log(buffer);
  }

  g_log_busy = false;
}