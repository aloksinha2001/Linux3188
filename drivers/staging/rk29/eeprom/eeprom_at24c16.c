/* drivers/staging/rk29/eeprom/eeprom_at24c16.c - driver for ateml eeprom at24c16
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
#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/bcd.h>
#include <linux/delay.h>
#include <linux/slab.h>


#include "eeprom_at24c16.h"


#define EEPROM_SPEED 	100 * 1000


#if 1
#define DBG(x...) printk(KERN_INFO x)
#else
#define DBG(x...) do { } while (0)
#endif

#if 1
#define ERR(x...) printk(KERN_ERR x)
#else
#define ERR(x...) do { } while (0)
#endif

struct i2c_client *eeprom_client;

static int eeprom_i2c_read(struct i2c_client *client, u8 reg, u8 buf[], unsigned len)
{
	int ret; 
	
	ret = i2c_master_reg8_recv(client, reg, buf, len, EEPROM_SPEED);
	if (ret != len)
		ERR("eeprom i2c read fail,ret=0x%x\n",ret);
		
	return ret; 
}

static int eeprom_i2c_write(struct i2c_client *client, u8 reg, u8 buf[], unsigned len)
{
	int ret; 
	
	ret = i2c_master_reg8_send(client, reg, buf, len, EEPROM_SPEED);
	if (ret != len)
		ERR("eeprom i2c write fail,ret=0x%x\n",ret);
		
	return ret; 
}


int eeprom_read_data(u8 reg, u8 buf[], unsigned len)
{
    int ret; 
    
    DBG("eeprom read data,offset=0x%x,len=0x%x\n",reg,len);
    
    ret = eeprom_i2c_read(eeprom_client, reg, buf, len);
    
    return ret;
}


int eeprom_write_data(u8 reg, u8 buf[], unsigned len)
{
    int ret; 
    
    DBG("eeprom write data,offset=0x%x,len=0x%x\n",reg,len);
    
    ret = eeprom_i2c_write(eeprom_client, reg, buf, len);
    
    return ret;
    
}

//u8 test_buf[10]={0x96,0xF4,0x8F,0x80,0x9E,0x76};

static int __devinit at24c16_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	int ret;

  DBG("enter at24c16_probe\n");
    
	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C))
		return -ENODEV;
    
    
	eeprom_client = kzalloc(sizeof(struct i2c_client), GFP_KERNEL);
	if (!eeprom_client) {
		return -ENOMEM;
	}
    
	eeprom_client = client;
	
#if 0   
    ret = eeprom_write_data(0,test_buf,6);
    printk("eeprom_write_data ret =0x%x\n",ret);
#endif

  DBG("exit at24c16_probe!\n");
  
	return 0;
}

static int __devexit at24c16_remove(struct i2c_client *client)
{
	DBG("exit at24c16_remove!\n");
	
	if (eeprom_client)
		kfree(eeprom_client);
	eeprom_client = NULL;
	return 0;
}

static const struct i2c_device_id at24c16_id[] = {
	{ "eeprom_at24c16", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, hym8563_id);

static struct i2c_driver at24c16_driver = {
	.driver		= {
		.name	= "eeprom_at24c16",
		.owner	= THIS_MODULE,
	},
	.probe		= at24c16_probe,
	.remove		= __devexit_p(at24c16_remove),
	.id_table	= at24c16_id,
};

static int __init at24c16_init(void)
{
	return i2c_add_driver(&at24c16_driver);
}

static void __exit at24c16_exit(void)
{
	i2c_del_driver(&at24c16_driver);
}

MODULE_AUTHOR("hzb@rock-chips.com");
MODULE_DESCRIPTION("atmel 24c16 eeprom driver");
MODULE_LICENSE("GPL");

fs_initcall(at24c16_init);
module_exit(at24c16_exit);

