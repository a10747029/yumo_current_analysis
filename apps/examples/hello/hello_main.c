/****************************************************************************
 * apps/examples/hello/hello_main.c
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.  The
 * ASF licenses this file to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance with the
 * License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.  See the
 * License for the specific language governing permissions and limitations
 * under the License.
 *
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/
/****************************************************************************
 * apps/examples/hello/hello_main.c
 *
 * Color-cycle test for the GC9B72 LCD using the existing hello app.
 ****************************************************************************/

#include <nuttx/config.h>
#include <nuttx/board.h>
#include <nuttx/lcd/lcd.h>

#include <stdio.h>
#include <stdint.h>
#include <unistd.h>

#define COLOR_RED    0xf800
#define COLOR_GREEN  0x07e0
#define COLOR_BLUE   0x001f
#define COLOR_BLACK  0x0000
#define COLOR_WHITE  0xffff

static void fill_screen(FAR struct lcd_dev_s *lcd, uint16_t color)
{
  struct fb_videoinfo_s vinfo;
  struct lcd_planeinfo_s pinfo;
  uint16_t line[648];
  int row;
  int ret;

  ret = lcd->getvideoinfo(lcd, &vinfo);
  if (ret < 0)
    {
      printf("hello: getvideoinfo failed: %d\n", ret);
      return;
    }

  ret = lcd->getplaneinfo(lcd, 0, &pinfo);
  if (ret < 0)
    {
      printf("hello: getplaneinfo failed: %d\n", ret);
      return;
    }

  for (int i = 0; i < vinfo.xres; i++)
    {
      line[i] = color;
    }

  for (row = 0; row < vinfo.yres; row++)
    {
      ret = pinfo.putrun(pinfo.dev, row, 0,
                         (FAR const uint8_t *)line, vinfo.xres);
      if (ret < 0)
        {
          printf("hello: putrun failed at row %d: %d\n", row, ret);
          return;
        }
    }
}

int main(int argc, FAR char *argv[])
{
  FAR struct lcd_dev_s *lcd;

  printf("hello: start\n");

  lcd = board_lcd_getdev(0);
  if (!lcd)
    {
      printf("hello: LCD device not found\n");
      return 1;
    }

  lcd->setpower(lcd, 1);

  while (1)
    {
      fill_screen(lcd, COLOR_RED);
      printf("hello: red\n");
      sleep(1);

      fill_screen(lcd, COLOR_GREEN);
      printf("hello: green\n");
      sleep(1);

      fill_screen(lcd, COLOR_BLUE);
      printf("hello: blue\n");
      sleep(1);

      fill_screen(lcd, COLOR_BLACK);
      printf("hello: black\n");
      sleep(1);

      fill_screen(lcd, COLOR_WHITE);
      printf("hello: white\n");
      sleep(1);
    }

  return 0;
}
