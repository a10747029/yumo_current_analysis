/****************************************************************************
 * include/nuttx/lcd/gc9b72.h
 *
 * SPDX-License-Identifier: Apache-2.0
 ****************************************************************************/

#ifndef __INCLUDE_NUTTX_LCD_GC9B72_H
#define __INCLUDE_NUTTX_LCD_GC9B72_H

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/compiler.h>

/****************************************************************************
 * Public Function Prototypes
 ****************************************************************************/

#ifdef __cplusplus
extern "C"
{
#endif

struct lcd_dev_s;
struct spi_dev_s;

/****************************************************************************
 * Name: gc9b72_lcdinitialize
 *
 * Description:
 *   Initialize the GC9B72 SPI LCD controller.
 *
 ****************************************************************************/

FAR struct lcd_dev_s *gc9b72_lcdinitialize(FAR struct spi_dev_s *spi);

#ifdef __cplusplus
}
#endif

#endif /* __INCLUDE_NUTTX_LCD_GC9B72_H */
