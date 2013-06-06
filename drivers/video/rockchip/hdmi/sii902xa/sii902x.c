#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/i2c.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <mach/gpio.h>
#include <mach/iomux.h>
#include <mach/board.h>

#include "sii902x.h"

static struct sii902x *sii902x = NULL;

//------------------------------------------------------------------------------
// Function Name: DelayMS()
// Function Description: Introduce a busy-wait delay equal, in milliseconds, to the input parameter.
//
// Accepts: Length of required delay in milliseconds (max. 65535 ms)
inline void DelayMS (word MS)
{
	msleep(MS);
}

byte I2CReadByte ( byte SlaveAddr, byte RegAddr )
{
	byte val = 0;
	
	sii902x->client->addr = SlaveAddr >> 1;
	i2c_master_reg8_recv(sii902x->client, RegAddr, &val, 1, sii902x_SCL_RATE);
	return val;
}

//------------------------------------------------------------------------------
// Function Name: I2CReadBlock
// Function Description: Reads block of data from HDMI Device
//------------------------------------------------------------------------------
byte I2CReadBlock( byte SlaveAddr, byte RegAddr, byte NBytes, byte * Data )
{
	int ret;
	sii902x->client->addr = SlaveAddr >> 1;
	ret = i2c_master_reg8_recv(sii902x->client, RegAddr, Data, NBytes, sii902x_SCL_RATE);
	if(ret > 0)
		return 0;
	else
		return 1;
}

void I2CWriteByte ( byte SlaveAddr, byte RegAddr, byte Data )
{
//	struct i2c_adapter *adap=client->adapter;
	sii902x->client->addr = SlaveAddr >> 1;
	sii902x->client->adapter->retries = 0;
	i2c_master_reg8_send(sii902x->client, RegAddr, &Data, 1, sii902x_SCL_RATE);
}

//------------------------------------------------------------------------------
// Function Name:  I2CWriteBlock
// Function Description: Writes block of data from HDMI Device
//------------------------------------------------------------------------------
byte I2CWriteBlock( byte SlaveAddr, byte RegAddr, byte NBytes, byte * Data )
{
	int ret;
	sii902x->client->addr = SlaveAddr >> 1;
	ret = i2c_master_reg8_send(sii902x->client, RegAddr, Data, NBytes, sii902x_SCL_RATE);
	if(ret > 0)
		return 0;
	else
		return -1;
}

//------------------------------------------------------------------------------
// Function Name:  siiReadSegmentBlockEDID
// Function Description: Reads segment block of EDID from HDMI Downstream Device
//------------------------------------------------------------------------------
byte siiReadSegmentBlockEDID(byte SlaveAddr, byte Segment, byte Offset, byte *Buffer, byte Length)
{
	int ret;
	struct i2c_client *client = sii902x->client;
	struct i2c_adapter *adap=client->adapter;
	struct i2c_msg msgs[3];

	msgs[0].addr = EDID_SEG_ADDR >> 1;
	msgs[0].flags = client->flags | I2C_M_IGNORE_NAK;
	msgs[0].len = 1;
	msgs[0].buf = (char *)&Segment;
	msgs[0].scl_rate = sii902x_SCL_RATE;
	msgs[0].udelay = client->udelay;

	msgs[1].addr = SlaveAddr >> 1;
	msgs[1].flags = client->flags;
	msgs[1].len = 1;
	msgs[1].buf = &Offset;
	msgs[1].scl_rate = sii902x_SCL_RATE;
	msgs[1].udelay = client->udelay;

	msgs[2].addr = SlaveAddr >> 1;
	msgs[2].flags = client->flags | I2C_M_RD;
	msgs[2].len = Length;
	msgs[2].buf = (char *)Buffer;
	msgs[2].scl_rate = sii902x_SCL_RATE;
	msgs[2].udelay = client->udelay;
	
	ret = i2c_transfer(adap, msgs, 3);
	return (ret == 3) ? 0 : -1;
}

void SII902x_Reset(void)
{
	if(sii902x->io_rst_pin != INVALID_GPIO) {
		gpio_set_value(sii902x->io_rst_pin, 0);
		msleep(10);
		gpio_set_value(sii902x->io_rst_pin, 1);        //reset
	}
}

static int sii902x_enable(struct hdmi *hdmi)
{
//	printk("sii902x_enable\n");
	if(sii902x_detect_hotplug(hdmi) == HDMI_HPD_ACTIVED)
		hdmi_submit_work(hdmi, HDMI_HPD_CHANGE, 10, NULL);
	return 0;
}

static int sii902x_disable(struct hdmi *hdmi)
{
//	printk("sii902x_disable\n");
	return 0;
}

static void sii902x_irq_work_func(struct work_struct *work)
{
	struct hdmi *hdmi = sii902x->hdmi;

	sii902x_irq_poll(hdmi);
	queue_delayed_work(sii902x->workqueue, &sii902x->delay_work, 50);
}

static irqreturn_t sii902x_detect_irq(int irq, void *dev_id)
{
	printk(KERN_INFO "sii902x irq triggered.\n");
	schedule_work(&sii902x->irq_work);
    return IRQ_HANDLED;
}

static struct hdmi_property sii902x_property;

static struct hdmi_ops sii902x_ops = {
	.enable = sii902x_enable,
	.disable = sii902x_disable,
	.getStatus = sii902x_detect_hotplug,
	.insert = sii902x_insert,
	.remove = sii902x_remove,
	.getEdid = sii902x_read_edid,
	.setVideo = sii902x_config_video,
	.setAudio = sii902x_config_audio,
	.setMute = sii902x_control_output,
	.setVSI = sii902x_config_vsi,
	.setCEC = sii902x_config_cec,
};

static int sii902x_i2c_probe(struct i2c_client *client,const struct i2c_device_id *id)
{
    int rc = 0;
	struct rkdisplay_platform_data *hdmi_data = client->dev.platform_data;
	
	sii902x = kzalloc(sizeof(struct sii902x), GFP_KERNEL);
	if(!sii902x)
	{
        dev_err(&client->dev, "no memory for state\n");
        goto err_kzalloc_sii902x;
    }
	sii902x->client = client;
	sii902x->io_irq_pin = client->irq;
	i2c_set_clientdata(client, sii902x);
	
	sii902x_property.name = (char*)client->name;
	sii902x_property.priv = sii902x;
	
	// Register HDMI device
	if(hdmi_data) {
		sii902x->io_pwr_pin = hdmi_data->io_pwr_pin;
		sii902x->io_rst_pin = hdmi_data->io_reset_pin;
		sii902x_property.videosrc = hdmi_data->video_source;
		sii902x_property.display = hdmi_data->property;
		
	}
	else {
		sii902x->io_pwr_pin = INVALID_GPIO;
		sii902x->io_rst_pin = INVALID_GPIO;	
		sii902x_property.videosrc = DISPLAY_SOURCE_LCDC0;
		sii902x_property.display = DISPLAY_MAIN;
	}
	
	sii902x->hdmi = hdmi_register(&sii902x_property, &sii902x_ops);
	if(sii902x->hdmi == NULL)
	{
		dev_err(&client->dev, "fail to register hdmi\n");
		goto err_hdmi_register;
	}
	
	//Power on sii902x
	if(sii902x->io_pwr_pin != INVALID_GPIO) {
		rc = gpio_request(sii902x->io_pwr_pin, NULL);
		if(rc) {
			gpio_free(sii902x->io_pwr_pin);
        	printk(KERN_ERR "request sii902x power control gpio error\n ");
        	goto err_hdmi_register; 
		}
		else
			gpio_direction_output(sii902x->io_pwr_pin, GPIO_HIGH);
	}
	// Reset sii902x
	if(sii902x->io_rst_pin != INVALID_GPIO) {
		rc = gpio_request(sii902x->io_rst_pin, NULL);
		if(rc) {
			gpio_free(sii902x->io_rst_pin);
        	printk(KERN_ERR "request sii902x reset control gpio error\n ");
        	goto err_hdmi_register; 
		}
		else
			gpio_direction_output(sii902x->io_rst_pin, GPIO_HIGH);
	}
	
    if(sii902x_sys_init() != HDMI_ERROR_SUCESS) {
    	printk(KERN_ERR "sii902x init failed\n ");
    	goto err_request_gpio;
    }
#if 0
	if(sii902x->io_irq_pin != INVALID_GPIO) {
	    if((rc = gpio_request(sii902x->io_irq_pin, "hdmi gpio")) < 0)
	    {
	        dev_err(&client->dev, "fail to request irq gpio %d\n", client->irq);
	        goto err_request_gpio;
	    }		
	    gpio_pull_updown(sii902x->io_irq_pin, PullDisable);
	    gpio_direction_input(sii902x->io_irq_pin);
	    
	    INIT_WORK(&sii902x->irq_work, sii902x_irq_work_func);
	    sii902x->irq = gpio_to_irq(sii902x->io_irq_pin);
	    if((rc = request_irq(sii902x->irq, sii902x_detect_irq,IRQF_TRIGGER_FALLING,NULL,sii902x)) <0)
	    {
	        dev_err(&client->dev, "fail to request hdmi irq\n");
	        goto err_request_irq;
	    }
	}
	else
#endif
	{
		sii902x->workqueue = create_singlethread_workqueue("sii902x irq");
		INIT_DELAYED_WORK(&(sii902x->delay_work), sii902x_irq_work_func);
		sii902x_irq_work_func(NULL);
//		queue_delayed_work(sii902x->workqueue, &sii902x->delay_work, 0);
	}
	
	dev_info(&client->dev, "sii902x probe ok\n");

    return 0;
	
err_request_irq:
	gpio_free(client->irq);
err_request_gpio:
	hdmi_unregister(sii902x->hdmi);
err_hdmi_register:
	kfree(sii902x);
	sii902x = NULL;
err_kzalloc_sii902x:
	dev_err(&client->dev, "sii902x probe error\n");
	return rc;

}

static int __devexit sii902x_i2c_remove(struct i2c_client *client)
{
	struct hdmi *hdmi = sii902x->hdmi;

	free_irq(sii902x->irq, NULL);
	gpio_free(sii902x->irq);
	hdmi_unregister(hdmi);
	kfree(sii902x);
	sii902x = NULL;	
	printk(KERN_INFO "%s\n", __func__);
    return 0;
}


static const struct i2c_device_id sii902x_id[] = {
	{ "sii902x", 0 },
	{ }
};

static struct i2c_driver sii902x_i2c_driver  = {
    .driver = {
        .name  = "sii902x",
        .owner = THIS_MODULE,
    },
    .probe 		= &sii902x_i2c_probe,
    .remove     = &sii902x_i2c_remove,
    .id_table	= sii902x_id,
};

static int __init sii902x_init(void)
{
    return i2c_add_driver(&sii902x_i2c_driver);
}

static void __exit sii902x_exit(void)
{
    i2c_del_driver(&sii902x_i2c_driver);
}

//fs_initcall(sii902x_init);
module_init(sii902x_init);
module_exit(sii902x_exit);
