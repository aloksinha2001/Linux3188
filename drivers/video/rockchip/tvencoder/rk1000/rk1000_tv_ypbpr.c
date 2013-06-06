#include <linux/ctype.h>
#include <linux/string.h>
#include "rk1000_tv.h"

#define E(fmt, arg...) printk("<3>!!!%s:%d: " fmt, __FILE__, __LINE__, ##arg)

static const struct fb_videomode rk1000_YPbPr_mode [] = {
	{	"YPbPr480p",		60,			720,	480,	27000000,	70,		6,		30,		9,		62,		6,		0,			0,		0	},
	{	"YPbPr576p",		50,			720,	576,	27000000,	74,		6,		39,		5,		64,		5,		0,			0,		0	},
	{	"YPbPr720p@50",		50,			1280,	720,	74250000,	660,	0,		20,		5,		40,		5,		0,			0,		0	},
	{	"YPbPr720p@60",		60,			1280,	720,	74250000,	330,	0,		20,		5,		40,		5,		0,			0,		0	},
};

static struct rk1000_monspecs ypbpr_monspecs;

int rk1000_tv_Ypbpr480_init(void)
{
	unsigned char Tv_encoder_regs[] = {0x00, 0x00, 0x40, 0x08, 0x00, 0x02, 0x17, 0x0A, 0x0A};
	unsigned char Tv_encoder_control_regs[] = {0x00};
	int i;
	int ret;
	
	for(i=0; i<sizeof(Tv_encoder_regs); i++){
		ret = rk1000_tv_write_block(i, Tv_encoder_regs+i, 1);
		if(ret < 0){
			E("rk1000_tv_write_block err!\n");
			return ret;
		}
	}

	for(i=0; i<sizeof(Tv_encoder_control_regs); i++){
		ret = rk1000_control_write_block(i+3, Tv_encoder_control_regs+i, 1);
		if(ret < 0){
			E("rk1000_control_write_block err!\n");
			return ret;
		}
	}

	return 0;
}

int rk1000_tv_Ypbpr576_init(void)
{
	unsigned char Tv_encoder_regs[] = {0x06, 0x00, 0x40, 0x08, 0x00, 0x01, 0x17, 0x0A, 0x0A};
	unsigned char Tv_encoder_control_regs[] = {0x00};
	int i;
	int ret;
	
	for(i=0; i<sizeof(Tv_encoder_regs); i++){
		ret = rk1000_tv_write_block(i, Tv_encoder_regs+i, 1);
		if(ret < 0){
			E("rk1000_tv_write_block err!\n");
			return ret;
		}
	}

	for(i=0; i<sizeof(Tv_encoder_control_regs); i++){
		ret = rk1000_control_write_block(i+3, Tv_encoder_control_regs+i, 1);
		if(ret < 0){
			E("rk1000_control_write_block err!\n");
			return ret;
		}
	}

	return 0;
}

int rk1000_tv_Ypbpr720_50_init(void)
{
	unsigned char Tv_encoder_regs[] = {0x06, 0x00, 0x40, 0x08, 0x00, 0x13, 0x17, 0x0A, 0x0A};
	unsigned char Tv_encoder_control_regs[] = {0x00};
	int i;
	int ret;
	
	for(i=0; i<sizeof(Tv_encoder_regs); i++){
		ret = rk1000_tv_write_block(i, Tv_encoder_regs+i, 1);
		if(ret < 0){
			E("rk1000_tv_write_block err!\n");
			return ret;
		}
	}

	for(i=0; i<sizeof(Tv_encoder_control_regs); i++){
		ret = rk1000_control_write_block(i+3, Tv_encoder_control_regs+i, 1);
		if(ret < 0){
			E("rk1000_control_write_block err!\n");
			return ret;
		}
	}

	return 0;
}

int rk1000_tv_Ypbpr720_60_init(void)
{
	unsigned char Tv_encoder_regs[] = {0x06, 0x00, 0x40, 0x08, 0x00, 0x17, 0x17, 0x0A, 0x0A};
	unsigned char Tv_encoder_control_regs[] = {0x00};
	int i;
	int ret;
	
	for(i=0; i<sizeof(Tv_encoder_regs); i++){
		ret = rk1000_tv_write_block(i, Tv_encoder_regs+i, 1);
		if(ret < 0){
			E("rk1000_tv_write_block err!\n");
			return ret;
		}
	}

	for(i=0; i<sizeof(Tv_encoder_control_regs); i++){
		ret = rk1000_control_write_block(i+3, Tv_encoder_control_regs+i, 1);
		if(ret < 0){
			E("rk1000_control_write_block err!\n");
			return ret;
		}
	}

	return 0;
}

static int rk1000_ypbpr_set_enable(struct rk_display_device *device, int enable)
{
	if(ypbpr_monspecs.enable != enable || ypbpr_monspecs.mode_set != rk1000.mode)
	{
		if(enable == 0 && ypbpr_monspecs.enable)
		{
			ypbpr_monspecs.enable = 0;
			rk1000_tv_standby(RK1000_TVOUT_YPBPR);
		}
		else if(enable == 1)
		{
			rk1000_switch_fb(ypbpr_monspecs.mode, ypbpr_monspecs.mode_set);
			ypbpr_monspecs.enable = 1;
		}
	}
	return 0;
}

static int rk1000_ypbpr_get_enable(struct rk_display_device *device)
{
	return ypbpr_monspecs.enable;
}

static int rk1000_ypbpr_get_status(struct rk_display_device *device)
{
	if(rk1000.mode > TVOUT_CVBS_PAL)
		return 1;
	else
		return 0;
}

static int rk1000_ypbpr_get_modelist(struct rk_display_device *device, struct list_head **modelist)
{
	*modelist = &(ypbpr_monspecs.modelist);
	return 0;
}

static int rk1000_ypbpr_set_mode(struct rk_display_device *device, struct fb_videomode *mode)
{
	int i;

	for(i = 0; i < ARRAY_SIZE(rk1000_YPbPr_mode); i++)
	{
		if(fb_mode_is_equal(&rk1000_YPbPr_mode[i], mode))
		{	
			if( (i + 3) != rk1000.mode )
			{
				ypbpr_monspecs.mode_set = i + 3;
				ypbpr_monspecs.mode = (struct fb_videomode *)&rk1000_YPbPr_mode[i];
			}
			return 0;
		}
	}
	
	return -1;
}

static int rk1000_ypbpr_get_mode(struct rk_display_device *device, struct fb_videomode *mode)
{
	*mode = *(ypbpr_monspecs.mode);
	return 0;
}

static struct rk_display_ops rk1000_ypbpr_display_ops = {
	.setenable = rk1000_ypbpr_set_enable,
	.getenable = rk1000_ypbpr_get_enable,
	.getstatus = rk1000_ypbpr_get_status,
	.getmodelist = rk1000_ypbpr_get_modelist,
	.setmode = rk1000_ypbpr_set_mode,
	.getmode = rk1000_ypbpr_get_mode,
};

static int rk1000_display_YPbPr_probe(struct rk_display_device *device, void *devdata)
{
	device->owner = THIS_MODULE;
	strcpy(device->type, "YPbPr");
	device->name = "ypbpr";
	device->priority = DISPLAY_PRIORITY_YPbPr;
	device->property = rk1000.property;
	device->priv_data = devdata;
	device->ops = &rk1000_ypbpr_display_ops;
	return 1;
}

static struct rk_display_driver display_rk1000_YPbPr = {
	.probe = rk1000_display_YPbPr_probe,
};

int rk1000_register_display_YPbPr(struct device *parent)
{
	int i;
	
	memset(&ypbpr_monspecs, 0, sizeof(struct rk1000_monspecs));
	INIT_LIST_HEAD(&ypbpr_monspecs.modelist);
	for(i = 0; i < ARRAY_SIZE(rk1000_YPbPr_mode); i++)
		display_add_videomode(&rk1000_YPbPr_mode[i], &ypbpr_monspecs.modelist);
	if(rk1000.mode > TVOUT_CVBS_PAL) {
		ypbpr_monspecs.mode = (struct fb_videomode *)&(rk1000_YPbPr_mode[rk1000.mode - 3]);
		ypbpr_monspecs.mode_set = rk1000.mode;
	}
	else {
		ypbpr_monspecs.mode = (struct fb_videomode *)&(rk1000_YPbPr_mode[3]);
		ypbpr_monspecs.mode_set = TVOUT_YPbPr_1280x720p_60;
	}
	ypbpr_monspecs.ddev = rk_display_device_register(&display_rk1000_YPbPr, parent, NULL);
	rk1000.cvbs = &ypbpr_monspecs;
	if(rk1000.mode > TVOUT_CVBS_PAL)
		rk_display_device_enable(ypbpr_monspecs.ddev);
	return 0;
}
