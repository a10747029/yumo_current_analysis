/****************************************************************************
 * drivers/lcd/gc9b72.c
 *
 * GC9B72 SPI TFT LCD driver.
 *
 * The panel used by this board has a logical resolution of 648x200.
 * Its controller GRAM is addressed as X=200 and Y=648, therefore every
 * logical rectangle (x, y) is written as controller coordinates (y, x).
 *
 * SPDX-License-Identifier: Apache-2.0
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>

#include <sys/types.h>
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <errno.h>
#include <assert.h>

#include <nuttx/arch.h>
#include <nuttx/compiler.h>
#include <nuttx/debug.h>
#include <nuttx/spi/spi.h>
#include <nuttx/lcd/lcd.h>
#include <nuttx/lcd/gc9b72.h>

#include "gc9b72.h"

#ifdef CONFIG_LCD_GC9B72

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#ifndef CONFIG_LCD_GC9B72_SPIMODE
#  define CONFIG_LCD_GC9B72_SPIMODE SPIDEV_MODE0
#endif

#ifndef CONFIG_LCD_GC9B72_FREQUENCY
#  define CONFIG_LCD_GC9B72_FREQUENCY 18000000
#endif

#ifndef CONFIG_LCD_GC9B72_XRES
#  define CONFIG_LCD_GC9B72_XRES 648
#endif

#ifndef CONFIG_LCD_GC9B72_YRES
#  define CONFIG_LCD_GC9B72_YRES 200
#endif

#ifndef CONFIG_LCD_GC9B72_BPP
#  define CONFIG_LCD_GC9B72_BPP 16
#endif

#ifndef CONFIG_LCD_MAXPOWER
#  define CONFIG_LCD_MAXPOWER 1
#endif

#ifndef CONFIG_LCD_MAXCONTRAST
#  define CONFIG_LCD_MAXCONTRAST 1
#endif

#if CONFIG_LCD_GC9B72_BPP != 16
#  error "GC9B72 driver currently supports RGB565 (16 bpp) only"
#endif

#define GC9B72_XRES          CONFIG_LCD_GC9B72_XRES
#define GC9B72_YRES          CONFIG_LCD_GC9B72_YRES
#define GC9B72_BPP           16
#define GC9B72_BYTESPP       2
#define GC9B72_COLORFMT      FB_FMT_RGB16_565

/* This buffer is intentionally small enough for STM32F103 RAM.
 * 128 pixels * 2 bytes = 256 bytes.
 */

#define GC9B72_TXPIXELS      128
#define GC9B72_TXBUFSIZE     (GC9B72_TXPIXELS * GC9B72_BYTESPP)

/****************************************************************************
 * Private Types
 ****************************************************************************/

struct gc9b72_dev_s
{
  struct lcd_dev_s dev;
  FAR struct spi_dev_s *spi;
  uint8_t power;

  /* LCD API scratch buffer.  It must hold one logical raster line. */

  uint16_t runbuffer[GC9B72_XRES];

  /* SPI wire buffer.  Pixel bytes are converted explicitly into MSB-first
   * order, matching the successful Arduino spiWrite16() implementation.
   */

  uint8_t txbuffer[GC9B72_TXBUFSIZE];
};

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/

static void gc9b72_select(FAR struct gc9b72_dev_s *priv);
static void gc9b72_deselect(FAR struct gc9b72_dev_s *priv);

static void gc9b72_sendcmd(FAR struct gc9b72_dev_s *priv, uint8_t cmd);
static void gc9b72_cmddata(FAR struct gc9b72_dev_s *priv, uint8_t cmd,
                           FAR const uint8_t *data, size_t len);

static void gc9b72_init(FAR struct gc9b72_dev_s *priv);
static void gc9b72_setarea(FAR struct gc9b72_dev_s *priv,
                           uint16_t x0, uint16_t y0,
                           uint16_t x1, uint16_t y1);
static void gc9b72_wrram(FAR struct gc9b72_dev_s *priv,
                         FAR const uint8_t *buffer, size_t npixels);
static void gc9b72_fill(FAR struct gc9b72_dev_s *priv, uint16_t color);

static int gc9b72_putrun(FAR struct lcd_dev_s *dev,
                         fb_coord_t row, fb_coord_t col,
                         FAR const uint8_t *buffer, size_t npixels);

static int gc9b72_putarea(FAR struct lcd_dev_s *dev,
                          fb_coord_t row_start, fb_coord_t row_end,
                          fb_coord_t col_start, fb_coord_t col_end,
                          FAR const uint8_t *buffer, fb_coord_t stride);

static int gc9b72_getvideoinfo(FAR struct lcd_dev_s *dev,
                               FAR struct fb_videoinfo_s *vinfo);

static int gc9b72_getplaneinfo(FAR struct lcd_dev_s *dev,
                               unsigned int planeno,
                               FAR struct lcd_planeinfo_s *pinfo);

static int gc9b72_getpower(FAR struct lcd_dev_s *dev);
static int gc9b72_setpower(FAR struct lcd_dev_s *dev, int power);
static int gc9b72_getcontrast(FAR struct lcd_dev_s *dev);
static int gc9b72_setcontrast(FAR struct lcd_dev_s *dev,
                              unsigned int contrast);

/****************************************************************************
 * Private Data
 ****************************************************************************/

static struct gc9b72_dev_s g_gc9b72;

/****************************************************************************
 * Private Functions
 ****************************************************************************/

static void gc9b72_select(FAR struct gc9b72_dev_s *priv)
{
  SPI_LOCK(priv->spi, true);
  SPI_SELECT(priv->spi, SPIDEV_DISPLAY(0), true);
  SPI_SETMODE(priv->spi, CONFIG_LCD_GC9B72_SPIMODE);
  SPI_SETBITS(priv->spi, 8);
  SPI_SETFREQUENCY(priv->spi, CONFIG_LCD_GC9B72_FREQUENCY);
}

static void gc9b72_deselect(FAR struct gc9b72_dev_s *priv)
{
  SPI_SELECT(priv->spi, SPIDEV_DISPLAY(0), false);
  SPI_LOCK(priv->spi, false);
}

static void gc9b72_sendcmd(FAR struct gc9b72_dev_s *priv, uint8_t cmd)
{
  gc9b72_select(priv);

  SPI_CMDDATA(priv->spi, SPIDEV_DISPLAY(0), true);
  SPI_SEND(priv->spi, cmd);

  SPI_CMDDATA(priv->spi, SPIDEV_DISPLAY(0), false);
  gc9b72_deselect(priv);
}

static void gc9b72_cmddata(FAR struct gc9b72_dev_s *priv, uint8_t cmd,
                           FAR const uint8_t *data, size_t len)
{
  gc9b72_select(priv);

  SPI_CMDDATA(priv->spi, SPIDEV_DISPLAY(0), true);
  SPI_SEND(priv->spi, cmd);

  if (len > 0)
    {
      SPI_CMDDATA(priv->spi, SPIDEV_DISPLAY(0), false);
      SPI_SNDBLOCK(priv->spi, data, len);
    }

  gc9b72_deselect(priv);
}

/****************************************************************************
 * Name: gc9b72_init
 *
 * Description:
 *   This is the exact initialization sequence from the working Arduino
 *   application, rewritten with gc9b72_cmddata().
 *
 ****************************************************************************/

static void gc9b72_init(FAR struct gc9b72_dev_s *priv)
{
  static const uint8_t d80[] = {0x19};
  static const uint8_t d81[] = {0x30};
  static const uint8_t d82[] = {0x09};
  static const uint8_t d83[] = {0x03};
  static const uint8_t d84[] = {0x20};
  static const uint8_t d86[] = {0x18};
  static const uint8_t d87[] = {0x0a};
  static const uint8_t d89[] = {0x38};
  static const uint8_t d8a[] = {0x40};
  static const uint8_t d8b[] = {0x0a};
  static const uint8_t d8e[] = {0x0f};
  static const uint8_t d8f[] = {0x10};
  static const uint8_t d3a[] = {0x05};
  static const uint8_t d36[] = {0x40};
  static const uint8_t dec[] = {0x07};
  static const uint8_t d98[] = {0x3e};
  static const uint8_t d99[] = {0x3e};
  static const uint8_t da1[] = {0x01, 0x04};
  static const uint8_t da2[] = {0x01, 0x04};
  static const uint8_t dcb[] = {0x02};
  static const uint8_t db5[] = {0x15, 0x15};
  static const uint8_t deb[] = {0x02, 0x87};
  static const uint8_t d60[] = {0x58, 0x22, 0x01, 0x58};
  static const uint8_t d63[] = {0x2d, 0x39, 0x01, 0x52};
  static const uint8_t d64[] = {0x38, 0x24, 0x75, 0x3c, 0x04, 0x58};
  static const uint8_t d65[] = {0x38, 0x28, 0x75, 0x40, 0x04, 0x58};
  static const uint8_t d66[] = {0x38, 0x24, 0x75, 0x3c, 0x04, 0x58};
  static const uint8_t d6a[] = {0x00, 0x00};

  static const uint8_t d6c[] =
  {
    0xcc, 0x0c, 0xcc, 0x00, 0xcc, 0x04, 0x5f
  };

  static const uint8_t d6e[] =
  {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02,
    0x0a, 0x0c, 0x0e, 0x10, 0x08, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x07, 0x0f, 0x0d, 0x0b, 0x09,
    0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00
  };

  static const uint8_t d74[] =
  {
    0x00, 0xe6, 0x00, 0x00, 0x00, 0x00
  };

  static const uint8_t d7d[] = {0x72};
  static const uint8_t d7e[] = {0x1c};
  static const uint8_t d7c[] = {0xb6, 0x2b};
  static const uint8_t dac[] = {0x30};

  static const uint8_t d70[] =
  {
    0x02, 0x03, 0x03, 0x06, 0x03,
    0x03, 0x09, 0x07, 0x09, 0x03
  };

  static const uint8_t d90[] = {0x06, 0x06, 0x01, 0x01};
  static const uint8_t d93[] = {0x45, 0xff, 0x00};
  static const uint8_t dbe[] = {0x11};
  static const uint8_t dc3[] = {0x39};
  static const uint8_t dc4[] = {0x39};
  static const uint8_t dc9[] = {0x2a};
  static const uint8_t ded[] = {0x00, 0x00};

  static const uint8_t df0[] =
  {
    0x02, 0x06, 0x07, 0x05, 0x04, 0x27
  };

  static const uint8_t df1[] =
  {
    0x3f, 0x74, 0x74, 0x28, 0x32, 0x8f
  };

  static const uint8_t df2[] =
  {
    0x03, 0x06, 0x07, 0x05, 0x04, 0x27
  };

  static const uint8_t df3[] =
  {
    0x3f, 0x74, 0x74, 0x28, 0x32, 0x8f
  };

  static const uint8_t df6[] = {0x80};
  static const uint8_t df9[] = {0x70};
  static const uint8_t dfb[] = {0x70, 0x70};
  static const uint8_t dfd[] = {0x00, 0x00};
  static const uint8_t db4[] = {0x0a};
  static const uint8_t d35[] = {0x00};

  gc9b72_sendcmd(priv, GC9B72_SLPOUT);
  up_mdelay(120);

  gc9b72_sendcmd(priv, 0xfe);
  gc9b72_sendcmd(priv, 0xef);

  gc9b72_cmddata(priv, 0x80, d80, sizeof(d80));
  gc9b72_cmddata(priv, 0x81, d81, sizeof(d81));
  gc9b72_cmddata(priv, 0x82, d82, sizeof(d82));
  gc9b72_cmddata(priv, 0x83, d83, sizeof(d83));
  gc9b72_cmddata(priv, 0x84, d84, sizeof(d84));
  gc9b72_cmddata(priv, 0x86, d86, sizeof(d86));
  gc9b72_cmddata(priv, 0x87, d87, sizeof(d87));
  gc9b72_cmddata(priv, 0x89, d89, sizeof(d89));
  gc9b72_cmddata(priv, 0x8a, d8a, sizeof(d8a));
  gc9b72_cmddata(priv, 0x8b, d8b, sizeof(d8b));
  gc9b72_cmddata(priv, 0x8e, d8e, sizeof(d8e));
  gc9b72_cmddata(priv, 0x8f, d8f, sizeof(d8f));

  gc9b72_cmddata(priv, GC9B72_COLMOD, d3a, sizeof(d3a));
  gc9b72_cmddata(priv, GC9B72_MADCTL, d36, sizeof(d36));

  gc9b72_cmddata(priv, 0xec, dec, sizeof(dec));
  gc9b72_cmddata(priv, 0x98, d98, sizeof(d98));
  gc9b72_cmddata(priv, 0x99, d99, sizeof(d99));
  gc9b72_cmddata(priv, 0xa1, da1, sizeof(da1));
  gc9b72_cmddata(priv, 0xa2, da2, sizeof(da2));
  gc9b72_cmddata(priv, 0xcb, dcb, sizeof(dcb));
  gc9b72_cmddata(priv, 0xb5, db5, sizeof(db5));
  gc9b72_cmddata(priv, 0xeb, deb, sizeof(deb));
  gc9b72_cmddata(priv, 0x60, d60, sizeof(d60));
  gc9b72_cmddata(priv, 0x63, d63, sizeof(d63));
  gc9b72_cmddata(priv, 0x64, d64, sizeof(d64));
  gc9b72_cmddata(priv, 0x65, d65, sizeof(d65));
  gc9b72_cmddata(priv, 0x66, d66, sizeof(d66));
  gc9b72_cmddata(priv, 0x6a, d6a, sizeof(d6a));
  gc9b72_cmddata(priv, 0x6c, d6c, sizeof(d6c));
  gc9b72_cmddata(priv, 0x6e, d6e, sizeof(d6e));
  gc9b72_cmddata(priv, 0x74, d74, sizeof(d74));
  gc9b72_cmddata(priv, 0x7d, d7d, sizeof(d7d));
  gc9b72_cmddata(priv, 0x7e, d7e, sizeof(d7e));
  gc9b72_cmddata(priv, 0x7c, d7c, sizeof(d7c));
  gc9b72_cmddata(priv, 0xac, dac, sizeof(dac));
  gc9b72_cmddata(priv, 0x70, d70, sizeof(d70));
  gc9b72_cmddata(priv, 0x90, d90, sizeof(d90));
  gc9b72_cmddata(priv, 0x93, d93, sizeof(d93));
  gc9b72_cmddata(priv, 0xbe, dbe, sizeof(dbe));
  gc9b72_cmddata(priv, 0xc3, dc3, sizeof(dc3));
  gc9b72_cmddata(priv, 0xc4, dc4, sizeof(dc4));
  gc9b72_cmddata(priv, 0xc9, dc9, sizeof(dc9));
  gc9b72_cmddata(priv, 0xed, ded, sizeof(ded));
  gc9b72_cmddata(priv, 0xf0, df0, sizeof(df0));
  gc9b72_cmddata(priv, 0xf1, df1, sizeof(df1));
  gc9b72_cmddata(priv, 0xf2, df2, sizeof(df2));
  gc9b72_cmddata(priv, 0xf3, df3, sizeof(df3));
  gc9b72_cmddata(priv, 0xf6, df6, sizeof(df6));
  gc9b72_cmddata(priv, 0xf9, df9, sizeof(df9));
  gc9b72_cmddata(priv, 0xfb, dfb, sizeof(dfb));
  gc9b72_cmddata(priv, 0xfd, dfd, sizeof(dfd));
  gc9b72_cmddata(priv, 0xb4, db4, sizeof(db4));
  gc9b72_cmddata(priv, GC9B72_TEON, d35, sizeof(d35));

  gc9b72_sendcmd(priv, 0xfe);
  gc9b72_sendcmd(priv, 0xee);

  gc9b72_sendcmd(priv, GC9B72_SLPOUT);
  up_mdelay(120);

  gc9b72_sendcmd(priv, GC9B72_DISPON);
  up_mdelay(20);
}

/****************************************************************************
 * Name: gc9b72_setarea
 *
 * Description:
 *   Set address window.  The panel uses controller coordinates:
 *
 *     controller X = logical Y
 *     controller Y = logical X
 *
 ****************************************************************************/

static void gc9b72_setarea(FAR struct gc9b72_dev_s *priv,
                           uint16_t x0, uint16_t y0,
                           uint16_t x1, uint16_t y1)
{
  uint8_t data[4];

  /* CASET: controller X range = logical Y range */

  data[0] = y0 >> 8;
  data[1] = y0 & 0xff;
  data[2] = y1 >> 8;
  data[3] = y1 & 0xff;
  gc9b72_cmddata(priv, GC9B72_CASET, data, sizeof(data));

  /* RASET: controller Y range = logical X range */

  data[0] = x0 >> 8;
  data[1] = x0 & 0xff;
  data[2] = x1 >> 8;
  data[3] = x1 & 0xff;
  gc9b72_cmddata(priv, GC9B72_RASET, data, sizeof(data));
}

/****************************************************************************
 * Name: gc9b72_wrram
 *
 * Description:
 *   Write npixels RGB565 pixels.  NuttX buffers hold uint16_t in CPU order;
 *   the panel requires every color in high-byte, low-byte order.
 *
 ****************************************************************************/

static void gc9b72_wrram(FAR struct gc9b72_dev_s *priv,
                         FAR const uint8_t *buffer, size_t npixels)
{
  FAR const uint16_t *src = (FAR const uint16_t *)buffer;
  size_t done = 0;

  gc9b72_select(priv);

  SPI_CMDDATA(priv->spi, SPIDEV_DISPLAY(0), true);
  SPI_SEND(priv->spi, GC9B72_RAMWR);
  SPI_CMDDATA(priv->spi, SPIDEV_DISPLAY(0), false);

  while (done < npixels)
    {
      size_t count = npixels - done;
      size_t i;

      if (count > GC9B72_TXPIXELS)
        {
          count = GC9B72_TXPIXELS;
        }

      for (i = 0; i < count; i++)
        {
          uint16_t pixel = src[done + i];

          priv->txbuffer[i * 2]     = pixel >> 8;
          priv->txbuffer[i * 2 + 1] = pixel & 0xff;
        }

      SPI_SNDBLOCK(priv->spi, priv->txbuffer, count * GC9B72_BYTESPP);
      done += count;
    }

  gc9b72_deselect(priv);
}

static void gc9b72_fill(FAR struct gc9b72_dev_s *priv, uint16_t color)
{
  size_t i;
  size_t pixels = (size_t)GC9B72_XRES * GC9B72_YRES;

  for (i = 0; i < GC9B72_TXPIXELS; i++)
    {
      priv->txbuffer[i * 2]     = color >> 8;
      priv->txbuffer[i * 2 + 1] = color & 0xff;
    }

  gc9b72_setarea(priv, 0, 0, GC9B72_XRES - 1, GC9B72_YRES - 1);

  gc9b72_select(priv);

  SPI_CMDDATA(priv->spi, SPIDEV_DISPLAY(0), true);
  SPI_SEND(priv->spi, GC9B72_RAMWR);
  SPI_CMDDATA(priv->spi, SPIDEV_DISPLAY(0), false);

  while (pixels > 0)
    {
      size_t count = pixels > GC9B72_TXPIXELS ?
                     GC9B72_TXPIXELS : pixels;

      SPI_SNDBLOCK(priv->spi, priv->txbuffer, count * GC9B72_BYTESPP);
      pixels -= count;
    }

  gc9b72_deselect(priv);
}

static int gc9b72_putrun(FAR struct lcd_dev_s *dev,
                         fb_coord_t row, fb_coord_t col,
                         FAR const uint8_t *buffer, size_t npixels)
{
  FAR struct gc9b72_dev_s *priv = (FAR struct gc9b72_dev_s *)dev;

  if (buffer == NULL || row < 0 || col < 0 ||
      row >= GC9B72_YRES || col >= GC9B72_XRES ||
      npixels == 0 || npixels > (size_t)(GC9B72_XRES - col))
    {
      return -EINVAL;
    }

  gc9b72_setarea(priv, col, row, col + npixels - 1, row);
  gc9b72_wrram(priv, buffer, npixels);
  return OK;
}

static int gc9b72_putarea(FAR struct lcd_dev_s *dev,
                          fb_coord_t row_start, fb_coord_t row_end,
                          fb_coord_t col_start, fb_coord_t col_end,
                          FAR const uint8_t *buffer, fb_coord_t stride)
{
  FAR struct gc9b72_dev_s *priv = (FAR struct gc9b72_dev_s *)dev;
  size_t width;
  size_t rowbytes;
  fb_coord_t row;

  if (buffer == NULL || row_start < 0 || col_start < 0 ||
      row_end < row_start || col_end < col_start ||
      row_end >= GC9B72_YRES || col_end >= GC9B72_XRES)
    {
      return -EINVAL;
    }

  width = col_end - col_start + 1;
  rowbytes = width * GC9B72_BYTESPP;

  if (stride < (fb_coord_t)rowbytes)
    {
      return -EINVAL;
    }

  /* Because logical X/Y are transposed in hardware, a logical multi-row
   * rectangle is not contiguous in controller scan order.  Write one
   * logical row at a time; this remains efficient because each row uses
   * a single RAMWR stream rather than pixel-by-pixel transactions.
   */

  for (row = row_start; row <= row_end; row++)
    {
      FAR const uint8_t *src =
        buffer + (size_t)(row - row_start) * stride;

      gc9b72_setarea(priv, col_start, row, col_end, row);
      gc9b72_wrram(priv, src, width);
    }

  return OK;
}

static int gc9b72_getvideoinfo(FAR struct lcd_dev_s *dev,
                               FAR struct fb_videoinfo_s *vinfo)
{
  DEBUGASSERT(dev != NULL && vinfo != NULL);

  vinfo->fmt     = GC9B72_COLORFMT;
  vinfo->xres    = GC9B72_XRES;
  vinfo->yres    = GC9B72_YRES;
  vinfo->nplanes = 1;
  return OK;
}

static int gc9b72_getplaneinfo(FAR struct lcd_dev_s *dev,
                               unsigned int planeno,
                               FAR struct lcd_planeinfo_s *pinfo)
{
  FAR struct gc9b72_dev_s *priv = (FAR struct gc9b72_dev_s *)dev;

  if (planeno != 0 || pinfo == NULL)
    {
      return -EINVAL;
    }

  pinfo->putrun  = gc9b72_putrun;
  pinfo->putarea = gc9b72_putarea;
#ifndef CONFIG_LCD_NOGETRUN
  pinfo->getrun  = NULL;
#endif
  pinfo->buffer  = (FAR uint8_t *)priv->runbuffer;
  pinfo->bpp     = GC9B72_BPP;
  pinfo->dev     = dev;

  return OK;
}

static int gc9b72_getpower(FAR struct lcd_dev_s *dev)
{
  FAR struct gc9b72_dev_s *priv = (FAR struct gc9b72_dev_s *)dev;
  return priv->power;
}

static int gc9b72_setpower(FAR struct lcd_dev_s *dev, int power)
{
  FAR struct gc9b72_dev_s *priv = (FAR struct gc9b72_dev_s *)dev;

  if (power < 0 || power > CONFIG_LCD_MAXPOWER)
    {
      return -EINVAL;
    }

  if (power == 0)
    {
      gc9b72_sendcmd(priv, GC9B72_DISPOFF);
      priv->power = 0;
    }
  else
    {
      gc9b72_sendcmd(priv, GC9B72_DISPON);
      priv->power = power;
    }

  return OK;
}

static int gc9b72_getcontrast(FAR struct lcd_dev_s *dev)
{
  return -ENOSYS;
}

static int gc9b72_setcontrast(FAR struct lcd_dev_s *dev,
                              unsigned int contrast)
{
  return -ENOSYS;
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

FAR struct lcd_dev_s *gc9b72_lcdinitialize(FAR struct spi_dev_s *spi)
{
  FAR struct gc9b72_dev_s *priv = &g_gc9b72;

  if (spi == NULL)
    {
      return NULL;
    }

  priv->dev.getvideoinfo = gc9b72_getvideoinfo;
  priv->dev.getplaneinfo = gc9b72_getplaneinfo;
  priv->dev.getpower     = gc9b72_getpower;
  priv->dev.setpower     = gc9b72_setpower;
  priv->dev.getcontrast  = gc9b72_getcontrast;
  priv->dev.setcontrast  = gc9b72_setcontrast;

  priv->spi = spi;
  priv->power = 0;

  gc9b72_init(priv);
  gc9b72_fill(priv, 0xf800);

  return &priv->dev;
}

#endif /* CONFIG_LCD_GC9B72 */
