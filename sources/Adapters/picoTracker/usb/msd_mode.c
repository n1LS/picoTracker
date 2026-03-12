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
#include "hardware/structs/watchdog.h"
#include "hardware/watchdog.h"
#include "pico/stdlib.h"
#include "tusb.h"
#include <string.h>

bool g_msd_mode = false;

bool msd_mode_requested(void) {
  if (watchdog_hw->scratch[5] == MSD_MAGIC) {
    // Clear the magic so we don't get stuck in MSD mode after a crash
    watchdog_hw->scratch[5] = 0;
    return true;
  }
  return false;
}

void msd_mode_run(void) {
  // Initialize display
  chargfx_init();
  chargfx_set_font_index(0);

  chargfx_set_palette_color(15, 0x0000); // BLACK
  chargfx_set_palette_color(14, 0x07E0); // GREEN

  chargfx_set_background(CHARGFX_GURU_BG);
  chargfx_set_foreground(CHARGFX_GURU_TXT);

  chargfx_clear(CHARGFX_BG);

  // Draw "USB Storage Mode" centered on screen
  const char *line1 = "USB Storage Mode";
  const char *line2 = "Press any key to exit";

  int len1 = strlen(line1);
  int center1 = (32 - len1) / 2;

  int len2 = strlen(line2);
  int center2 = (32 - len2) / 2;

  chargfx_set_foreground(CHARGFX_GURU_TXT);
  chargfx_set_background(CHARGFX_BG);

  chargfx_set_cursor(center1, 10);
  chargfx_print(line1, false);

  chargfx_set_cursor(center2, 12);
  chargfx_print(line2, false);

  chargfx_draw_screen();

  // Main MSD loop - service USB and poll for keypresses
  while (1) {
    tud_task();

    uint16_t keys = scanKeys();
    if (keys) {
      // Any key pressed - reboot back to normal mode
      watchdog_reboot(0, 0, 0);
      // Should not reach here
      while (1) {
        tight_loop_contents();
      }
    }
  }
}
