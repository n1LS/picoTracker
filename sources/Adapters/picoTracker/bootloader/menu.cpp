/*
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 2026 nILS Podewski
 *
 * This file is part of the picoTracker Boot Manager
 */

#include "menu.h"
#include "bootloader_chargfx.h"
#include "path_utils.h"
#include <cstdint>
#include <cstring>

#define GLYPH(x) ((x)[0])

/**
 * Display progress during firmware write
 *
 * @param current Current bytes written
 * @param total Total bytes to write
 */
void menu_show_write_progress(uint32_t current, uint32_t total) {
  (void)current;
  (void)total;
}

int menu_show_firmware_selection(void) { return 0; }

static void render_text(uint8_t x, uint8_t y, const char *s) {
  chargfx_set_cursor(x, y);
  while (*s && x < TEXT_WIDTH) {
    x++;
    chargfx_putc(*s++);
  }
}

// Render `s` starting at (x, y), then pad with spaces out to (x + width) so
// any previous longer content at that row is overwritten.
static void render_text_padded(uint8_t x, uint8_t y, const char *s,
                               uint8_t width) {
  uint8_t col = x;
  const uint8_t end = (x + width < TEXT_WIDTH) ? (x + width) : TEXT_WIDTH;
  chargfx_set_cursor(col, y);
  while (*s && col < end) {
    col++;
    chargfx_putc(*s++);
  }
  while (col < end) {
    col++;
    chargfx_putc(' ');
  }
}

// GRUB-style single-line box with the section label embedded in the top
// border. Cols 0 and TEXT_WIDTH-1 of every row between top_y..bot_y hold the
// vertical sides; the dynamic list renderer must keep cols 1..TEXT_WIDTH-2
// to itself so it doesn't paint over the borders.
static void draw_list_box(uint8_t top_y, uint8_t bot_y) {
  // Top border: ┌───...──┐
  chargfx_set_foreground(CHARGFX_GRAY);
  chargfx_set_cursor(1, top_y);
  chargfx_putc(GLYPH(char_border_single_topLeft_s));

  uint8_t col = 2;
  chargfx_set_foreground(CHARGFX_GRAY);
  while (col < TEXT_WIDTH - 2) {
    chargfx_set_cursor(col++, top_y);
    chargfx_putc(GLYPH(char_border_single_horizontal_s));
  }
  chargfx_set_cursor(TEXT_WIDTH - 2, top_y);
  chargfx_putc(GLYPH(char_border_single_topRight_s));

  // Vertical sides on every row between top and bottom.
  for (uint8_t y = top_y + 1; y < bot_y; ++y) {
    chargfx_set_cursor(1, y);
    chargfx_putc(GLYPH(char_border_single_vertical_s));
    chargfx_set_cursor(TEXT_WIDTH - 2, y);
    chargfx_putc(GLYPH(char_border_single_vertical_s));
  }

  // Bottom border.
  chargfx_set_cursor(1, bot_y);
  chargfx_putc(GLYPH(char_border_single_bottomLeft_s));
  for (uint8_t x = 2; x < TEXT_WIDTH - 2; ++x) {
    chargfx_set_cursor(x, bot_y);
    chargfx_putc(GLYPH(char_border_single_horizontal_s));
  }
  chargfx_set_cursor(TEXT_WIDTH - 2, bot_y);
  chargfx_putc(GLYPH(char_border_single_bottomRight_s));
}

void menu_render_static(void) {
  chargfx_clear(CHARGFX_BG);
  chargfx_set_background(CHARGFX_BLUE);
  chargfx_set_foreground(CHARGFX_WHITE);

  // Title bar — full-width inverted.
  char title[TEXT_WIDTH + 1];
  std::memset(title, ' ', TEXT_WIDTH);
  title[TEXT_WIDTH] = 0;
  const char *t = "pBM - picoTracker Boot Manager";
  std::memcpy(title + 1, t, std::strlen(t));
  render_text(0, 0, title);

  // Static labels.
  chargfx_set_background(CHARGFX_BG);
  chargfx_set_foreground(CHARGFX_GRAY);
  render_text(1, 2, "Installed:");

  // box around the firmware list. Box spans rows 4..23; the
  // dynamic list lives on rows 5..22 inside it.
  draw_list_box(4, 23);

  // Key legend at the bottom
  chargfx_set_foreground(CHARGFX_GRAY);
  render_text(1, 25,
              "  ENTER " char_border_single_vertical_s
              " boot installed application");
  render_text(1, 26,
              "   PLAY " char_border_single_vertical_s
              " import selected firmware");
  render_text(1, 27,
              "DOWN,UP " char_border_single_vertical_s " select application");
  render_text(1, 28,
              "   EDIT " char_border_single_vertical_s
              " update picoTracker firmware");

  chargfx_draw_changed();
}

void menu_render_main(const Uf2FileEntry *uf2_files, int uf2_count,
                      int selected_index, const char *installed_firmware,
                      bool sd_ready, int auto_boot_timeout) {
  // Installed firmware name (row 3). Strip leading '/' and trailing
  // .uf2/.UF2 extension for display.
  char fw_buf[TEXT_WIDTH];
  const char *fw_src = (installed_firmware && installed_firmware[0])
                           ? bl_path_basename(installed_firmware)
                           : "(none)";
  bl_copy_str(fw_buf, sizeof(fw_buf), fw_src);
  (void)bl_strip_extension_ci(fw_buf, ".uf2");
  chargfx_set_foreground(CHARGFX_WHITE);
  render_text_padded(12, 2, fw_buf, TEXT_WIDTH - 4);

  // Firmware list rows 5..19 (14 rows). Every row is repainted (with
  // padding) so transitions between empty/non-empty and between different
  // file counts clean themselves up.
  const int kListRow0 = 6;
  const int kRowsAvail = 16;

  // List content is bounded by the box drawn in menu_render_static: cols 0
  // and TEXT_WIDTH-1 hold the vertical borders, so dynamic content can only
  // touch cols 1..TEXT_WIDTH-2 (width = TEXT_WIDTH - 2).
  const uint8_t kBoxInnerX = 3;
  const uint8_t kBoxInnerWidth = TEXT_WIDTH - 6;

  if (!sd_ready || uf2_count == 0) {
    menu_show_message(sd_ready ? "No firmware found in /firmwares"
                               : "SD card not mounted");
  } else {
    const int shown = uf2_count < kRowsAvail ? uf2_count : kRowsAvail;

    for (int i = 0; i < kRowsAvail; ++i) {
      const bool sel = (i == selected_index);

      if (sel) {
        chargfx_set_background(CHARGFX_BLUE);
        chargfx_set_foreground(CHARGFX_WHITE);
      } else {
        chargfx_set_background(CHARGFX_BG);
        chargfx_set_foreground(CHARGFX_WHITE);
      }

      const uint8_t y = static_cast<uint8_t>(kListRow0 + i);
      if (i >= shown) {
        break;
      }

      // Show the firmware's display name: strip a leading directory and
      // any trailing .bin/.uf2 extension.
      const char *name = bl_path_basename(uf2_files[i].path);
      char name_buf[64];
      bl_copy_str(name_buf, sizeof(name_buf), name);
      if (!bl_strip_extension_ci(name_buf, ".bin")) {
        (void)bl_strip_extension_ci(name_buf, ".uf2");
      }

      // Name occupies cols 3..TEXT_WIDTH-6 (= 34): width 34.
      render_text_padded(3, y, name_buf, static_cast<uint8_t>(kBoxInnerWidth));
    }
  }

  // Auto-boot status row. Keep this on the display; menu_show_message()
  // currently writes to the serial console only.
  chargfx_set_background(CHARGFX_BG);
  chargfx_set_foreground(CHARGFX_YELLOW);

  if (auto_boot_timeout > 0) {
    char text[35] = "Auto-Boot in Xs. Any key to abort.";
    text[13] = '0' + (auto_boot_timeout / 1000);
    menu_show_message(text);
  }

  chargfx_draw_changed();
}

void menu_show_message(const char *message, const char *message2) {
  // Single-line modal box spanning the full width, vertically centered in
  // the list area. The next menu_render_main() will fully overwrite it.
  const uint8_t y_top = 14;
  const uint8_t y_mid = 15;
  const uint8_t y_bot = 16;

  chargfx_set_foreground(CHARGFX_YELLOW);

  // Top border.
  chargfx_set_cursor(1, y_top);
  chargfx_putc(GLYPH(char_border_single_topLeft_s));
  for (uint8_t x = 2; x < TEXT_WIDTH - 2; ++x) {
    chargfx_set_cursor(x, y_top);
    chargfx_putc(GLYPH(char_border_single_horizontal_s));
  }
  chargfx_set_cursor(TEXT_WIDTH - 2, y_top);
  chargfx_putc(GLYPH(char_border_single_topRight_s));

  chargfx_set_cursor(1, y_mid);
  chargfx_putc(GLYPH(char_border_single_vertical_s));

  uint8_t col = 2;
  const uint8_t end = TEXT_WIDTH - 2;
  chargfx_set_cursor(col, y_mid);
  auto write_char = [&](char c) {
    if (col < end) {
      col++;
      chargfx_putc(c);
    }
  };
  write_char(' ');
  while (*message)
    write_char(*message++);
  if (message2) {
    while (*message2)
      write_char(*message2++);
  }
  while (col < end)
    write_char(' ');

  chargfx_set_cursor(TEXT_WIDTH - 2, y_mid);
  chargfx_putc(GLYPH(char_border_single_vertical_s));

  // Bottom border.
  chargfx_set_cursor(1, y_bot);
  chargfx_putc(GLYPH(char_border_single_bottomLeft_s));
  for (uint8_t x = 2; x < TEXT_WIDTH - 2; ++x) {
    chargfx_set_cursor(x, y_bot);
    chargfx_putc(GLYPH(char_border_single_horizontal_s));
  }
  chargfx_set_cursor(TEXT_WIDTH - 2, y_bot);
  chargfx_putc(GLYPH(char_border_single_bottomRight_s));

  chargfx_draw_changed();
}
