/****************************************************************************
 * drivers/lcd/gc9b72.h
 *
 * SPDX-License-Identifier: Apache-2.0
 ****************************************************************************/

#ifndef __DRIVERS_LCD_GC9B72_H
#define __DRIVERS_LCD_GC9B72_H

/****************************************************************************
 * GC9B72 / MIPI-like commands used by this driver
 ****************************************************************************/

#define GC9B72_SWRESET   0x01
#define GC9B72_SLPIN     0x10
#define GC9B72_SLPOUT    0x11
#define GC9B72_INVOFF    0x20
#define GC9B72_INVON     0x21
#define GC9B72_DISPOFF   0x28
#define GC9B72_DISPON    0x29
#define GC9B72_CASET     0x2a
#define GC9B72_RASET     0x2b
#define GC9B72_RAMWR     0x2c
#define GC9B72_TEON      0x35
#define GC9B72_MADCTL    0x36
#define GC9B72_COLMOD    0x3a

#endif /* __DRIVERS_LCD_GC9B72_H */
