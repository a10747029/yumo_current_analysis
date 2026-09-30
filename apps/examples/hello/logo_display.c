/****************************************************************************
 * apps/examples/hello/logo_display.c
 *
 * Build/use assumptions:
 *   - NuttX is configured for the board's GC9B72 LCD and board_lcd_getdev().
 *   - The board LCD driver performs the GC9B72 reset and Arduino-derived
 *     initialization sequence before this app starts.
 *   - Add this file as the application's source (or use it in place of
 *     hello_main.c). The LCD API exposes the logical 648 x 200 landscape
 *     surface; the driver maps it to the controller's transposed GRAM.
 *
 * The 16 x 16 one-bit glyphs below are embedded so this file has no font
 * dependencies. They are enlarged when drawn to the requested text sizes.
 *
 * SPDX-License-Identifier: Apache-2.0
 ****************************************************************************/

#include <nuttx/config.h>
#include <nuttx/board.h>
#include <nuttx/lcd/lcd.h>

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#define LOGO_LOGICAL_WIDTH  648
#define LOGO_LOGICAL_HEIGHT 200

#define LOGO_BIG_SIZE       72
#define LOGO_SMALL_SIZE     48

#define COLOR_WHITE         0xffff
#define COLOR_BLACK         0x0000
#define COLOR_RED           0xf800

static const uint16_t g_yu[16] =
{
  0x0000, 0x0ff8, 0x0808, 0x0ff8,
  0x0808, 0x0808, 0x0ff8, 0x0000,
  0x1ffc, 0x0420, 0x0240, 0x0180,
  0x0ff8, 0x0000, 0x0000, 0x3ffe
};

static const uint16_t g_mo[16] =
{
  0x0000, 0x0db0, 0x0248, 0x0ff8,
  0x0888, 0x0ff8, 0x0888, 0x0ff8,
  0x0000, 0x1998, 0x04c4, 0x1ffc,
  0x0080, 0x0080, 0x0080, 0x3ffe
};

static const uint16_t g_zhi[16] =
{
  0x0000, 0x1ffc, 0x0680, 0x01be,
  0x1fe2, 0x00be, 0x0fa2, 0x003e,
  0x0000, 0x0ff8, 0x0888, 0x0888,
  0x0ff8, 0x0888, 0x0888, 0x0ff8
};

static const uint16_t g_neng[16] =
{
  0x0000, 0x023e, 0x01a2, 0x00fe,
  0x3322, 0x0c3e, 0x0022, 0x003e,
  0x1f00, 0x113e, 0x1122, 0x1122,
  0x1f22, 0x072c, 0x1810, 0x2020
};

static const uint16_t g_gong[16] =
{
  0x0000, 0x0000, 0x0000, 0x7f3f,
  0x0809, 0x0811, 0x0811, 0x0811,
  0x7f11, 0x0021, 0x0021, 0x0021,
  0x0022, 0x0044, 0x0048, 0x0000
};

static const uint16_t g_hao[16] =
{
  0x0000, 0x0800, 0x0800, 0x3fff,
  0x1a08, 0x0c08, 0x7fbf, 0x1a08,
  0x2908, 0x087f, 0x3f08, 0x0808,
  0x0808, 0x080c, 0x080a, 0x0009
};

static const uint16_t g_fen[16] =
{
  0x0000, 0x0000, 0x0220, 0x0220,
  0x0410, 0x0140, 0x0220, 0x0410,
  0x0808, 0x10fe, 0x200a, 0x0008,
  0x0008, 0x0008, 0x0030, 0x0040
};

static const uint16_t g_xi[16] =
{
  0x0000, 0x0000, 0x083f, 0x0820,
  0x2a20, 0x1c7e, 0x7f30, 0x0828,
  0x1c28, 0x1a24, 0x2822, 0x0822,
  0x0831, 0x0820, 0x0860, 0x0080
};

static const uint16_t g_yi[16] =
{
  0x0000, 0x0020, 0x1020, 0x1020,
  0x2800, 0x28fe, 0x5440, 0x1020,
  0x1020, 0x1010, 0x100c, 0x100e,
  0x1031, 0x10c0, 0x1100, 0x1000
};

static uint16_t g_line[LOGO_LOGICAL_WIDTH];

static int fill_screen(FAR struct lcd_planeinfo_s *plane,
                       uint16_t width, uint16_t height, uint16_t color)
{
  uint16_t row;
  uint16_t col;

  for (col = 0; col < width; col++)
    {
      g_line[col] = color;
    }

  for (row = 0; row < height; row++)
    {
      int ret = plane->putrun(plane->dev, row, 0,
                              (FAR const uint8_t *)g_line, width);
      if (ret < 0)
        {
          printf("logo_display: clear failed at row %u: %d\n", row, ret);
          return ret;
        }
    }

  return 0;
}

static int draw_glyph(FAR struct lcd_planeinfo_s *plane,
                      uint16_t screen_width, uint16_t screen_height,
                      int x, int y, uint16_t size,
                      FAR const uint16_t *bitmap, uint16_t foreground,
                      uint16_t background)
{
  uint16_t row;

  if (x < 0 || y < 0 || x + size > screen_width ||
      y + size > screen_height)
    {
      return -1;
    }

  for (row = 0; row < size; row++)
    {
      uint16_t col;
      unsigned int source_y = ((unsigned int)row * 16) / size;

      for (col = 0; col < size; col++)
        {
          unsigned int source_x = ((unsigned int)col * 16) / size;
          bool pixel = (bitmap[source_y] &
                        (uint16_t)(1u << (15 - source_x))) != 0;

          g_line[col] = pixel ? foreground : background;
        }

      {
        int ret = plane->putrun(plane->dev, y + row, x,
                                (FAR const uint8_t *)g_line, size);
        if (ret < 0)
          {
            printf("logo_display: glyph write failed at (%d,%u): %d\n",
                   x, y + row, ret);
            return ret;
          }
      }
    }

  return 0;
}

static int draw_logo(FAR struct lcd_planeinfo_s *plane,
                     uint16_t width, uint16_t height)
{
  const int big_gap = 4;
  const int group_gap = 16;
  const int small_gap = 3;
  const int total_width = 4 * LOGO_BIG_SIZE + 3 * big_gap +
                          group_gap + 5 * LOGO_SMALL_SIZE +
                          4 * small_gap;
  const int big_y = (height - LOGO_BIG_SIZE) / 2;
  const int small_y = big_y + LOGO_BIG_SIZE - LOGO_SMALL_SIZE;
  int x = (width - total_width) / 2;
  int ret;

#define DRAW_BIG(glyph) \
  do \
    { \
      ret = draw_glyph(plane, width, height, x, big_y, LOGO_BIG_SIZE, \
                       glyph, COLOR_RED, COLOR_WHITE); \
      if (ret < 0) \
        { \
          return ret; \
        } \
      x += LOGO_BIG_SIZE + big_gap; \
    } \
  while (0)

  DRAW_BIG(g_yu);
  DRAW_BIG(g_mo);
  DRAW_BIG(g_zhi);
  DRAW_BIG(g_neng);
  x -= big_gap;
  x += group_gap;

#undef DRAW_BIG

#define DRAW_SMALL(glyph) \
  do \
    { \
      ret = draw_glyph(plane, width, height, x, small_y, LOGO_SMALL_SIZE, \
                       glyph, COLOR_BLACK, COLOR_WHITE); \
      if (ret < 0) \
        { \
          return ret; \
        } \
      x += LOGO_SMALL_SIZE + small_gap; \
    } \
  while (0)

  DRAW_SMALL(g_gong);
  DRAW_SMALL(g_hao);
  DRAW_SMALL(g_fen);
  DRAW_SMALL(g_xi);
  DRAW_SMALL(g_yi);

#undef DRAW_SMALL

  return 0;
}

int main(int argc, FAR char *argv[])
{
  FAR struct lcd_dev_s *lcd;
  struct fb_videoinfo_s vinfo;
  struct lcd_planeinfo_s plane;
  int ret;

  (void)argc;
  (void)argv;

  lcd = board_lcd_getdev(0);
  if (lcd == NULL)
    {
      printf("logo_display: LCD device not found\n");
      return 1;
    }

  ret = lcd->getvideoinfo(lcd, &vinfo);
  if (ret < 0)
    {
      printf("logo_display: getvideoinfo failed: %d\n", ret);
      return 1;
    }

  ret = lcd->getplaneinfo(lcd, 0, &plane);
  if (ret < 0)
    {
      printf("logo_display: getplaneinfo failed: %d\n", ret);
      return 1;
    }

  if (plane.putrun == NULL)
    {
      printf("logo_display: LCD does not support putrun\n");
      return 1;
    }

  if (vinfo.xres > LOGO_LOGICAL_WIDTH ||
      vinfo.xres < 4 * LOGO_BIG_SIZE + 3 * 4 + 16 +
                   5 * LOGO_SMALL_SIZE + 4 * 3 ||
      vinfo.yres < LOGO_BIG_SIZE ||
      vinfo.yres > LOGO_LOGICAL_HEIGHT)
    {
      printf("logo_display: LCD is too small (%u x %u)\n",
             vinfo.xres, vinfo.yres);
      return 1;
    }

  ret = lcd->setpower(lcd, 1);
  if (ret < 0)
    {
      printf("logo_display: setpower failed: %d\n", ret);
      return 1;
    }

  ret = fill_screen(&plane, vinfo.xres, vinfo.yres, COLOR_WHITE);
  if (ret < 0)
    {
      return 1;
    }

  ret = draw_logo(&plane, vinfo.xres, vinfo.yres);
  if (ret < 0)
    {
      return 1;
    }

  printf("logo_display: logo drawn\n");
  return 0;
}
