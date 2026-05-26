/*
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 2026 nILS Podewski
 *
 * This file is part of the picoTracker Boot Manager
 */

#include "path_utils.h"
#include <cstring>

namespace {

char lower_ascii(char c) {
  if (c >= 'A' && c <= 'Z') {
    return static_cast<char>(c - 'A' + 'a');
  }
  return c;
}

bool equals_ci(const char *a, const char *b, size_t len) {
  for (size_t i = 0; i < len; ++i) {
    if (lower_ascii(a[i]) != lower_ascii(b[i])) {
      return false;
    }
  }
  return true;
}

} // namespace

const char *bl_path_basename(const char *path) {
  if (path == nullptr) {
    return "";
  }
  const char *slash = std::strrchr(path, '/');
  return slash ? slash + 1 : path;
}

void bl_copy_str(char *dst, size_t dst_size, const char *src) {
  if (dst == nullptr || dst_size == 0) {
    return;
  }
  if (src == nullptr) {
    dst[0] = 0;
    return;
  }
  std::strncpy(dst, src, dst_size - 1);
  dst[dst_size - 1] = 0;
}

void bl_append_str(char *dst, size_t dst_size, const char *src) {
  if (dst == nullptr || dst_size == 0 || src == nullptr) {
    return;
  }
  const size_t len = std::strlen(dst);
  if (len >= dst_size - 1) {
    return;
  }
  std::strncpy(dst + len, src, dst_size - len - 1);
  dst[dst_size - 1] = 0;
}

bool bl_path_has_extension_ci(const char *path, const char *extension) {
  if (path == nullptr || extension == nullptr) {
    return false;
  }
  const size_t path_len = std::strlen(path);
  const size_t ext_len = std::strlen(extension);
  if (ext_len == 0 || path_len < ext_len) {
    return false;
  }
  return equals_ci(path + path_len - ext_len, extension, ext_len);
}

bool bl_replace_extension_ci(char *path, size_t path_size,
                             const char *extension, const char *replacement) {
  if (path == nullptr || extension == nullptr || replacement == nullptr ||
      path_size == 0) {
    return false;
  }
  const size_t path_len = std::strlen(path);
  const size_t ext_len = std::strlen(extension);
  const size_t repl_len = std::strlen(replacement);
  if (ext_len == 0 || path_len < ext_len) {
    return false;
  }
  char *tail = path + path_len - ext_len;
  if (!equals_ci(tail, extension, ext_len)) {
    return false;
  }
  if (path_len - ext_len + repl_len >= path_size) {
    return false;
  }
  std::memcpy(tail, replacement, repl_len);
  tail[repl_len] = 0;
  return true;
}

bool bl_strip_extension_ci(char *path, const char *extension) {
  if (path == nullptr || extension == nullptr) {
    return false;
  }
  const size_t path_len = std::strlen(path);
  const size_t ext_len = std::strlen(extension);
  if (ext_len == 0 || path_len < ext_len) {
    return false;
  }
  char *tail = path + path_len - ext_len;
  if (!equals_ci(tail, extension, ext_len)) {
    return false;
  }
  *tail = 0;
  return true;
}
