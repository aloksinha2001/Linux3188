#ifndef __CH7025_7026_H__
#define __CH7025_7026_H__
#include <mach/gpio.h>
#include <linux/display-sys.h>
#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
#endif
#include "ch7025_regs.h"


#define CH7025_I2C_RATE			100000

#define VIDEO_SWITCH_CVBS		GPIO_LOW
#define VIDEO_SWITCH_OTHER		GPIO_HIGH

enum {
	TVOUT_CVBS_NTSC = 1,
	TVOUT_CVBS_PAL,
	TVOUT_YPbPr_720x480p_60,
	TVOUT_YPbPr_720x576p_50,
	TVOUT_YPbPr_1280x720p_50,
	TVOUT_YPbPr_1280x720p_60
};

/* CH7025 default output mode */
#define CH7025_DEFAULT_MODE		TVOUT_CVBS_NTSC

struct ch7025_monspecs {
	struct rk_display_device	*ddev;
	unsigned int				enable;
	struct fb_videomode			*mode;
	struct list_head			modelist;
	unsigned int 				mode_set;
};

struct ch7025 {
	struct device		*dev;
	struct i2c_client	*client;
	#ifdef CONFIG_HAS_EARLYSUSPEND
	struct early_suspend	early_suspend;
	#endif
	int		io_pwr_pin;
	int		io_rst_pin;
	int		io_switch_pin;
	int		video_source;
	int		property;
	int 	chip_id;	//0x55 for ch7025; 0x54 for ch7026
	int		mode;
	struct ch7025_monspecs *cvbs;
	struct ch7025_monspecs *ypbpr;
};

extern struct ch7025 ch7025;

//#define DEBUG

#ifdef DEBUG
#define DBG(format, ...) \
		printk(KERN_INFO "CH7025/7026: " format "\n", ## __VA_ARGS__)
#else
#define DBG(format, ...)
#endif

extern int ch7025_write_reg(char reg, char value);
extern int ch7025_read_reg(char reg);
extern int ch7025_switch_fb(const struct fb_videomode *modedb, int mode);
extern void ch7025_standby(void);
#ifdef CONFIG_CH7025_7026_TVOUT_CVBS
extern int ch7025_register_display_cvbs(struct device *parent);
#endif
#ifdef CONFIG_CH7025_7026_TVOUT_YPBPR
extern int ch7025_register_display_ypbpr(struct device *parent);
#endif
#endif //__CH7025_7026_H__