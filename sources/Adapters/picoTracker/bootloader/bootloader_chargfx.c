/*
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 2024 xiphonics, inc.
 *
 * This file is part of the picoTracker firmware
 */

#include "bootloader_chargfx.h"
#include "bootloader_font.h"
#include "hardware/spi.h"
#include "pico/stdlib.h"
#include <stdio.h>
#include <string.h>

/* Character graphics mode */

#define SWAP_BYTES(color) ((uint16_t)(color >> 8) | (uint16_t)(color << 8))
#define BUFFER_CHARS 15
static chargfx_color_t screen_bg_color = CHARGFX_BG;
static chargfx_color_t screen_fg_color = CHARGFX_NORMAL;
static int cursor_x = 0;
static int cursor_y = 0;
uint8_t screen[TEXT_HEIGHT * TEXT_WIDTH] = {0};
uint8_t colors[TEXT_HEIGHT * TEXT_WIDTH] = {0};
uint16_t buffer[CHAR_HEIGHT * CHAR_WIDTH * BUFFER_CHARS] = {0};

// Using a bit array in order to save memory, there is a slight performance
// hit in doing so vs a bool array
static uint8_t changed[TEXT_HEIGHT * TEXT_WIDTH / 8] = {0};
#define SetBit(A, k) (A[(k) / 8] |= (1 << ((k) % 8)))
#define ClearBit(A, k) (A[(k) / 8] &= ~(1 << ((k) % 8)))
#define TestBit(A, k) (A[(k) / 8] & (1 << ((k) % 8)))

// Default palette, can be redefined
uint16_t palette[16] = {
    SWAP_BYTES(0x0000), SWAP_BYTES(0x49E5), SWAP_BYTES(0xB926),
    SWAP_BYTES(0xE371), SWAP_BYTES(0x9CF3), SWAP_BYTES(0xA324),
    SWAP_BYTES(0xEC46), SWAP_BYTES(0xF70D), SWAP_BYTES(0xffff),
    SWAP_BYTES(0x1926), SWAP_BYTES(0x2A49), SWAP_BYTES(0x4443),
    SWAP_BYTES(0xA664), SWAP_BYTES(0x02B0), SWAP_BYTES(0x351E),
    SWAP_BYTES(0xB6FD)};

void chargfx_clear(chargfx_color_t color) {
  int size = TEXT_WIDTH * TEXT_HEIGHT;
  memset(screen, 0, size);
  memset(colors, color, size);
  chargfx_set_cursor(0, 0);
  chargfx_draw_screen();
}

void chargfx_set_foreground(chargfx_color_t color) { screen_fg_color = color; }

void chargfx_set_background(chargfx_color_t color) { screen_bg_color = color; }

void chargfx_set_cursor(uint8_t x, uint8_t y) {
  cursor_x = x;
  cursor_y = y;
}

uint8_t chargfx_get_cursor_x() { return cursor_x; }

uint8_t chargfx_get_cursor_y() { return cursor_y; }

void chargfx_putc(char c) {
  if (cursor_x < 0 || cursor_x >= TEXT_WIDTH || cursor_y < 0 ||
      cursor_y >= TEXT_HEIGHT) {
    return;
  }

  int idx = cursor_y * TEXT_WIDTH + cursor_x;
  if (c >= 32) {
    screen[idx] = c - 32;
    SetBit(changed, idx);
    colors[idx] = ((screen_fg_color & 0xf) << 4) | (screen_bg_color & 0xf);
  }

  if (cursor_x + 1 < TEXT_WIDTH) {
    cursor_x++;
  }
}

void chargfx_print(const char *str) {
  char c;
  while ((c = *str++)) {
    chargfx_putc(c);
  }
}

void chargfx_write(const char *str, int len) {
  for (int i = 0; i < len; i++) {
    chargfx_putc(*str++);
  }
}

static inline void chargfx_set_window_for_region(uint16_t screen_x,
                                                 uint16_t screen_y,
                                                 uint16_t screen_width,
                                                 uint16_t screen_height) {
  // column address set
  ili9341_set_command(ILI9341_CASET);
  ili9341_command_param16(screen_y);
  ili9341_command_param16(screen_y + screen_height - 1);

  // page address set
  ili9341_set_command(ILI9341_PASET);
  ili9341_command_param16(screen_x);
  ili9341_command_param16(screen_x + screen_width - 1);

  // start writing
  ili9341_set_command(ILI9341_RAMWR);
}

static inline void chargfx_rasterize_char_column(uint8_t char_col,
                                                 uint8_t row_start,
                                                 uint8_t row_count,
                                                 uint16_t *dst) {
  for (int glyph_bit = CHAR_WIDTH - 1; glyph_bit >= 0; glyph_bit--) {
    uint16_t glyph_mask = 1 << (CHAR_WIDTH - 1 - glyph_bit);

    for (int char_row = row_start + row_count - 1; char_row >= row_start;
         char_row--) {
      int16_t cell_idx = char_row * TEXT_WIDTH + char_col;
      uint8_t glyph_index = screen[cell_idx];
      uint16_t fg_rgb565 = palette[colors[cell_idx] >> 4];
      uint16_t bg_rgb565 = palette[colors[cell_idx] & 0xf];

      const uint8_t *glyph_rows =
          (const uint8_t *)(font + char_map[glyph_index]);

      // draw the character into the output buffer
      for (int pixel_row = CHAR_HEIGHT - 1; pixel_row >= 0; pixel_row--) {
        uint16_t glyph_row_bits = glyph_rows[pixel_row];
        *dst++ = (glyph_row_bits & glyph_mask) ? fg_rgb565 : bg_rgb565;
      }
    }
  }
}

void chargfx_draw_region(uint8_t x, uint8_t y, uint8_t width, uint8_t height) {
  int remaining_rows = height;
  while (remaining_rows) {
    int chunk_rows =
        (remaining_rows > BUFFER_CHARS) ? BUFFER_CHARS : remaining_rows;
    uint8_t chunk_y = y + height - remaining_rows;
    remaining_rows -= chunk_rows;
    chargfx_draw_sub_region(x, chunk_y, width, chunk_rows);
  }
}

// NOTE: we make life easier for ourselves by using the LCD controllers
// orientation command to let us treat the x,y coords passed into this function
// as the visual x & y instead of trying to transform them to the LCDs physical
// x,y coords to compensate for the fact that on the picoTracker the screen is
// mounted rotated 90deg clockwise, ie. the "bottom" of the LCD with the flex
// pcb connector is actually on the left instead of its normal orientation of
// being mounted on the bottom of the LCD
void chargfx_fill_rect(uint8_t color_index, uint16_t x, uint16_t y,
                       uint16_t width, uint16_t height) {
  // Get the RGB565 color from the current foreground palette index
  uint16_t color = palette[color_index];

  // Clip the rectangle to the screen dimensions
  if (x >= ILI9341_TFTHEIGHT || y >= ILI9341_TFTWIDTH) {
    return;
  }
  if (x + width > ILI9341_TFTHEIGHT) {
    width = ILI9341_TFTHEIGHT - x;
  }
  if (y + height > ILI9341_TFTWIDTH) {
    height = ILI9341_TFTWIDTH - y;
  }

  // display_x is from right hand edge and since the picoTracker LCD is mounted
  // rotated 90deg clockwise, the LCDs "physical height" is actually visually
  // speaking the width
  uint16_t display_x = ILI9341_TFTHEIGHT - x - width;
#ifdef LCD_ST7789
  uint16_t display_y = y;
#else
  uint16_t display_y = ILI9341_TFTWIDTH - y - height;
#endif
  uint16_t display_w = width;
  uint16_t display_h = height;

  // Set rotation for rectangle drawing
  ili9341_set_command(ILI9341_MADCTL);
  ili9341_command_param(0x28); // 90-degree clockwise rotation

  // Set display window
  ili9341_set_command(ILI9341_CASET);
  ili9341_command_param16(display_x);
  ili9341_command_param16(display_x + display_w - 1);

  ili9341_set_command(ILI9341_PASET);
  ili9341_command_param16(display_y);
  ili9341_command_param16(display_y + display_h - 1);

  ili9341_set_command(ILI9341_RAMWR);
  ili9341_start_writing();

  // just use the char cell buffer for our line buffer as its more than big
  // enough
  for (uint16_t i = 0; i < display_w; i++) {
    buffer[i] = color;
  }

  // Write the buffer for each column
  for (uint16_t i = 0; i < display_h; i++) {
    ili9341_write_data_continuous(buffer, display_w * sizeof(uint16_t));
  }

  ili9341_stop_writing();

  // Restore original rotation
  ili9341_set_command(ILI9341_MADCTL);
  ili9341_command_param(LCD_MADCTL_DEFAULT);
}

inline void chargfx_draw_sub_region(uint8_t x, uint8_t y, uint8_t width,
                                    uint8_t height) {
  assert(height <= BUFFER_CHARS);

  uint16_t screen_x = x * CHAR_WIDTH;
  uint16_t screen_y = (TEXT_HEIGHT - height - y) * CHAR_HEIGHT;
  uint16_t screen_width = width * CHAR_WIDTH;
  uint16_t screen_height = height * CHAR_HEIGHT;

  chargfx_set_window_for_region(screen_x, screen_y, screen_width,
                                screen_height);

  ili9341_start_writing();

  for (int char_col = x; char_col < x + width; char_col++) {
    // create one column of screen information
    uint16_t *buffer_idx = buffer;

    chargfx_rasterize_char_column(char_col, y, height, buffer_idx);

    ili9341_write_data_continuous(buffer,
                                  CHAR_WIDTH * screen_height * sizeof(int16_t));
  }
  ili9341_stop_writing();
}

void chargfx_draw_changed() {
  for (int idx = 0; idx < TEXT_HEIGHT * TEXT_WIDTH; idx++) {
    if (TestBit(changed, idx)) {
      ClearBit(changed, idx);
      // check adjacent in order to find bigger rectangle
      uint16_t y = idx / TEXT_WIDTH;
      uint16_t x = idx - (TEXT_WIDTH * y);

      int height = 1;
      // first pass tests the height
      for (int probe_y = y + 1; probe_y < TEXT_HEIGHT; probe_y++) {
        int probe_idx = probe_y * TEXT_WIDTH + x;
        if (TestBit(changed, probe_idx)) {
          ClearBit(changed, probe_idx);
          height++;
          continue;
        }
        break;
      }

      int16_t width = 1;
      // having the height, we can test every subsequent column
      for (int probe_x = x + 1; probe_x < TEXT_WIDTH; probe_x++) {
        for (int probe_y = y; probe_y < y + height; probe_y++) {
          // if we don't get to max height, then abort
          int probe_idx = probe_y * TEXT_WIDTH + probe_x;
          if (!TestBit(changed, probe_idx)) {
            // undo last column
            for (int undo_y = y; undo_y < probe_y; undo_y++) {
              SetBit(changed, undo_y * TEXT_WIDTH + probe_x);
            }
            goto end;
          }
          ClearBit(changed, probe_idx);
        }
        width++;
      }
    end:
      chargfx_draw_region(x, y, width, height);
    }
  }
}

void chargfx_draw_changed_simple() {
  // This method is better (faster) for fewer characters changed
  for (int idx = 0; idx < TEXT_HEIGHT * TEXT_WIDTH; idx++) {
    if (TestBit(changed, idx)) {
      ClearBit(changed, idx);
      uint16_t y = idx / TEXT_WIDTH;
      uint16_t x = idx - (TEXT_WIDTH * y);
      chargfx_draw_region(x, y, 1, 1);
    }
  }
}

void chargfx_draw_screen() {
  // draw the whole screen
  chargfx_draw_region(0, 0, TEXT_WIDTH, TEXT_HEIGHT);
}

void chargfx_set_palette_color(int idx, uint16_t rgb565_color) {
  palette[idx] = SWAP_BYTES(rgb565_color);
}

void chargfx_init() { ili9341_init(); }
