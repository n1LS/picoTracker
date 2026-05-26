#ifndef PICOTRACKER_BOOTLOADER_CHARGFX_H
#define PICOTRACKER_BOOTLOADER_CHARGFX_H

#include "Adapters/picoTracker/display/ili9341.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define CHAR_HEIGHT 8
#define CHAR_WIDTH 8
#define TEXT_WIDTH (ILI9341_TFTHEIGHT / CHAR_WIDTH)
#define TEXT_HEIGHT (ILI9341_TFTWIDTH / CHAR_HEIGHT)

#define char_border_single_topLeft_s "\xDA"
#define char_border_single_topRight_s "\xBF"
#define char_border_single_bottomLeft_s "\xC0"
#define char_border_single_bottomRight_s "\xD9"
#define char_border_single_horizontal_s "\xC4"
#define char_border_single_vertical_s "\xB3"
#define char_border_single_horizontalUp_s "\xC1"
#define char_border_single_horizontalDown_s "\xC2"
#define char_border_single_verticalLeft_s "\xB4"
#define char_border_single_verticalRight_s "\xC3"
#define char_border_single_cross_s "\xC5"

typedef enum {
  CHARGFX_BG,
  CHARGFX_NORMAL,
  CHARGFX_HILITE,
  CHARGFX_HILITE2,
  CHARGFX_GRAY,
  CHARGFX_DESERT,
  CHARGFX_ORANGE,
  CHARGFX_YELLOW,
  CHARGFX_WHITE,
  CHARGFX_MIDNIGHT,
  CHARGFX_DARK_SLATE_GRAY,
  CHARGFX_GREEN,
  CHARGFX_YELLOW_GREEN,
  CHARGFX_BLUE,
  CHARGFX_GURU_TXT,
  CHARGFX_GURU_BG
} chargfx_color_t;

void chargfx_set_cursor(uint8_t x, uint8_t y);
void chargfx_draw_screen(void);
void chargfx_draw_sub_region(uint8_t x, uint8_t y, uint8_t width,
                             uint8_t height);
void chargfx_draw_changed();
void chargfx_putc(char c);
void chargfx_set_foreground(chargfx_color_t color);
void chargfx_set_background(chargfx_color_t color);
void chargfx_clear(chargfx_color_t color);

#ifdef __cplusplus
}
#endif

#endif