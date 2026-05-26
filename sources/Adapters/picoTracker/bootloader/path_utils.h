/*
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 2026 nILS Podewski
 *
 * This file is part of the picoTracker Boot Manager
 */

#ifndef PICOTRACKER_BOOTLOADER_PATH_UTILS_H
#define PICOTRACKER_BOOTLOADER_PATH_UTILS_H

#include <cstddef>

const char *bl_path_basename(const char *path);
void bl_copy_str(char *dst, size_t dst_size, const char *src);
void bl_append_str(char *dst, size_t dst_size, const char *src);
bool bl_path_has_extension_ci(const char *path, const char *extension);
bool bl_replace_extension_ci(char *path, size_t path_size,
                             const char *extension, const char *replacement);
bool bl_strip_extension_ci(char *path, const char *extension);

#endif
