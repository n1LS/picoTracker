/*
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 2026 nILS Podewski
 *
 * This file is part of the picoTracker Boot Manager
 */

#ifndef PICOTRACKER_BOOTLOADER_MENU_H
#define PICOTRACKER_BOOTLOADER_MENU_H

#include <cstdint>

// One entry in the firmware library list. After boot the list holds paths to
// .bin files under /firmwares; during the import pass it can also be used as
// a scratch list of source .uf2 paths from SD root.
struct Uf2FileEntry {
  char path[64];
};

void menu_show_message(const char *message, const char *message2 = nullptr);
int menu_show_firmware_selection(void);
void menu_show_write_progress(uint32_t current, uint32_t total);

// Paint the title, static labels and the bottom key legend once. The screen
// is cleared as part of this call. Call this exactly once at startup, after
// chargfx_init(), before the first menu_render_main().
void menu_render_static(void);

// Paint only the regions that change at runtime: the installed firmware name
// and the UF2 list. Does not clear and does not redraw any static content.
void menu_render_main(const Uf2FileEntry *uf2_files, int uf2_count,
                      int selected_index, const char *installed_firmware,
                      bool sd_ready, int auto_boot_timeout);

#endif
