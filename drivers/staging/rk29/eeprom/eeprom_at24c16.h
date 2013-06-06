/* drivers/staging/rk29/eeprom/eeprom_at24c16.h - head for eeprom_at24c16.c
 *
 * Copyright (C) 2010 ROCKCHIP, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
 
#ifndef _DRIVERS_EEPROM_AT24C16_H
#define _DRIVERS_EEPROM_AT24C16_H
 
extern int eeprom_read_data(u8 reg, u8 buf[], unsigned len);
extern int eeprom_write_data(u8 reg, u8 buf[], unsigned len);


#endif  /*_DRIVERS_EEPROM_AT24C16_H*/
