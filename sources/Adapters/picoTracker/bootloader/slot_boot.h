/*
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 2026 nILS Podewski
 *
 * This file is part of the picoTracker Boot Manager
 */

#ifndef PICOTRACKER_BOOTLOADER_SLOT_BOOT_H
#define PICOTRACKER_BOOTLOADER_SLOT_BOOT_H

#include <cstdint>

bool boot_firmware_slot(uint32_t slot_base_address);

#endif
