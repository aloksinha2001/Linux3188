#include <linux/module.h>
#include <linux/device.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/fcntl.h>
#include <linux/fs.h>
#include <linux/fb.h>
#include <linux/console.h>
#include <asm/uaccess.h>
#include <linux/rk_fb.h>
#include "ch7025.h"

#ifdef CONFIG_ARCH_RK29
extern int FB_Switch_Screen( struct rk29fb_screen *screen, u32 enable );
#endif

struct ch7025 ch7025;

int ch7025_write_reg(char reg, char value)
{
	if(ch7025.client == NULL)
		return -1;
	
	if (i2c_master_reg8_send(ch7025.client, reg, &value, 1, CH7025_I2C_RATE) > 0)
		return 0;
	else
		return -1;
}

int ch7025_read_reg(char reg)
{
	char data = 0;
	
	if(ch7025.client == NULL)
		return -1;
	
	if(i2c_master_reg8_recv(ch7025.client, reg, &data, 1, CH7025_I2C_RATE) > 0)
		return data;
	else
		return -1;
}

int ch7025_switch_fb(const struct fb_videomode *modedb, int mode)
{	
	struct rk29fb_screen *screen;
	
	if(modedb == NULL)
		return -1;
	screen =  kzalloc(sizeof(struct rk29fb_screen), GFP_KERNEL);
	if(screen == NULL)
		return -1;
	
	memset(screen, 0, sizeof(struct rk29fb_screen));	
	/* screen type & face */
    screen->type = SCREEN_HDMI;
	screen->mode = modedb->vmode;
	screen->face = modedb->flag;
	/* Screen size */
	screen->x_res = modedb->xres;
    screen->y_res = modedb->yres;

    /* Timing */
    screen->pixclock = modedb->pixclock;

	screen->lcdc_aclk = 500000000;
	screen->left_margin = modedb->left_margin;
	screen->right_margin = modedb->right_margin;
	screen->hsync_len = modedb->hsync_len;
	screen->upper_margin = modedb->upper_margin;
	screen->lower_margin = modedb->lower_margin;
	screen->vsync_len = modedb->vsync_len;

	/* Pin polarity */
	if(FB_SYNC_HOR_HIGH_ACT & modedb->sync)
		screen->pin_hsync = 1;
	else
		screen->pin_hsync = 0;
	if(FB_SYNC_VERT_HIGH_ACT & modedb->sync)
		screen->pin_vsync = 1;
	else
		screen->pin_vsync = 0;	
	screen->pin_den = 0;
	screen->pin_dclk = 1;

	/* Swap rule */
    screen->swap_rb = 0;
    screen->swap_rg = 0;
    screen->swap_gb = 0;
    screen->swap_delta = 0;
    screen->swap_dumy = 0;

    /* Operation function*/
    screen->init = NULL;
    screen->standby = NULL;	
	
	ch7025.mode = mode;
#ifdef CONFIG_ARCH_RK29    
   	FB_Switch_Screen(screen, 1);
#else
	rk_fb_switch_screen(screen, 1 , ch7025.video_source);
#endif
   	kfree(screen);
   	if(ch7025.io_switch_pin != INVALID_GPIO) {
   		if(mode < TVOUT_YPbPr_720x480p_60)
   			gpio_direction_output(ch7025.io_switch_pin, VIDEO_SWITCH_CVBS);
   		else
   			gpio_direction_output(ch7025.io_switch_pin, VIDEO_SWITCH_OTHER);
   	}
	return 0;
}

static void ch7205_disable(void)
{
	DBG("%s start", __FUNCTION__);
	if(ch7025.io_rst_pin != INVALID_GPIO) {
		gpio_direction_output(ch7025.io_rst_pin, GPIO_LOW);
		msleep(1);
		gpio_direction_output(ch7025.io_rst_pin, GPIO_HIGH);
	}
	else
		ch7025_write_reg( 0x04, v_PD_DAC(7) | v_PD_PWR(1) );
}

void ch7025_standby(void)
{
	int ypbpr = 0, cvbs = 0;
	
	if(ch7025.ypbpr)
		ypbpr = ch7025.ypbpr->enable;
	if(ch7025.cvbs)
		cvbs = ch7025.cvbs->enable;
	
	if(!cvbs && !ypbpr)
		ch7205_disable();
}

#ifdef CONFIG_HAS_EARLYSUSPEND
static void ch7025_early_suspend(struct early_suspend *h)
{
	DBG("ch7025 enter early suspend");
	if(ch7025.ypbpr)
		ch7025.ypbpr->ddev->ops->setenable(ch7025.ypbpr->ddev, 0);
	if(ch7025.cvbs)
		ch7025.cvbs->ddev->ops->setenable(ch7025.cvbs->ddev, 0);
	return;
}

static void ch7025_early_resume(struct early_suspend *h)
{
	DBG("ch7025 exit early resume");
	if( ch7025.cvbs && (ch7025.mode < TVOUT_YPbPr_720x480p_60) ) {
		rk_display_device_enable((ch7025.cvbs)->ddev);
	}
	else if( ch7025.ypbpr && (ch7025.mode > TVOUT_CVBS_PAL) ) {
		rk_display_device_enable((ch7025.ypbpr)->ddev);
	}
	return;
}
#endif

static int ch7025_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	int ret;
	struct rkdisplay_platform_data *tv_data;
	
	memset(&ch7025, 0, sizeof(struct ch7025));
	ch7025.client = client;
	ch7025.dev = &client->dev;
	ch7025.mode = CH7025_DEFAULT_MODE;

	if(client->dev.platform_data) {
		tv_data = client->dev.platform_data;
		ch7025.io_pwr_pin = tv_data->io_pwr_pin;
		ch7025.io_rst_pin = tv_data->io_reset_pin;
		ch7025.io_switch_pin = tv_data->io_switch_pin;
		ch7025.video_source = tv_data->video_source;
		ch7025.property = tv_data->property;
	}
	else {
		ch7025.video_source = DISPLAY_SOURCE_LCDC0;
		ch7025.property = DISPLAY_MAIN;
	}

	// Reset
	if(ch7025.io_rst_pin != INVALID_GPIO) {
		ret = gpio_request(ch7025.io_rst_pin, NULL);
		if(ret) {
			printk(KERN_ERR "ch7025/7026 request reset gpio error\n");
			return -1;
		}
		gpio_direction_output(ch7025.io_rst_pin, GPIO_LOW);
		msleep(1);
		gpio_direction_output(ch7025.io_rst_pin, GPIO_HIGH);
	}
	if(ch7025.io_switch_pin != INVALID_GPIO) {
		ret = gpio_request(ch7025.io_switch_pin, NULL);
		if(ret) {
			gpio_free(ch7025.io_switch_pin);
			printk(KERN_ERR "ch7025/7026 request video switch gpio error\n");
			return -1;
		}
	}
	//Read Chip ID
	ch7025.chip_id = ch7025_read_reg(DEVICE_ID);
	DBG("chip id is 0x%02x revision id %02x", ch7025.chip_id, ch7025_read_reg(REVISION_ID)&0xFF);
	
	#ifdef CONFIG_CH7025_7026_TVOUT_CVBS
	ch7025_register_display_cvbs(&client->dev);
	if( ch7025.cvbs && (ch7025.mode < TVOUT_YPbPr_720x480p_60) )
		rk_display_device_enable((ch7025.cvbs)->ddev);
	#endif
	
	#ifdef CONFIG_CH7025_7026_TVOUT_YPBPR
	ch7025_register_display_ypbpr(&client->dev);
	if( ch7025.ypbpr && (ch7025.mode > TVOUT_CVBS_PAL) ) 
		rk_display_device_enable((ch7025.ypbpr)->ddev);
	#endif
	
	#ifdef CONFIG_HAS_EARLYSUSPEND
	ch7025.early_suspend.suspend = ch7025_early_suspend;
	ch7025.early_suspend.resume = ch7025_early_resume;
	ch7025.early_suspend.level = EARLY_SUSPEND_LEVEL_DISABLE_FB - 10;
	register_early_suspend(&(ch7025.early_suspend));
	#endif
	
	printk(KERN_INFO "ch7025/7026 probe sucess.\n");
	return 0;
}

static int ch7025_remove(struct i2c_client *client)
{
	return 0;
}

#define DRV_NAME "ch7025/7026"

static const struct i2c_device_id ch7025_id[] = {
	{ DRV_NAME, 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, ch7025_id);

static struct i2c_driver ch7025_driver = {
	.driver 	= {
		.name	= DRV_NAME,
	},
	.id_table = ch7025_id,
	.probe = ch7025_probe,
	.remove = ch7025_remove,
};

static int __init ch7025_init(void)
{
	int ret = 0;
	ret = i2c_add_driver(&ch7025_driver);
	if(ret < 0){
		printk("i2c_add_driver err, ret = %d\n", ret);
	}
	return ret;
}

static void __exit ch7025_exit(void)
{
    i2c_del_driver(&ch7025_driver);
}

module_init(ch7025_init);
module_exit(ch7025_exit);
