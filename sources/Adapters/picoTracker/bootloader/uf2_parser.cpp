/*
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 2026 nILS Podewski
 *
 * This file is part of the picoTracker Boot Manager
 */

#include "Adapters/picoTracker/bootloader/bootlog.h"
#include "Adapters/picoTracker/sdcard/sdcard.h"
#include "Externals/SdFat/src/SdFat.h"
#include "flash_writer.h"
#include "hardware/flash.h"
#include <cstdint>
#include <cstdio>
#include <cstring>

namespace {

constexpr uint32_t UF2_MAGIC_START0 = 0x0A324655u;
constexpr uint32_t UF2_MAGIC_START1 = 0x9E5D5157u;
constexpr uint32_t UF2_MAGIC_END = 0x0AB16F30u;
constexpr uint32_t UF2_BLOCK_SIZE = 512u;
constexpr uint32_t APP_SLOT_SIZE = 0x007C0000u;
constexpr uint32_t kFillChunkSize = 256u;
static SdFs g_sd;
static FsFile g_file;
static FsFile g_derived;
static uint8_t g_uf2_block[UF2_BLOCK_SIZE];
static uint8_t g_flash_page[FLASH_PAGE_SIZE];
static uint8_t g_fill_chunk[kFillChunkSize];

uint32_t read_le32(const uint8_t *p) {
  return static_cast<uint32_t>(p[0]) | (static_cast<uint32_t>(p[1]) << 8u) |
         (static_cast<uint32_t>(p[2]) << 16u) |
         (static_cast<uint32_t>(p[3]) << 24u);
}

bool is_valid_uf2_block(const uint8_t *block) {
  const uint32_t magic0 = read_le32(block + 0);
  const uint32_t magic1 = read_le32(block + 4);
  const uint32_t magic_end = read_le32(block + 508);
  return magic0 == UF2_MAGIC_START0 && magic1 == UF2_MAGIC_START1 &&
         magic_end == UF2_MAGIC_END;
}

bool mount_sd() {
  if (g_sd.begin(SD_CONFIG)) {
    return true;
  }
  if (!g_sd.card() || g_sd.sdErrorCode() != 0) {
    return false;
  }
  return static_cast<FsVolume *>(&g_sd)->begin(g_sd.card(), true, 0);
}

bool validate_uf2_targets_for_app_slot(uint32_t min_addr, uint32_t max_addr,
                                       uint32_t target_slot) {
  const uint32_t slot_start = target_slot;
  const uint32_t slot_end = target_slot + APP_SLOT_SIZE;

  if (min_addr < slot_start || max_addr > slot_end) {
    bootlog("UF2: image address range [0x%08x .. 0x%08x) is outside app slot "
            "[0x%08x .. 0x%08x)\n",
            min_addr, max_addr, slot_start, slot_end);
    bootlog("UF2: this UF2 is not linked for this app slot.\n");
    return false;
  }

  bootlog("UF2: preflight OK for app slot. image range [0x%08x .. 0x%08x)\n",
          min_addr, max_addr);
  return true;
}

int flash_derived_bin_to_slot(const char *bin_path, uint32_t target_slot) {
  if (!mount_sd()) {
    bootlog("BIN: SD mount failed\n");
    return -1;
  }

  FsFile bin_file;
  if (!bin_file.open(bin_path, O_RDONLY)) {
    bootlog("BIN: failed to open %s\n", bin_path);
    return -1;
  }

  const uint32_t bin_size = static_cast<uint32_t>(bin_file.fileSize());
  if (bin_size == 0 || bin_size > APP_SLOT_SIZE) {
    bootlog("BIN: invalid size %u for %s\n", bin_size, bin_path);
    bin_file.close();
    return -1;
  }

  if (erase_firmware_range(target_slot, bin_size) != 0) {
    bootlog("BIN: erase failed for slot 0x%08x size %u\n", target_slot,
            bin_size);
    bin_file.close();
    return -1;
  }

  uint32_t offset = 0;
  while (offset < bin_size) {
    std::memset(g_flash_page, 0xFF, sizeof(g_flash_page));

    const uint32_t remaining = bin_size - offset;
    const uint32_t to_read =
        (remaining < FLASH_PAGE_SIZE) ? remaining : FLASH_PAGE_SIZE;
    const int read_bytes =
        bin_file.read(g_flash_page, static_cast<size_t>(to_read));
    if (read_bytes != static_cast<int>(to_read)) {
      bootlog("BIN: short read at offset %u (expected %u, got %d)\n", offset,
              to_read, read_bytes);
      bin_file.close();
      return -1;
    }

    const uint32_t absolute = target_slot + offset;
    if (write_firmware_chunk(absolute, g_flash_page, FLASH_PAGE_SIZE) != 0) {
      bootlog("BIN: write failed at 0x%08x\n", absolute);
      bin_file.close();
      return -1;
    }

    if (verify_firmware_chunk(absolute, g_flash_page, FLASH_PAGE_SIZE) != 0) {
      bootlog("BIN: verify failed at 0x%08x\n", absolute);
      bin_file.close();
      return -1;
    }

    offset += to_read;
  }

  bin_file.close();
  bootlog("BIN: flashed %u bytes from %s to app slot 0x%08x\n", bin_size,
          bin_path, target_slot);
  return 0;
}

} // namespace

// Parse UF2 from SD, validate slot range, write derived .bin, and
// optionally flash app slot.
int parse_uf2_and_write_to_flash(const char *filename, uint32_t target_slot,
                                 const char *derived_output_path,
                                 bool do_flash) {
  if (do_flash) {
    return flash_derived_bin_to_slot(derived_output_path, target_slot);
  }

  bootlog("UF2: converting %s -> %s\n", filename, derived_output_path);

  if (!mount_sd()) {
    bootlog("UF2: SD mount failed\n");
    return -1;
  }

  if (!g_file.open(filename, O_RDONLY)) {
    bootlog("UF2: failed to open %s\n", filename);
    return -1;
  }

  uint32_t min_addr = 0xFFFFFFFFu;
  uint32_t max_addr = 0u;
  uint32_t block_count = 0;

  while (g_file.read(g_uf2_block, UF2_BLOCK_SIZE) ==
         static_cast<int>(UF2_BLOCK_SIZE)) {
    if (!is_valid_uf2_block(g_uf2_block)) {
      continue;
    }

    const uint32_t target_addr = read_le32(g_uf2_block + 12);
    const uint32_t payload_size = read_le32(g_uf2_block + 16);

    if (payload_size == 0 || payload_size > 476u) {
      bootlog("UF2: found a broken block payload size");
      continue;
    }

    if (target_addr < min_addr) {
      min_addr = target_addr;
    }
    if (target_addr + payload_size > max_addr) {
      max_addr = target_addr + payload_size;
    }
    ++block_count;
  }

  if (block_count == 0 || min_addr == 0xFFFFFFFFu || max_addr <= min_addr) {
    bootlog("UF2: no valid UF2 blocks");
    g_file.close();
    return -1;
  }

  const uint32_t image_size = max_addr - min_addr;
  const uint32_t derived_size = max_addr - target_slot;
  if (image_size > APP_SLOT_SIZE) {
    bootlog("UF2: image too large (%u bytes)\n", image_size);
    g_file.close();
    return -1;
  }

  if (derived_size == 0 || derived_size > APP_SLOT_SIZE) {
    bootlog("UF2: invalid derived size (%u bytes)\n", derived_size);
    g_file.close();
    return -1;
  }

  if (!validate_uf2_targets_for_app_slot(min_addr, max_addr, target_slot)) {
    g_file.close();
    return -1;
  }

  if (!g_derived.open(derived_output_path, O_WRONLY | O_CREAT | O_TRUNC)) {
    bootlog("UF2: failed to create derived artifact %s\n", derived_output_path);
    g_file.close();
    return -1;
  }

  std::memset(g_fill_chunk, 0xFF, sizeof(g_fill_chunk));
  uint32_t remaining_fill = derived_size;
  while (remaining_fill > 0) {
    const uint32_t chunk = (remaining_fill < sizeof(g_fill_chunk))
                               ? remaining_fill
                               : sizeof(g_fill_chunk);
    if (g_derived.write(g_fill_chunk, chunk) != chunk) {
      bootlog("UF2: failed pre-filling derived artifact\n");
      g_derived.close();
      g_file.close();
      return -1;
    }
    remaining_fill -= chunk;
  }

  g_file.rewind();
  while (g_file.read(g_uf2_block, UF2_BLOCK_SIZE) ==
         static_cast<int>(UF2_BLOCK_SIZE)) {
    if (!is_valid_uf2_block(g_uf2_block)) {
      continue;
    }

    const uint32_t target_addr = read_le32(g_uf2_block + 12);
    const uint32_t payload_size = read_le32(g_uf2_block + 16);
    if (payload_size == 0 || payload_size > 476u) {
      continue;
    }

    const uint32_t relative = target_addr - target_slot;

    if (relative + payload_size > APP_SLOT_SIZE) {
      bootlog("UF2: invalid app-slot range at 0x%08x\n", target_addr);
      g_derived.close();
      g_file.close();
      return -1;
    }

    std::memset(g_flash_page, 0xFF, sizeof(g_flash_page));
    if (payload_size > FLASH_PAGE_SIZE) {
      bootlog("UF2: payload %u exceeds flash page size\n", payload_size);
      g_derived.close();
      g_file.close();
      return -1;
    }

    if (!g_derived.seekSet(relative) ||
        g_derived.write(g_uf2_block + 32, payload_size) != payload_size) {
      bootlog("UF2: failed writing derived artifact at offset %u\n", relative);
      g_derived.close();
      g_file.close();
      return -1;
    }
  }

  g_derived.sync();
  g_derived.close();
  g_file.close();
  bootlog("UF2: imported %u blocks from %s\n", block_count, filename);
  bootlog("UF2: derived artifact created at %s\n", derived_output_path);
  return 0;
}
