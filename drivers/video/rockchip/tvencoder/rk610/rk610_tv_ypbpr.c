#include <linux/ctype.h>
#include <linux/string.h>
#include "rk610_tv.h"

#define E(fmt, arg...) printk("<3>!!!%s:%d: " fmt, __FILE__, __LINE__, ##arg)

#define FILTER_LEVEL	0
static const struct fb_videomode rk610_YPbPr_mode [] = {
		//name				refresh		xres	yres	pixclock	h_bp	h_fp	v_bp	v_fp	h_pw	v_pw	polariry	PorI	flag
	{	"YPbPr480p",		60,			720,	480,	27000000,	55,		19,		37,		5,		64,		5,		0,			0,		OUT_P888	},
	{	"YPbPr576p",		50,			720,	576,	27000000,	68,		12,		39,		5,		64,		5,		0,			0,		OUT_P888	},
	{	"YPbPr720p@50",		50,			1280,	720,	74250000,	600,	0,		20,		5,		100,	5,		0,			0,		OUT_P888	},
	{	"YPbPr720p@60",		60,			1280,	720,	74250000,	270,	0,		20,		5,		100,	5,		0,			0,		OUT_P888	},
	//{	"YPbPr1080i@50",	50,			1920,	1080,	148500000,	620,	0,		15,		2,		100,	5,		0,			1,		OUT_CCIR656	},
	{	"YPbPr1080i@60",	60,			1920,	1080,	148500000,	180,	0,		15,		2,		100,	5,		0,			1,		OUT_CCIR656	},
	{	"YPbPr1080p@50",	50,			1920,	1080,	148500000,	620,	0,		36,		4,		100,	5,		0,			0,		OUT_P888	},
	{	"YPbPr1080p@60",	60,			1920,	1080,	148500000,	180,	0,		36,		4,		100,	5,		0,			0,		OUT_P888	},
};

static struct rk610_monspecs ypbpr_monspecs;

int rk610_tv_ypbpr_init(void)
{
	unsigned char TVE_Regs[9];
	unsigned char TVE_CON_Reg;
	int i, ret;
	
	rk610_tv_wirte_reg(TVE_HDTVCR, TVE_RESET);
	memset(TVE_Regs, 0, 9);	
	
	TVE_CON_Reg = 0x00;
	
	TVE_Regs[TVE_VINCR] 	=	TVE_VINCR_PIX_DATA_DELAY(0) | TVE_VINCR_H_SYNC_POLARITY_NEGTIVE | TVE_VINCR_V_SYNC_POLARITY_NEGTIVE | TVE_VINCR_VSYNC_FUNCTION_VSYNC;
	TVE_Regs[TVE_POWERCR]	=	TVE_DAC_CLK_INVERSE_DISABLE | TVE_DAC_Y_ENABLE | TVE_DAC_U_ENABLE | TVE_DAC_V_ENABLE;
	TVE_Regs[TVE_VOUTCR]	=	TVE_VOUTCR_OUTPUT_YPBPR;
	TVE_Regs[TVE_YADJCR]	=	0x17;
	TVE_Regs[TVE_YCBADJCR]	=	0x10;
	TVE_Regs[TVE_YCRADJCR]	=	0x10;
	
	switch(rk610.mode)
	{
		case TVOUT_YPbPr_720x480p_60:
			TVE_Regs[TVE_VFCR]		=	TVE_VFCR_BLACK_0_IRE;
			TVE_Regs[TVE_VINCR]		|=	TVE_VINCR_INPUT_FORMAT(INPUT_FORMAT_BT601_SLAVE);
			TVE_Regs[TVE_HDTVCR]	=	TVE_FILTER(FILTER_LEVEL) | TVE_COLOR_CONVERT_REC601 | TVE_INPUT_DATA_RGB | TVE_OUTPUT_60HZ | TVE_OUTPUT_MODE_480P;
			break;
		case TVOUT_YPbPr_720x576p_50:
			TVE_Regs[TVE_VFCR]		=	TVE_VFCR_BLACK_0_IRE | TVE_VFCR_PAL_NC;
			TVE_Regs[TVE_VINCR]		|=	TVE_VINCR_INPUT_FORMAT(INPUT_FORMAT_BT601_SLAVE);
			TVE_Regs[TVE_HDTVCR]	=	TVE_FILTER(FILTER_LEVEL) | TVE_COLOR_CONVERT_REC601 | TVE_INPUT_DATA_RGB | TVE_OUTPUT_50HZ | TVE_OUTPUT_MODE_576P;
			break;
		case TVOUT_YPbPr_1280x720p_50:
			TVE_Regs[TVE_VFCR]		=	TVE_VFCR_BLACK_0_IRE | TVE_VFCR_PAL_NC;
			TVE_Regs[TVE_VINCR]		|=	TVE_VINCR_INPUT_FORMAT(INPUT_FORMAT_BT601_SLAVE);
			TVE_Regs[TVE_HDTVCR]	=	TVE_FILTER(FILTER_LEVEL) | TVE_COLOR_CONVERT_REC709 | TVE_INPUT_DATA_RGB | TVE_OUTPUT_50HZ | TVE_OUTPUT_MODE_720P;
			break;
		case TVOUT_YPbPr_1280x720p_60:
			TVE_Regs[TVE_VFCR]		=	TVE_VFCR_BLACK_0_IRE | TVE_VFCR_PAL_NC;
			TVE_Regs[TVE_VINCR]		|=	TVE_VINCR_INPUT_FORMAT(INPUT_FORMAT_BT601_SLAVE);
			TVE_Regs[TVE_HDTVCR]	=	TVE_FILTER(FILTER_LEVEL) | TVE_COLOR_CONVERT_REC709 | TVE_INPUT_DATA_RGB | TVE_OUTPUT_60HZ | TVE_OUTPUT_MODE_720P;
			break;
		/*case TVOUT_YPbPr_1920x1080i_50:
			TVE_Regs[TVE_VFCR]		=	TVE_VFCR_BLACK_0_IRE | TVE_VFCR_PAL_NC;
			TVE_Regs[TVE_VINCR]		|=	TVE_VINCR_INPUT_FORMAT(INPUT_FORMAT_BT656);
			TVE_Regs[TVE_HDTVCR]	=	TVE_FILTER(FILTER_LEVEL) | TVE_INPUT_DATA_YUV | TVE_OUTPUT_50HZ;
			TVE_Regs[TVE_YADJCR]	|=	TVE_OUTPUT_MODE_1080I;
			break;
			*/
		case TVOUT_YPbPr_1920x1080i_60:
			TVE_Regs[TVE_VFCR]		=	TVE_VFCR_BLACK_0_IRE | TVE_VFCR_PAL_NC;
			TVE_Regs[TVE_VINCR]		|=	TVE_VINCR_INPUT_FORMAT(INPUT_FORMAT_BT656);
			TVE_Regs[TVE_HDTVCR]	=	TVE_FILTER(FILTER_LEVEL) | TVE_INPUT_DATA_YUV | TVE_OUTPUT_60HZ;
			TVE_Regs[TVE_YADJCR]	|=	TVE_OUTPUT_MODE_1080I;
			break;
		case TVOUT_YPbPr_1920x1080p_50:
			TVE_Regs[TVE_VFCR]		=	TVE_VFCR_BLACK_0_IRE | TVE_VFCR_PAL_NC;
			TVE_Regs[TVE_VINCR]		|=	TVE_VINCR_INPUT_FORMAT(INPUT_FORMAT_BT601_SLAVE);
			TVE_Regs[TVE_HDTVCR]	=	TVE_FILTER(FILTER_LEVEL) | TVE_COLOR_CONVERT_REC709 | TVE_INPUT_DATA_RGB | TVE_OUTPUT_50HZ;
			TVE_Regs[TVE_YADJCR]	|=	TVE_OUTPUT_MODE_1080P;
			break;
		case TVOUT_YPbPr_1920x1080p_60:
			TVE_Regs[TVE_VFCR]		=	TVE_VFCR_BLACK_0_IRE | TVE_VFCR_PAL_NC;
			TVE_Regs[TVE_VINCR]		|=	TVE_VINCR_INPUT_FORMAT(INPUT_FORMAT_BT601_SLAVE);
			TVE_Regs[TVE_HDTVCR]	=	TVE_FILTER(FILTER_LEVEL) | TVE_COLOR_CONVERT_REC709 | TVE_INPUT_DATA_RGB | TVE_OUTPUT_60HZ;
			TVE_Regs[TVE_YADJCR]	|=	TVE_OUTPUT_MODE_1080P;
			break;
		default:
			return -1;
	}
	
	rk610_control_send_byte(TVE_CON, TVE_CON_Reg);
	
	for(i = 0; i < sizeof(TVE_Regs); i++){
//		printk(KERN_ERR "reg[%d] = 0x%02x\n", i, TVE_Regs[i]);
		ret = rk610_tv_wirte_reg(i, TVE_Regs[i]);
		if(ret < 0){
			E("rk610_tv_wirte_reg %d err!\n", i);
			return ret;
		}
	}
	return 0;
}

static int rk610_ypbpr_set_enable(struct rk_display_device *device, int enable)
{
	if(ypbpr_monspecs.enable != enable || ypbpr_monspecs.mode_set != rk610.mode)
	{
		if(enable == 0 && ypbpr_monspecs.enable)
		{
			ypbpr_monspecs.enable = 0;
			rk610_tv_standby(RK610_TVOUT_YPBPR);
		}
		else if(enable == 1)
		{
			rk610_switch_fb(ypbpr_monspecs.mode, ypbpr_monspecs.mode_set);
			ypbpr_monspecs.enable = 1;
		}
	}
	return 0;
}

static int rk610_ypbpr_get_enable(struct rk_display_device *device)
{
	return ypbpr_monspecs.enable;
}

static int rk610_ypbpr_get_status(struct rk_display_device *device)
{
	if(rk610.mode > TVOUT_CVBS_PAL)
		return 1;
	else
		return 0;
}

static int rk610_ypbpr_get_modelist(struct rk_display_device *device, struct list_head **modelist)
{
	*modelist = &(ypbpr_monspecs.modelist);
	return 0;
}

static int rk610_ypbpr_set_mode(struct rk_display_device *device, struct fb_videomode *mode)
{
	int i;

	for(i = 0; i < ARRAY_SIZE(rk610_YPbPr_mode); i++)
	{
		if(fb_mode_is_equal(&rk610_YPbPr_mode[i], mode))
		{	
			if( (i + 3) != rk610.mode )
			{
				ypbpr_monspecs.mode_set = i + 3;
				ypbpr_monspecs.mode = (struct fb_videomode *)&rk610_YPbPr_mode[i];
			}
			return 0;
		}
	}
	
	return -1;
}

static int rk610_ypbpr_get_mode(struct rk_display_device *device, struct fb_videomode *mode)
{
	*mode = *(ypbpr_monspecs.mode);
	return 0;
}

static struct rk_display_ops rk610_ypbpr_display_ops = {
	.setenable = rk610_ypbpr_set_enable,
	.getenable = rk610_ypbpr_get_enable,
	.getstatus = rk610_ypbpr_get_status,
	.getmodelist = rk610_ypbpr_get_modelist,
	.setmode = rk610_ypbpr_set_mode,
	.getmode = rk610_ypbpr_get_mode,
};

static int rk610_display_YPbPr_probe(struct rk_display_device *device, void *devdata)
{
	device->owner = THIS_MODULE;
	strcpy(device->type, "YPbPr");
	device->name = "ypbpr";
	device->priority = DISPLAY_PRIORITY_YPbPr;
	device->property = rk610.property;
	device->priv_data = devdata;
	device->ops = &rk610_ypbpr_display_ops;
	return 1;
}

static struct rk_display_driver display_rk610_YPbPr = {
	.probe = rk610_display_YPbPr_probe,
};

int rk610_register_display_ypbpr(struct device *parent)
{
	int i;
	
	memset(&ypbpr_monspecs, 0, sizeof(struct rk610_monspecs));
	INIT_LIST_HEAD(&ypbpr_monspecs.modelist);
	for(i = 0; i < ARRAY_SIZE(rk610_YPbPr_mode); i++)
		display_add_videomode(&rk610_YPbPr_mode[i], &ypbpr_monspecs.modelist);
	if(rk610.mode > TVOUT_CVBS_PAL) {
		ypbpr_monspecs.mode = (struct fb_videomode *)&(rk610_YPbPr_mode[rk610.mode - 3]);
		ypbpr_monspecs.mode_set = rk610.mode;
	}
	else {
		ypbpr_monspecs.mode = (struct fb_videomode *)&(rk610_YPbPr_mode[3]);
		ypbpr_monspecs.mode_set = TVOUT_YPbPr_1280x720p_60;
	}
	ypbpr_monspecs.ddev = rk_display_device_register(&display_rk610_YPbPr, parent, NULL);
	rk610.ypbpr = &ypbpr_monspecs;
	if(rk610.mode > TVOUT_CVBS_PAL)
		rk_display_device_enable(ypbpr_monspecs.ddev);
	return 0;
}
