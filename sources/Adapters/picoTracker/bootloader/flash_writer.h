/*
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 2026 nILS Podewski
 *
 * This file is part of the picoTracker Boot Manager
 */

#ifndef PICOTRACKER_BOOTLOADER_FLASH_WRITER_H
#define PICOTRACKER_BOOTLOADER_FLASH_WRITER_H

#include <cstdint>

int erase_firmware_range(uint32_t slot_address, uint32_t image_size);
int write_firmware_chunk(uint32_t absolute_address, const uint8_t *data,
                         uint32_t length);
int verify_firmware_chunk(uint32_t absolute_address, const uint8_t *data,
                          uint32_t length);

#endif
