#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <mach/board.h>
#include <mach/gpio.h>
#include <mach/iomux.h>
#include <linux/i2c.h>
#include "it66121.h"

//yxg<<++
#include <linux/wakelock.h>
static struct wake_lock hdmi_wake_lock;
#include <mach/board.h>
//yxg ++>>

static struct it66121 *it66121 = NULL;

BOOL i2c_write_byte( BYTE address,BYTE offset,BYTE byteno,BYTE *p_data,BYTE device )
{
	if(i2c_master_reg8_send(it66121->client, offset, p_data, byteno, IT66121_I2C_RATE) == byteno)
		return true;
	else
		return false;
}

BOOL i2c_read_byte( BYTE address,BYTE offset,BYTE byteno,BYTE *p_data,BYTE device )
{
	if(i2c_master_reg8_recv(it66121->client, offset, p_data, byteno, IT66121_I2C_RATE) == byteno)
		return true;
	else
		return false;
}

static void it66121_irq_work_func(struct work_struct *work)
{
	struct hdmi *hdmi = it66121->hdmi;

	it66121_poll_status(hdmi);
	queue_delayed_work(it66121->workqueue, &it66121->delay_work, 50);
}

static irqreturn_t it66121_detect_irq(int irq, void *dev_id)
{
	schedule_work(&it66121->irq_work);
    return IRQ_HANDLED;
}

static struct hdmi_property it66121_property;

static struct hdmi_ops it66121_ops = {
//	.enable = it66121_enable,
//	.disable = it66121_disable,
	.getStatus = it66121_detect_hotplug,
	.insert = it66121_insert,
	.remove = it66121_remove,
	.getEdid = it66121_read_edid,
	.setVideo = it66121_config_video,
	.setAudio = it66121_config_audio,
	.setMute = it66121_set_output,
//	.setVSI = it66121_config_vsi,
//	.setCEC = it66121_config_cec,
};

static int it66121_i2c_probe(struct i2c_client *client,const struct i2c_device_id *id)
{
	int rc = 0;
	struct hdmi *hdmi = NULL;
	struct rkdisplay_platform_data *hdmi_data = client->dev.platform_data;
	
	it66121 = kzalloc(sizeof(struct it66121), GFP_KERNEL);
	if(!it66121)
	{
        dev_err(&client->dev, "no memory for state\n");
        goto err_kzalloc_it66121;
    }
	it66121->client = client;
	it66121->io_irq_pin = client->irq;
	i2c_set_clientdata(client, it66121);
	
	it66121_property.name = (char*)client->name;
	it66121_property.priv = it66121;
	
	// Register HDMI device
	if(hdmi_data) {
		it66121->io_pwr_pin = hdmi_data->io_pwr_pin;
		it66121->io_rst_pin = hdmi_data->io_reset_pin;
		it66121_property.videosrc = hdmi_data->video_source;
		it66121_property.display = hdmi_data->property;
		
	}
	else {
		it66121->io_pwr_pin = INVALID_GPIO;
		it66121->io_rst_pin = INVALID_GPIO;	
		it66121_property.videosrc = DISPLAY_SOURCE_LCDC0;
		it66121_property.display = DISPLAY_MAIN;
	}
	
	it66121->hdmi = hdmi_register(&it66121_property, &it66121_ops);
	if(it66121->hdmi == NULL)
	{
		dev_err(&client->dev, "fail to register hdmi\n");
		goto err_hdmi_register;
	}
	
	//Power on it66121
	if(it66121->io_pwr_pin != INVALID_GPIO) {
		rc = gpio_request(it66121->io_pwr_pin, NULL);
		if(rc) {
			gpio_free(it66121->io_pwr_pin);
        	printk(KERN_ERR "request it66121 power control gpio error\n ");
        	goto err_hdmi_register; 
		}
		else
			gpio_direction_output(it66121->io_pwr_pin, GPIO_HIGH);
	}
	// Reset it66121
	if(it66121->io_rst_pin != INVALID_GPIO) {
		rc = gpio_request(it66121->io_rst_pin, NULL);
		if(rc) {
			gpio_free(it66121->io_rst_pin);
        	printk(KERN_ERR "request it66121 reset control gpio error\n ");
        	goto err_hdmi_register; 
		}
		else
			gpio_direction_output(it66121->io_rst_pin, GPIO_HIGH);
	}
	
	if(it66121_initial())
		goto err_request_gpio;
	
//	hdmi_enable(hdmi);
#if 0
	if((rc = gpio_request(it66121->io_irq_pin, "hdmi gpio")) < 0)
    {
        dev_err(&client->dev, "fail to request gpio %d\n", client->irq);
        goto err_request_gpio;
    }
    gpio_pull_updown(it66121->io_irq_pin, PullDisable);
    gpio_direction_input(it66121->io_irq_pin);
    
    INIT_WORK(&it66121->irq_work, it66121_irq_work_func);
    it66121->irq = gpio_to_irq(it66121->io_irq_pin);
    if((rc = request_irq(it66121->irq, it66121_detect_irq,IRQF_TRIGGER_FALLING,NULL,it66121)) <0)
    {
        dev_err(&client->dev, "fail to request hdmi irq\n");
        goto err_request_irq;
    }
#endif
	{
		it66121->workqueue = create_singlethread_workqueue("it66121 irq");
		INIT_DELAYED_WORK(&(it66121->delay_work), it66121_irq_work_func);
		it66121_irq_work_func(NULL);
//		queue_delayed_work(sii902x->workqueue, &sii902x->delay_work, 0);
	}

	//yxg <<++
	wake_lock_init(&hdmi_wake_lock, WAKE_LOCK_SUSPEND, "hdmi_lock");
	wake_lock(&hdmi_wake_lock); 
	printk("yxg--------hdmi_wake_lock\n");
//yxg ++>>
	printk(KERN_INFO "IT66121 probe success.");	
	return 0;
	
err_request_irq:
	gpio_free(client->irq);
err_request_gpio:
	hdmi_unregister(it66121->hdmi);
err_hdmi_register:
	kfree(it66121);
	it66121 = NULL;
err_kzalloc_it66121:
	dev_err(&client->dev, "it66121 probe error\n");
	return rc;
}

static int __devexit it66121_i2c_remove(struct i2c_client *client)
{
	
    return 0;
}

static const struct i2c_device_id it66121_id[] = {
	{ "it66121", 0 },
	{ }
};

static struct i2c_driver it66121_i2c_driver = {
    .driver = {
        .name  = "it66121",
        .owner = THIS_MODULE,
    },
    .probe      = &it66121_i2c_probe,
    .remove     = &it66121_i2c_remove,
    .id_table	= it66121_id,
};

static int __init it66121_init(void)
{
    return i2c_add_driver(&it66121_i2c_driver);
}

static void __exit it66121_exit(void)
{
    i2c_del_driver(&it66121_i2c_driver);
}

//module_init(it66121_init);
fs_initcall(it66121_init);
module_exit(it66121_exit);
