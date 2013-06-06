#ifndef _RK1000_TV_H
#define _RK1000_TV_H
#include <linux/fb.h>
#include <linux/display-sys.h>
#include <linux/rk_fb.h>
#include <linux/rk_screen.h>
#include <mach/board.h>
#include <mach/gpio.h>
#include <mach/iomux.h>
#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
#endif

/******************* TVOUT OUTPUT TYPE **********************/
struct rk1000_monspecs {
	struct rk_display_device	*ddev;
	unsigned int				enable;
	struct fb_videomode			*mode;
	struct list_head			modelist;
	unsigned int 				mode_set;
};

struct rk1000 {
	struct device		*dev;
	struct i2c_client	*client;
	#ifdef CONFIG_HAS_EARLYSUSPEND
	struct early_suspend	early_suspend;
	#endif
	int		io_switch_pin;
	int		video_source;
	int		property;
	int		mode;
	struct rk1000_monspecs *cvbs;
	struct rk1000_monspecs *ypbpr;
};

extern struct rk1000 rk1000;

enum {
	TVOUT_CVBS_NTSC = 1,
	TVOUT_CVBS_PAL,
	TVOUT_YPbPr_720x480p_60,
	TVOUT_YPbPr_720x576p_50,
	TVOUT_YPbPr_1280x720p_50,
	TVOUT_YPbPr_1280x720p_60
};

enum {
	RK1000_TVOUT_CVBS = 0,
	RK1000_TVOUT_YC,
	RK1000_TVOUT_YPBPR,
};

#define RK1000_TVOUT_DEAULT TVOUT_CVBS_NTSC

extern int rk1000_control_write_block(u8 addr, u8 *buf, u8 len);
extern int rk1000_tv_write_block(u8 addr, u8 *buf, u8 len);
extern int rk1000_tv_standby(int type);
extern int rk1000_switch_fb(const struct fb_videomode *modedb, int tv_mode);
extern int rk1000_register_display(struct device *parent);

#ifdef CONFIG_RK1000_TVOUT_YPbPr
extern int rk1000_tv_Ypbpr480_init(void);
extern int rk1000_tv_Ypbpr576_init(void);
extern int rk1000_tv_Ypbpr720_50_init(void);
extern int rk1000_tv_Ypbpr720_60_init(void);
extern int rk1000_register_display_YPbPr(struct device *parent);
#endif

#ifdef CONFIG_RK1000_TVOUT_CVBS
extern int rk1000_tv_ntsc_init(void);
extern int rk1000_tv_pal_init(void);
extern int rk1000_register_display_cvbs(struct device *parent);
#endif

#endif

