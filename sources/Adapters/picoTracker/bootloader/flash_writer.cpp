/*
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 2026 nILS Podewski
 *
 * This file is part of the picoTracker firmware
 */

#include "hardware/flash.h"
#include "hardware/sync.h"
#include <cstdint>

#define APP_SLOT_ADDR 0x10040000u
#define APP_SLOT_SIZE 0x007C0000u

static bool is_in_slot_range(uint32_t absolute_address, uint32_t length) {
  if (length == 0) {
    return false;
  }

  const uint32_t write_end = absolute_address + length;
  if (write_end < absolute_address) {
    return false;
  }

  const uint32_t slot_start = APP_SLOT_ADDR;
  const uint32_t slot_end = APP_SLOT_ADDR + APP_SLOT_SIZE;

  if (absolute_address < slot_start || write_end > slot_end) {
    return false;
  }

  return true;
}

int erase_firmware_range(uint32_t slot_address, uint32_t image_size) {
  if (image_size == 0 || image_size > APP_SLOT_SIZE) {
    return -1;
  }

  if (slot_address < APP_SLOT_ADDR ||
      slot_address >= (APP_SLOT_ADDR + APP_SLOT_SIZE)) {
    return -1;
  }

  if (!is_in_slot_range(slot_address, image_size)) {
    return -1;
  }

  const uint32_t erase_start = slot_address & ~(FLASH_SECTOR_SIZE - 1u);
  const uint32_t erase_end =
      (slot_address + image_size + (FLASH_SECTOR_SIZE - 1u)) &
      ~(FLASH_SECTOR_SIZE - 1u);
  const uint32_t erase_size = erase_end - erase_start;

  const uint32_t flash_offset = erase_start - XIP_BASE;
  uint32_t irq_state = save_and_disable_interrupts();
  flash_range_erase(flash_offset, erase_size);
  restore_interrupts(irq_state);

  return 0;
}

int write_firmware_chunk(uint32_t absolute_address, const uint8_t *data,
                         uint32_t length) {
  if (data == nullptr || length == 0) {
    return -1;
  }

  if ((length % FLASH_PAGE_SIZE) != 0) {
    return -1;
  }

  if ((absolute_address % FLASH_PAGE_SIZE) != 0) {
    return -1;
  }

  if (!is_in_slot_range(absolute_address, length)) {
    return -1;
  }

  const uint32_t flash_offset = absolute_address - XIP_BASE;
  uint32_t irq_state = save_and_disable_interrupts();
  flash_range_program(flash_offset, data, length);
  restore_interrupts(irq_state);
  return 0;
}

int verify_firmware_chunk(uint32_t absolute_address, const uint8_t *data,
                          uint32_t length) {
  if (data == nullptr || length == 0) {
    return -1;
  }

  if (!is_in_slot_range(absolute_address, length)) {
    return -1;
  }

  const uint8_t *flash_ptr =
      reinterpret_cast<const uint8_t *>(absolute_address);
  for (uint32_t i = 0; i < length; ++i) {
    if (flash_ptr[i] != data[i]) {
      return -1;
    }
  }

  return 0;
}
