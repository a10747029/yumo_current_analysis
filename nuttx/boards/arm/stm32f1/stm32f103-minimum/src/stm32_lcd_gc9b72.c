/****************************************************************************
 * Board glue for GC9B72 SPI LCD
 ****************************************************************************/

#include <nuttx/config.h>

#include <errno.h>

#include <nuttx/arch.h>
#include <nuttx/board.h>
#include <nuttx/debug.h>
#include <nuttx/lcd/lcd.h>
#include <nuttx/lcd/gc9b72.h>
#include <nuttx/spi/spi.h>

#include "stm32.h"
#include "stm32_spi.h"
#include "stm32f103_minimum.h"

#ifdef CONFIG_LCD_GC9B72

#define GC9B72_SPI_PORT 1

static FAR struct spi_dev_s *g_spi;
static FAR struct lcd_dev_s *g_lcd;

int board_lcd_initialize(void)
{
  /* Configure non-SPI control pins.  CS is configured by
   * stm32_spidev_initialize(), called from stm32_boardinitialize().
   */

  stm32_configgpio(GPIO_GC9B72_DC);
  stm32_configgpio(GPIO_GC9B72_RST);
  stm32_configgpio(GPIO_GC9B72_BL);

  stm32_gpiowrite(GPIO_GC9B72_DC, false);
  stm32_gpiowrite(GPIO_GC9B72_RST, true);
  stm32_gpiowrite(GPIO_GC9B72_BL, false);

  g_spi = stm32_spibus_initialize(GC9B72_SPI_PORT);
  if (g_spi == NULL)
    {
      lcderr("ERROR: SPI%d initialization failed\n", GC9B72_SPI_PORT);
      return -ENODEV;
    }

  /* Hardware reset: same timing as the proven Arduino program. */

  up_mdelay(50);
  stm32_gpiowrite(GPIO_GC9B72_RST, false);
  up_mdelay(50);
  stm32_gpiowrite(GPIO_GC9B72_RST, true);
  up_mdelay(120);

  g_lcd = gc9b72_lcdinitialize(g_spi);
  if (g_lcd == NULL)
    {
      lcderr("ERROR: gc9b72_lcdinitialize failed\n");
      return -ENODEV;
    }

  g_lcd->setpower(g_lcd, CONFIG_LCD_MAXPOWER);

  /* Arduino code says BL is active-high. */

  stm32_gpiowrite(GPIO_GC9B72_BL, true);
  return OK;
}

FAR struct lcd_dev_s *board_lcd_getdev(int lcddev)
{
  return lcddev == 0 ? g_lcd : NULL;
}

void board_lcd_uninitialize(void)
{
  if (g_lcd != NULL)
    {
      g_lcd->setpower(g_lcd, LCD_FULL_OFF);
    }

  stm32_gpiowrite(GPIO_GC9B72_BL, false);
}

#endif /* CONFIG_LCD_GC9B72 */
