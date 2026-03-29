/*
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 2024 xiphonics, inc.
 *
 * This file is part of the picoTracker firmware
 *
 * USB Mass Storage Device mode implementation.
 */

#include "msd_mode.h"
#include "Adapters/picoTracker/display/chargfx.h"
#include "Adapters/picoTracker/system/input.h"
#include "Foundation/Constants/SpecialCharacters.h"
#include "hardware/structs/watchdog.h"
#include "hardware/watchdog.h"
#include "pico/stdlib.h"
#include "tusb.h"
#include <stdio.h>
#include <string.h>

// TinyUSB callbacks for connection state tracking
void tud_mount_cb(void) {}

static volatile bool g_usb_suspended = false;

void tud_suspend_cb(bool remote_wakeup_en) {
  (void)remote_wakeup_en;
  g_usb_suspended = true;
}

bool g_msd_mode = false;

bool msd_mode_requested() {
  if (watchdog_hw->scratch[5] == MSD_MAGIC) {
    // Clear the magic so we don't get stuck in MSD mode after a crash
    watchdog_hw->scratch[5] = 0;
    return true;
  }
  return false;
}

#define SCREEN_WIDTH 32
#define SCREEN_HEIGHT 24

// 30x10 box centered on the screen
#define BOX_W 30
#define BOX_H 10
#define BOX_X ((SCREEN_WIDTH - BOX_W) / 2)  // 1
#define BOX_Y ((SCREEN_HEIGHT - BOX_H) / 2) // 7
#define BOX_INNER_X (BOX_X + 1)             // 2
#define BOX_INNER_Y (BOX_Y + 1)             // 8
#define BOX_INNER_W (BOX_W - 2)             // 28
#define BOX_INNER_H (BOX_H - 2)             // 8

// Draw text centered within the box interior at interior row `row` (0-indexed).
// Clears the row before drawing.
static void msd_draw_inner(int row, const char *str) {
  int len = (int)strlen(str);
  if (len > BOX_INNER_W)
    len = BOX_INNER_W;
  int x = BOX_INNER_X + (BOX_INNER_W - len) / 2;
  int y = BOX_INNER_Y + row;
  // Clear the row
  for (int i = 0; i < BOX_INNER_W; i++) {
    chargfx_set_cursor(BOX_INNER_X + i, y);
    chargfx_putc(' ', false);
  }
  // Draw text
  for (int i = 0; i < len; i++) {
    chargfx_set_cursor(x + i, y);
    chargfx_putc(str[i], false);
  }
}

static void msd_draw_box() {
  int x0 = BOX_X, y0 = BOX_Y;
  int x1 = BOX_X + BOX_W - 1, y1 = BOX_Y + BOX_H - 1;

  // Top row
  chargfx_set_cursor(x0, y0);
  chargfx_putc(char_border_single_topLeft, false);
  for (int x = x0 + 1; x < x1; x++) {
    chargfx_set_cursor(x, y0);
    chargfx_putc(char_border_single_horizontal, false);
  }
  chargfx_set_cursor(x1, y0);
  chargfx_putc(char_border_single_topRight, false);

  // Bottom row
  chargfx_set_cursor(x0, y1);
  chargfx_putc(char_border_single_bottomLeft, false);
  for (int x = x0 + 1; x < x1; x++) {
    chargfx_set_cursor(x, y1);
    chargfx_putc(char_border_single_horizontal, false);
  }
  chargfx_set_cursor(x1, y1);
  chargfx_putc(char_border_single_bottomRight, false);

  // Side borders
  for (int y = y0 + 1; y < y1; y++) {
    chargfx_set_cursor(x0, y);
    chargfx_putc(char_border_single_vertical, false);
    chargfx_set_cursor(x1, y);
    chargfx_putc(char_border_single_vertical, false);
  }

  // Separator under title (interior row 1)
  int sep_y = y0 + 2;
  chargfx_set_cursor(x0, sep_y);
  chargfx_putc(char_border_single_verticalRight, false);
  for (int x = x0 + 1; x < x1; x++) {
    chargfx_set_cursor(x, sep_y);
    chargfx_putc(char_border_single_horizontal, false);
  }
  chargfx_set_cursor(x1, sep_y);
  chargfx_putc(char_border_single_verticalLeft, false);
}

static void msd_draw_screen(const char *status) {
  chargfx_clear(CHARGFX_GURU_BG);
  msd_draw_box();
  msd_draw_inner(0, "USB STORAGE DEVICE MODE");
  msd_draw_inner(3, status);                   // center row of interior
  msd_draw_inner(7, "Press any key to exit."); // bottom interior row
  chargfx_draw_screen();
}

void msd_mode_run() {
  // Initialize display
  chargfx_init();
  chargfx_set_font_index(2);
  chargfx_set_palette_color(CHARGFX_GURU_BG, 0x000);  // black
  chargfx_set_palette_color(CHARGFX_GURU_TXT, 0xFF0); // green
  chargfx_set_background(CHARGFX_GURU_BG);
  chargfx_set_foreground(CHARGFX_GURU_TXT);

  msd_draw_screen("Initializing...");

  extern bool msd_sd_init();
  if (!msd_sd_init()) {
    msd_draw_screen("SD card init failed!");
  }

  // Initialize TinyUSB - all slow init (display, SD) is already done,
  // so tud_task() can be serviced immediately in the loop below.
  tusb_init();

  // Estimate mount time from actual FAT table size on the SD card.
  // At Full Speed USB (~700 KB/s effective), total FAT sectors * 512 bytes
  // gives the data volume; divide by throughput for estimated time.
  extern uint32_t msd_sd_get_fat_sectors(void);
  uint32_t fat_sectors = msd_sd_get_fat_sectors();
  uint32_t est_seconds = 0;
  if (fat_sectors > 0) {
    uint32_t fat_kb = fat_sectors / 2; // 512-byte sectors to KB
    est_seconds = fat_kb / 800 + 5;    // ~700 KB/s USB throughput + overhead
  } else {
    // Fallback: rough guess from card size
    extern uint32_t msd_sd_get_sector_count(void);
    uint32_t card_mb = msd_sd_get_sector_count() / 2048;
    est_seconds = (card_mb * 27) / (64 * 1024) + 5;
  }
  if (est_seconds < 5)
    est_seconds = 5;
  if (est_seconds > 180)
    est_seconds = 180;

  // Track I/O activity to detect when mount is complete
  extern volatile uint32_t msd_last_io_time_us;

  uint32_t last_display_s = 0;
  bool mounted = false;
  bool io_started = false;
  uint32_t io_first_time = 0;

  // Main MSD loop - service USB and poll for keypresses.
  bool keys_released = false;

  while (1) {
    tud_task();

    // When the cable is removed, SOF packets stop and the USB controller
    // fires suspend after ~3ms. Reboot back to normal mode.
    if (g_usb_suspended && (io_started || mounted)) {
      watchdog_reboot(0, 0, 0);
      while (1) {
        tight_loop_contents();
      }
    }

    uint32_t now = time_us_32();
    uint32_t now_s = now / 1000000;

    // Update mount detection state on every iteration, not just display
    // updates, so we catch I/O silence gaps that fall between 1-second display
    // ticks.
    uint32_t last_io = msd_last_io_time_us;
    if (last_io != 0 && !io_started) {
      io_started = true;
      io_first_time = last_io;
    }

    // Mount is complete when I/O was active then silent for 500ms
    // OR when the countdown estimate has elapsed.
    uint32_t io_elapsed_s = (now - io_first_time) / 1000000;
    if (io_started && !mounted &&
        (((now - last_io) > 500000) || (io_elapsed_s >= est_seconds))) {
      mounted = true;
    }

    // Update display once per second
    if (now_s != last_display_s) {
      last_display_s = now_s;

      char status[BOX_INNER_W + 1];
      if (!io_started) {
        snprintf(status, sizeof(status), "Waiting for USB...");
      } else if (!mounted) {
        uint32_t remaining = 0;
        if (io_elapsed_s < est_seconds) {
          remaining = est_seconds - io_elapsed_s;
        }
        if (remaining > 0) {
          snprintf(status, sizeof(status), "Mounting (~%lus)...",
                   (unsigned long)remaining);
        } else {
          snprintf(status, sizeof(status), "Mounting...");
        }
      } else {
        snprintf(status, sizeof(status), "Mounted!");
      }
      msd_draw_inner(3, status);
      chargfx_draw_screen();
    }

    uint16_t keys = scanKeys();
    if (!keys_released) {
      if (!keys) {
        keys_released = true;
      }
      continue;
    }

    if (keys) {
      // any key press -> reboot back to normal mode
      tud_disconnect();
      sleep_ms(500);
      watchdog_reboot(0, 0, 0);
      // Should not reach here
      while (1) {
        tight_loop_contents();
      }
    }
  }
}
