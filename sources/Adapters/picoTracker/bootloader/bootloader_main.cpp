/*
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 2026 nILS Podewski
 *
 * This file is part of the picoTracker Boot Manager
 */

#include "../system/input.h"
#include "Adapters/picoTracker/display/chargfx.h"
#include "Adapters/picoTracker/platform/platform.h"
#include "Adapters/picoTracker/sdcard/sdcard.h"
#include "Externals/SdFat/src/SdFat.h"
#include "bsp/board.h"
#include "hardware/clocks.h"
#include "hardware/pll.h"
#include "hardware/structs/watchdog.h"
#include "hardware/watchdog.h"
#include "menu.h"
#include "path_utils.h"
#include "pico/stdlib.h"
#include "pico/time.h"
#include "slot_boot.h"
#include <cstring>

#define APP_SLOT_ADDR 0x10040000u
#define FIRMWARE_DIR "/firmwares"
#define FIRMWARE_INFO_FILE "/firmwares/firmware_info.txt"

constexpr int kMaxUf2Files = 16;
constexpr uint32_t kHeartbeatPeriodMs = 1000;
constexpr bool kEnableAutoBoot = true;
constexpr bool kEnableUsbDeviceTask = false;
constexpr const char *kBootloaderBuildTag = "BLD-2026-05-23-library-v3";
constexpr uint32_t kAppBootTraceMagic = 0x41505452u; // 'APTR'

extern int parse_uf2_and_write_to_flash(const char *filename,
                                        uint32_t target_slot,
                                        const char *derived_output_path,
                                        bool do_flash);

// Convert "/foo.uf2" -> "/firmwares/foo.bin".
static void uf2_to_firmware_bin_path(const char *uf2_path, char *out,
                                     size_t out_size) {
  const char *base = bl_path_basename(uf2_path);
  bl_copy_str(out, out_size, FIRMWARE_DIR);
  bl_append_str(out, out_size, "/");
  bl_append_str(out, out_size, base);
  (void)bl_replace_extension_ci(out, out_size, ".uf2", ".bin");
}

static SdFs g_sd;
bool auto_boot_armed = false;

static bool ensure_firmware_dir() {
  if (g_sd.exists(FIRMWARE_DIR)) {
    return true;
  }
  return g_sd.mkdir(FIRMWARE_DIR, true);
}

static bool write_firmware_info(const char *last_uf2,
                                const char *derived_path) {
  if (!ensure_firmware_dir()) {
    return false;
  }

  FsFile info;
  if (!info.open(FIRMWARE_INFO_FILE, O_WRONLY | O_CREAT | O_TRUNC)) {
    return false;
  }

  info.print("last_uf2=");
  info.print(last_uf2 != nullptr ? last_uf2 : "");
  info.print("\n");
  info.print("derived=");
  info.print(derived_path != nullptr ? derived_path : "");
  info.print("\n");
  info.sync();
  info.close();
  return true;
}

static bool read_firmware_info(char *last_uf2_out, size_t capacity) {
  if (last_uf2_out == nullptr || capacity == 0) {
    return false;
  }

  last_uf2_out[0] = 0;
  FsFile info;
  if (!info.open(FIRMWARE_INFO_FILE, O_RDONLY)) {
    return false;
  }

  char line[64] = {0};
  bool found_uf2 = false;
  while (info.fgets(line, sizeof(line)) > 0) {
    if (std::strncmp(line, "last_uf2=", 9) == 0) {
      std::strncpy(last_uf2_out, line + 9, capacity - 1);
      const size_t len = std::strlen(last_uf2_out);
      if (len > 0 &&
          (last_uf2_out[len - 1] == '\n' || last_uf2_out[len - 1] == '\r')) {
        last_uf2_out[len - 1] = 0;
      }
      found_uf2 = true;
    }
  }

  info.close();
  return found_uf2;
}

static bool has_uf2_extension(const char *name) {
  return bl_path_has_extension_ci(name, ".uf2");
}

static bool mount_sd_card() {
  if (g_sd.begin(SD_CONFIG)) {
    return true;
  }
  if (!g_sd.card() || g_sd.sdErrorCode() != 0) {
    return false;
  }
  return static_cast<FsVolume *>(&g_sd)->begin(g_sd.card(), true, 0);
}

static int scan_uf2_files(Uf2FileEntry *entries, int capacity) {
  if (!g_sd.chdir("/")) {
    return 0;
  }

  FsFile cwd;
  if (!cwd.openCwd()) {
    return 0;
  }

  FsFile entry;
  int count = 0;
  char filename[64];

  while (entry.openNext(&cwd, O_RDONLY) && count < capacity) {
    filename[0] = 0;
    entry.getName(filename, sizeof(filename));

    // Ignore hidden files (dot-prefixed) in the UF2 inbox listing.
    if (!entry.isDirectory() && filename[0] != '.' &&
        has_uf2_extension(filename)) {
      bl_copy_str(entries[count].path, sizeof(entries[count].path), "/");
      bl_append_str(entries[count].path, sizeof(entries[count].path), filename);
      ++count;
    }

    entry.close();
  }

  cwd.close();
  return count;
}

// Scan /firmwares for *.bin files (the imported library).
static int scan_firmware_bins(Uf2FileEntry *entries, int capacity) {
  if (!g_sd.chdir(FIRMWARE_DIR)) {
    return 0;
  }
  FsFile cwd;
  if (!cwd.openCwd()) {
    g_sd.chdir("/");
    return 0;
  }

  FsFile entry;
  int count = 0;
  char filename[64];
  while (entry.openNext(&cwd, O_RDONLY) && count < capacity) {
    filename[0] = 0;
    entry.getName(filename, sizeof(filename));
    const bool is_bin = bl_path_has_extension_ci(filename, ".bin");
    if (!entry.isDirectory() && is_bin) {
      bl_copy_str(entries[count].path, sizeof(entries[count].path),
                  FIRMWARE_DIR);
      bl_append_str(entries[count].path, sizeof(entries[count].path), "/");
      bl_append_str(entries[count].path, sizeof(entries[count].path), filename);
      ++count;
    }
    entry.close();
  }

  cwd.close();
  g_sd.chdir("/");
  return count;
}

// Import new UF2 files from SD root to /firmwares/*.bin if not already present.
static void import_uf2_inbox(const Uf2FileEntry *inbox, int inbox_count) {
  if (inbox_count <= 0) {
    return;
  }
  if (!ensure_firmware_dir()) {
    return;
  }

  char bin_path[80];
  for (int i = 0; i < inbox_count; ++i) {
    uf2_to_firmware_bin_path(inbox[i].path, bin_path, sizeof(bin_path));
    if (g_sd.exists(bin_path)) {
      continue;
    }

    menu_show_message("Converting", inbox[i].path);
    const int rc = parse_uf2_and_write_to_flash(inbox[i].path, APP_SLOT_ADDR,
                                                bin_path, false);
    if (rc != 0) {
      g_sd.remove(bin_path);
    }
  }
}

static void report_app_boot_trace() {
  // Trace::Log("BOOTLOADER", "BOOTDBG: wd_caused=%d scratch2=0x%08x
  // scratch3=0x%08x", watchdog_caused_reboot() ? 1 : 0,
  // watchdog_hw->scratch[2], watchdog_hw->scratch[3]);

  if (watchdog_hw->scratch[2] != kAppBootTraceMagic) {
    return;
  }
  const uint32_t stage = watchdog_hw->scratch[3];
  // Trace::Log("BOOTLOADER", "BOOTDBG: app-trace stage=0x%08x", stage);
}

static void stop_auto_boot() {
  if (auto_boot_armed) {
    menu_render_static();
  }

  auto_boot_armed = false;
}

int main(int argc, char *argv[]) {
  // Initialize microcontroller hardware
  board_init();

  if constexpr (kEnableUsbDeviceTask) {
    // Match regular app init sequence so USB CDC behaves the same.
    tusb_init();
  }

  // Do remaining platform init (clocks, display, GPIO, SD card)
  // NOTE: platform_init() initializes full audio/MIDI setup which we don't need
  // For now, we'll use a simplified init below
  platform_init();

  // platform_init() configures the display SPI/GPIO/PWM but does not bring
  // the ILI9341 controller up; the bootloader has no GUI window to do it,
  // so do it here.
  chargfx_init();
  // Paint the title bar, static labels and key legend once. Subsequent
  // menu_render_main() calls only repaint the dynamic regions.
  menu_render_static();

  int selected_file = 0;
  Uf2FileEntry uf2_files[kMaxUf2Files] = {};
  // Stable-state debounce: a transition is only accepted after the raw
  // input has held the new value for at least kDebounceMs. This is robust
  // against contact bounce on both press AND release, unlike a time-based
  // lockout that can expire mid-bounce.
  uint16_t stable_keys = 0;
  uint16_t pending_keys = 0;
  uint32_t pending_since_ms = 0;
  constexpr uint32_t kDebounceMs = 25;
  bool sd_ready = false;
  uint32_t auto_boot_deadline = 0;
  char installed_firmware[64] = {0};
  bool display_dirty = true;
  int displayed_auto_boot_seconds = -1;

  report_app_boot_trace();

  if (mount_sd_card()) {
    sd_ready = true;
    if (read_firmware_info(installed_firmware, sizeof(installed_firmware))) {
      if constexpr (kEnableAutoBoot) {
        auto_boot_armed = true;
        const uint32_t now_ms = millis();
        auto_boot_deadline = now_ms + 3000;
      }
    }
  } else {
    menu_show_message("SD mount failed. Insert FAT32 SD card.");
  }

  int uf2_count = 0;
  if (sd_ready) {
    const int inbox_count = scan_uf2_files(uf2_files, kMaxUf2Files);
    import_uf2_inbox(uf2_files, inbox_count);

    uf2_count = scan_firmware_bins(uf2_files, kMaxUf2Files);
    selected_file = 0;
    display_dirty = true;
  }

  while (true) {
    if constexpr (kEnableUsbDeviceTask) {
      tud_task();
    }
    const uint32_t now_ms = millis();
    const uint16_t raw_keys = scanKeys();
    if (raw_keys != pending_keys) {
      pending_keys = raw_keys;
      pending_since_ms = now_ms;
    }
    uint16_t pressed = 0;
    if (pending_keys != stable_keys &&
        static_cast<int32_t>(now_ms - pending_since_ms) >=
            static_cast<int32_t>(kDebounceMs)) {
      const uint16_t prev_stable = stable_keys;
      stable_keys = pending_keys;
      pressed = stable_keys & static_cast<uint16_t>(~prev_stable);
    }
    const uint16_t keys = stable_keys;

    if constexpr (kEnableAutoBoot) {
      // Abort auto-boot on any key press.
      if (pressed != 0) {
        stop_auto_boot();
      }

      if (auto_boot_armed &&
          static_cast<int32_t>(now_ms - auto_boot_deadline) >= 0) {
        menu_show_message("Auto-booting app slot...");
        stop_auto_boot();
        if (!boot_firmware_slot(APP_SLOT_ADDR)) {
          menu_show_message(
              "App-slot boot failed. Check flashed firmware image.");
        }
      }
    }

    if ((pressed & KEY_UP) && uf2_count > 0) {
      selected_file = (selected_file - 1 + uf2_count) % uf2_count;
      // Trace::Log("BOOTLOADER", "Selected UF2: %s",
      // uf2_files[selected_file].path);
      display_dirty = true;
    }

    if ((pressed & KEY_DOWN) && uf2_count > 0) {
      selected_file = (selected_file + 1) % uf2_count;
      // Trace::Log("BOOTLOADER", "Selected UF2: %s",
      // uf2_files[selected_file].path);
      display_dirty = true;
    }

    if (pressed & KEY_START) {
      if (uf2_count <= 0) {
        menu_show_message(
            "No firmware in /firmwares. Add a UF2 to SD root, reboot.");
      } else {
        const char *bin_path = uf2_files[selected_file].path;
        const char *base = bl_path_basename(bin_path);
        char source_uf2[80];
        bl_copy_str(source_uf2, sizeof(source_uf2), "/");
        bl_append_str(source_uf2, sizeof(source_uf2), base);
        (void)bl_replace_extension_ci(source_uf2, sizeof(source_uf2), ".bin",
                                      ".uf2");
        // Pre-flash modal so the user sees something happen during
        // the ~1s parse+flash blocking call. Use bare firmware name.
        char display_name[64];
        bl_copy_str(display_name, sizeof(display_name), base);
        (void)bl_strip_extension_ci(display_name, ".bin");
        menu_show_message("Flashing", display_name);

        const int rc = parse_uf2_and_write_to_flash(source_uf2, APP_SLOT_ADDR,
                                                    bin_path, true);
        if (rc == 0) {
          if (!write_firmware_info(source_uf2, bin_path)) {
            menu_show_message(
                "Warning: could not persist firmware_info metadata.");
          }
          menu_show_message("Flash successful. Rebooting...");
          sleep_ms(100);
          platform_reboot();
        } else {
          menu_show_message("Flash failed. See serial log for details.");
        }
      }
    }

    if (pressed & KEY_ENTER) {
      menu_show_message("Booting app slot...");
      // Trace::Log("BOOTLOADER", "BOOTDBG[%s]: handoff(manual) ->
      // boot_firmware_slot(0x%08x)", kBootloaderBuildTag, APP_SLOT_ADDR);
      if (!boot_firmware_slot(APP_SLOT_ADDR)) {
        menu_show_message(
            "App-slot boot failed. Check flashed firmware image.");
        // Trace::Log("BOOTLOADER", "BOOTDBG: handoff(manual) returned
        // failure");
      }
    }

    // reboot to firmware update
    if (pressed & KEY_EDIT) {
      sleep_ms(100);
      platform_bootloader();
    }

    int auto_boot_timeout = -1;
    int auto_boot_seconds = -1;
    if constexpr (kEnableAutoBoot) {
      if (auto_boot_armed) {
        auto_boot_timeout = static_cast<int32_t>(auto_boot_deadline - now_ms);
        if (auto_boot_timeout < 0) {
          auto_boot_timeout = 0;
        }
        auto_boot_seconds = auto_boot_timeout / 1000 + 1;
      }
    }
    if (auto_boot_seconds != displayed_auto_boot_seconds) {
      display_dirty = true;
    }

    if (display_dirty) {
      menu_render_main(uf2_files, uf2_count, selected_file, installed_firmware,
                       sd_ready, auto_boot_timeout);
      display_dirty = false;
      displayed_auto_boot_seconds = auto_boot_seconds;
    }

    tight_loop_contents();
  }

  return 0;
}
