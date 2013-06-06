#ifndef __RK30_HDMI_H__
#define __RK30_HDMI_H__

#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
#endif
#include "../rk_hdmi.h"

#ifdef DEBUG
#define RK30DBG(format, ...) \
		printk(KERN_INFO "RK30 HDMI: " format "\n", ## __VA_ARGS__)
#else
#define RK30DBG(format, ...)
#endif

/* default HDMI video source */
#if defined(CONFIG_LCDC0_RK30) && defined(CONFIG_LCDC1_RK30)
#define HDMI_SOURCE_DEFAULT		DISPLAY_SOURCE_LCDC1
#else
#define HDMI_SOURCE_DEFAULT		DISPLAY_SOURCE_LCDC0
#endif

extern struct hdmi* rk30_hdmi_register_hdcp_callbacks(
					 void (*hdcp_cb)(void),
					 void (*hdcp_irq_cb)(int status),
					 int  (*hdcp_power_on_cb)(void),
					 void (*hdcp_power_off_cb)(void));
					 
struct rk30_hdmi {
	struct clk		*hclk;				//HDMI AHP clk
	int 			regbase;
	int				irq;
	int				regbase_phy;
	int				regsize_phy;
	struct hdmi		*hdmi;
	
	int				pwr_mode;
	
	int				tmdsclk;

	spinlock_t		irq_lock;

#ifdef CONFIG_HAS_EARLYSUSPEND
	struct early_suspend	early_suspend;
#endif
};

#endif /* __RK30_HDMI_H__ */
