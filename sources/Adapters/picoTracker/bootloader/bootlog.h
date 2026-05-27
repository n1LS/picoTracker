/*
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 2026 nILS Podewski
 *
 * This file is part of the picoTracker Boot Manager
 */

#ifndef PICOTRACKER_BOOTLOADER_BOOTLOG_H
#define PICOTRACKER_BOOTLOADER_BOOTLOG_H

#ifdef __cplusplus
extern "C" {
#endif

void bootlog(const char *fmt, ...);

#ifdef __cplusplus
}
#endif

#endif