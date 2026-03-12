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
#include "tusb.h"
#include <SdFat.h>
#include <string.h>

// SD card instance used exclusively in MSD mode
static SdioCard msd_sd_card;
static bool msd_sd_initialized = false;

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
// Return negative value to indicate error e.g unsupported command
int32_t tud_msc_scsi_cb(uint8_t lun, uint8_t const scsi_cmd[16], void *buffer,
                        uint16_t bufsize) {
  (void)lun;
  (void)buffer;
  (void)bufsize;

  // We don't handle any additional SCSI commands
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

  uint32_t block_count = bufsize / 512;
  if (block_count == 0) {
    return -1;
  }

  if (msd_sd_card.readSectors(lba, (uint8_t *)buffer, block_count)) {
    return (int32_t)(block_count * 512);
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

  uint32_t block_count = bufsize / 512;
  if (block_count == 0) {
    return -1;
  }

  if (msd_sd_card.writeSectors(lba, buffer, block_count)) {
    return (int32_t)(block_count * 512);
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
