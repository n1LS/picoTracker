/*
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 2024 xiphonics, inc.
 *
 * This file is part of the picoTracker firmware
 *
 * TinyUSB MSC (Mass Storage Class) SCSI callbacks for USB Mass Storage mode.
 * These callbacks are invoked by the TinyUSB stack when the device is
 * enumerated as a mass storage device.
 */

#include "Adapters/picoTracker/sdcard/sdcard.h"
#include "msd_mode.h"
#include "pico/time.h"
#include "tusb.h"
#include <SdFat.h>
#include <string.h>

// SD card instance used exclusively in MSD mode
static SdioCard msd_sd_card;
static bool msd_sd_initialized = false;

// Timestamp of last USB I/O activity (read or write callback)
volatile uint32_t msd_last_io_time_us = 0;

// Initialize the SD card for MSD mode
// Called once before the USB loop starts
extern "C" bool msd_sd_init(void) {
  SdioConfig config(DMA_SDIO);
  if (msd_sd_card.begin(config)) {
    msd_sd_initialized = true;
    return true;
  }
  return false;
}

extern "C" uint32_t msd_sd_get_sector_count(void) {
  if (msd_sd_initialized) {
    return msd_sd_card.sectorCount();
  }
  return 0;
}

// Read the boot sector to extract FAT size info for mount time estimation.
// Returns estimated total FAT+metadata sectors the host needs to read.
// Handles both FAT32 (two FAT copies ~16MB on 64GB) and exFAT (one FAT ~2MB).
extern "C" uint32_t msd_sd_get_fat_sectors(void) {
  if (!msd_sd_initialized)
    return 0;

  uint8_t buf[512] __attribute__((aligned(4)));

  // First read MBR (sector 0) to find partition start
  if (!msd_sd_card.readSector(0, buf))
    return 0;

  // Check for MBR signature
  uint32_t part_start = 0;
  if (buf[510] == 0x55 && buf[511] == 0xAA && buf[450] != 0x00) {
    // Read partition start LBA from first partition entry
    part_start = (uint32_t)buf[454] | ((uint32_t)buf[455] << 8) |
                 ((uint32_t)buf[456] << 16) | ((uint32_t)buf[457] << 24);
  }

  // Read the volume boot sector
  if (!msd_sd_card.readSector(part_start, buf))
    return 0;

  // Detect exFAT by OEM Name field at offset 3 ("EXFAT   ")
  if (memcmp(buf + 3, "EXFAT   ", 8) == 0) {
    // exFAT VBR: ClusterCount at offset 76 (4 bytes)
    // FAT is one copy, sized ClusterCount * 4 bytes
    uint32_t cluster_count = (uint32_t)buf[76] | ((uint32_t)buf[77] << 8) |
                             ((uint32_t)buf[78] << 16) |
                             ((uint32_t)buf[79] << 24);
    return (cluster_count * 4 + 511) / 512;
  }

  // FAT32 BPB: sectors per FAT at offset 36 (4 bytes)
  uint32_t fat_size = (uint32_t)buf[36] | ((uint32_t)buf[37] << 8) |
                      ((uint32_t)buf[38] << 16) | ((uint32_t)buf[39] << 24);
  // Number of FATs at offset 16 (usually 2)
  uint8_t num_fats = buf[16];
  if (num_fats == 0)
    num_fats = 2;

  // Total sectors host needs to read: both FAT copies + some overhead
  return fat_size * num_fats;
}

extern "C" {

// Invoked when received SCSI_CMD_INQUIRY
// Vendor id, product id, and revision string are filled in
void tud_msc_inquiry_cb(uint8_t lun, uint8_t vendor_id[8],
                        uint8_t product_id[16], uint8_t product_rev[4]) {
  (void)lun;

  const char vid[] = "picoTrkr";
  const char pid[] = "SD Card";
  const char rev[] = "1.0";

  memcpy(vendor_id, vid, 8);
  memcpy(product_id, pid, strlen(pid));
  memcpy(product_rev, rev, strlen(rev));
}

// Invoked when received Test Unit Ready command
// Return true if the device is ready, false otherwise
bool tud_msc_test_unit_ready_cb(uint8_t lun) {
  (void)lun;
  return msd_sd_initialized;
}

// Invoked when received SCSI_CMD_READ_CAPACITY_10 and
// SCSI_CMD_READ_FORMAT_CAPACITY
void tud_msc_capacity_cb(uint8_t lun, uint32_t *block_count,
                         uint16_t *block_size) {
  (void)lun;

  if (msd_sd_initialized) {
    *block_count = msd_sd_card.sectorCount();
    *block_size = 512;
  } else {
    *block_count = 0;
    *block_size = 0;
  }
}

// Invoked when received an SCSI command not in the built-in list
int32_t tud_msc_scsi_cb(uint8_t lun, uint8_t const scsi_cmd[16], void *buffer,
                        uint16_t bufsize) {
  (void)lun;
  (void)buffer;
  (void)bufsize;

  tud_msc_set_sense(lun, SCSI_SENSE_ILLEGAL_REQUEST, 0x20, 0x00);
  return -1;
}

// Callback invoked when received READ10 command.
// Copy disk's data to buffer (up to bufsize) and return number of copied bytes.
int32_t tud_msc_read10_cb(uint8_t lun, uint32_t lba, uint32_t offset,
                          void *buffer, uint32_t bufsize) {
  (void)lun;
  (void)offset;

  if (!msd_sd_initialized) {
    return -1;
  }

  uint32_t num_sectors = bufsize / 512;
  if (num_sectors == 0) {
    return -1;
  }

  // Read directly into the TinyUSB buffer.
  // The buffer is 4-byte aligned and DMA write completes before
  // TinyUSB initiates the USB transfer.
  uint32_t bytes_to_read = num_sectors * 512;

  bool ok;
  if (num_sectors == 1) {
    ok = msd_sd_card.readSector(lba, (uint8_t *)buffer);
  } else {
    ok = msd_sd_card.readSectors(lba, (uint8_t *)buffer, num_sectors);
  }

  if (ok) {
    msd_last_io_time_us = time_us_32();
    return (int32_t)bytes_to_read;
  }

  return -1;
}

// Callback invoked when received WRITE10 command.
// Process data in buffer to disk's storage and return number of written bytes.
int32_t tud_msc_write10_cb(uint8_t lun, uint32_t lba, uint32_t offset,
                           uint8_t *buffer, uint32_t bufsize) {
  (void)lun;
  (void)offset;

  if (!msd_sd_initialized) {
    return -1;
  }

  uint32_t num_sectors = bufsize / 512;
  if (num_sectors == 0) {
    return -1;
  }

  uint32_t bytes_to_write = num_sectors * 512;

  bool ok;
  if (num_sectors == 1) {
    ok = msd_sd_card.writeSector(lba, (const uint8_t *)buffer);
  } else {
    ok = msd_sd_card.writeSectors(lba, (const uint8_t *)buffer, num_sectors);
  }

  if (ok) {
    msd_last_io_time_us = time_us_32();
    return (int32_t)bytes_to_write;
  }

  return -1;
}

// Invoked when received Start Stop Unit command
// - Start = 0 : stopped power mode, if load_eject = 1 : unload disk storage
// - Start = 1 : active mode, if load_eject = 1 : load disk storage
bool tud_msc_start_stop_cb(uint8_t lun, uint8_t power_condition, bool start,
                           bool load_eject) {
  (void)lun;
  (void)power_condition;
  (void)start;
  (void)load_eject;
  return true;
}

} // extern "C"
