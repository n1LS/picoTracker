/*
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 2026 nILS Podewski
 *
 * This file is part of the picoTracker Boot Manager
 */

#include "slot_boot.h"
#include "Adapters/picoTracker/bootloader/bootlog.h"
#include "hardware/structs/nvic.h"
#include "hardware/structs/scb.h"
#include "hardware/structs/systick.h"
#include "hardware/watchdog.h"
#include "pico/stdlib.h"
#include <cstdint>
#include <cstdio>

namespace {
constexpr uint32_t kVectorTableOffset = 0x100u;
constexpr uint32_t kVectorTableOffsetMid = 0x110u;
constexpr uint32_t kVectorTableOffsetAlt = 0x000u;
constexpr uint32_t kSlotSize = 0x007C0000u;
constexpr uint32_t kRamStart = 0x20000000u;
constexpr uint32_t kRamEnd = 0x20042000u;

bool is_valid_stack_ptr(uint32_t stack_ptr) {
  return stack_ptr >= kRamStart && stack_ptr <= kRamEnd;
}

bool is_valid_reset_handler(uint32_t reset_handler,
                            uint32_t slot_base_address) {
  if ((reset_handler & 1u) == 0u) {
    return false;
  }

  const uint32_t reset_handler_addr = reset_handler & ~1u;
  return reset_handler_addr >= slot_base_address &&
         reset_handler_addr < (slot_base_address + kSlotSize);
}
} // namespace

bool boot_firmware_slot(uint32_t slot_base_address) {
  bootlog("BOOT: slot_boot mode=direct-jump-v4-no-cpsid\n");
  const uint32_t vector_table_primary = slot_base_address + kVectorTableOffset;
  uint32_t vector_table = vector_table_primary;
  uint32_t stack_ptr = *reinterpret_cast<const uint32_t *>(vector_table + 0u);
  uint32_t reset_handler =
      *reinterpret_cast<const uint32_t *>(vector_table + 4u);

  if (!is_valid_stack_ptr(stack_ptr) ||
      !is_valid_reset_handler(reset_handler, slot_base_address)) {
    const uint32_t vector_table_mid = slot_base_address + kVectorTableOffsetMid;
    const uint32_t mid_stack_ptr =
        *reinterpret_cast<const uint32_t *>(vector_table_mid + 0u);
    const uint32_t mid_reset_handler =
        *reinterpret_cast<const uint32_t *>(vector_table_mid + 4u);

    if (is_valid_stack_ptr(mid_stack_ptr) &&
        is_valid_reset_handler(mid_reset_handler, slot_base_address)) {
      bootlog("BOOT: using vector table fallback @0x%08x\n", vector_table_mid);
      vector_table = vector_table_mid;
      stack_ptr = mid_stack_ptr;
      reset_handler = mid_reset_handler;
    }
  }

  if (!is_valid_stack_ptr(stack_ptr) ||
      !is_valid_reset_handler(reset_handler, slot_base_address)) {
    const uint32_t vector_table_alt = slot_base_address + kVectorTableOffsetAlt;
    const uint32_t alt_stack_ptr =
        *reinterpret_cast<const uint32_t *>(vector_table_alt + 0u);
    const uint32_t alt_reset_handler =
        *reinterpret_cast<const uint32_t *>(vector_table_alt + 4u);

    if (is_valid_stack_ptr(alt_stack_ptr) &&
        is_valid_reset_handler(alt_reset_handler, slot_base_address)) {
      bootlog("BOOT: using vector table fallback @0x%08x\n", vector_table_alt);
      vector_table = vector_table_alt;
      stack_ptr = alt_stack_ptr;
      reset_handler = alt_reset_handler;
    }
  }

  if (!is_valid_stack_ptr(stack_ptr)) {
    bootlog("BOOT: invalid stack pointer 0x%08x\n", stack_ptr);
    return false;
  }

  if ((reset_handler & 1u) == 0u) {
    bootlog("BOOT: invalid reset handler 0x%08x\n", reset_handler);
    return false;
  }

  const uint32_t reset_handler_addr = reset_handler & ~1u;
  if (reset_handler_addr < slot_base_address ||
      reset_handler_addr >= (slot_base_address + kSlotSize)) {
    bootlog("BOOT: reset handler 0x%08x is outside slot range\n",
            reset_handler_addr);
    return false;
  }

  bootlog("BOOT: jumping to slot image @0x%08x\n", slot_base_address);
  // Cortex-M0+ VTOR requires 256-byte alignment. A misaligned vector table
  // means the app image is linked wrong; silently aligning down would set
  // VTOR to garbage and crash on the first interrupt.
  if ((vector_table & 0xFFu) != 0u) {
    bootlog(
        "BOOT: vector table 0x%08x is not 256-byte aligned; refusing handoff\n",
        vector_table);
    return false;
  }
  const uint32_t aligned_vtor = vector_table;
  bootlog("BOOT: VTOR=0x%08x MSP=0x%08x PC=0x%08x\n", aligned_vtor, stack_ptr,
          reset_handler);

  bootlog("BOOT: direct handoff to PC=0x%08x (thumb) SP=0x%08x\n",
          reset_handler, stack_ptr);
  sleep_ms(30);

  // Stop SysTick and clear pending NVIC state inherited from bootloader.
  systick_hw->csr = 0;
  nvic_hw->icer = 0xFFFFFFFFu;
  nvic_hw->icpr = 0xFFFFFFFFu;
  __asm volatile("dsb" : : : "memory");
  __asm volatile("isb" : : : "memory");

  // Route exceptions to the app vector table before entering app reset.
  scb_hw->vtor = aligned_vtor;
  __asm volatile("dsb" : : : "memory");
  __asm volatile("isb" : : : "memory");

  // Jump into application reset handler with app MSP. Do not mask interrupts:
  // the pico-sdk reset handler expects PRIMASK clear (its normal cold-boot
  // state) and never re-enables IRQs itself, so masking here would silently
  // hang anything interrupt-driven (tusb_init, sleep, etc). We have already
  // disabled SysTick and cleared all NVIC sources above, so no spurious IRQ
  // can fire in the gap before bx.
  __asm volatile("msr msp, %0\n"
                 "bx %1\n"
                 :
                 : "r"(stack_ptr), "r"(reset_handler)
                 : "memory");

  while (true) {
    tight_loop_contents();
  }
}
