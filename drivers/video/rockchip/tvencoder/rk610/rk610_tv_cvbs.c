#include <linux/ctype.h>
#include <linux/string.h>
#include "rk610_tv.h"

#define USE_RGB2CCIR

#ifdef USE_RGB2CCIR
static const struct fb_videomode rk610_cvbs_mode [] = {
		//name				refresh		xres	yres	pixclock	h_bp	h_fp	v_bp	v_fp	h_pw	v_pw	polariry	PorI	flag
	{	"NTSC",				60,			720,	480,	27000000,	116,	16,		25,		14,		6,		6,		0,			1,		OUT_P888	},
	{	"PAL",				50,			720,	576,	27000000,	126,	12,		37,		6,		6,		6,		0,			1,		OUT_P888	},
};
#else
static const struct fb_videomode rk610_cvbs_mode [] = {
		//name				refresh		xres	yres	pixclock	h_bp	h_fp	v_bp	v_fp	h_pw	v_pw	polariry	PorI	flag
	{	"NTSC",				60,			720,	480,	27000000,	116,	16,		16,		3,		6,		3,		0,			1,		OUT_CCIR656	},
	{	"PAL",				50,			720,	576,	27000000,	126,	12,		19,		2,		6,		3,		0,			1,		OUT_CCIR656	},
};
#endif

static struct rk610_monspecs cvbs_monspecs;


int rk610_tv_cvbs_init(void)
{
	unsigned char TVE_Regs[9];
	unsigned char TVE_CON_Reg;
	int ret, i;
	
	rk610_tv_wirte_reg(TVE_HDTVCR, TVE_RESET);

	memset(TVE_Regs, 0, 9);
	TVE_CON_Reg = TVE_CONTROL_CVBS_3_CHANNEL_ENALBE;
	TVE_Regs[TVE_VINCR] 	=	TVE_VINCR_PIX_DATA_DELAY(0) | TVE_VINCR_H_SYNC_POLARITY_NEGTIVE | TVE_VINCR_V_SYNC_POLARITY_NEGTIVE | TVE_VINCR_VSYNC_FUNCTION_VSYNC;
	TVE_Regs[TVE_POWERCR]	=	TVE_DAC_Y_ENABLE | TVE_DAC_U_ENABLE | TVE_DAC_V_ENABLE;
	TVE_Regs[TVE_VOUTCR]	=	TVE_VOUTCR_OUTPUT_CVBS;
	TVE_Regs[TVE_YADJCR]	=	0x17;
	TVE_Regs[TVE_YCBADJCR]	=	0x10;
	TVE_Regs[TVE_YCRADJCR]	=	0x10;
	
	switch(rk610.mode) {
		case TVOUT_CVBS_NTSC:
			TVE_Regs[TVE_VFCR] 		= TVE_VFCR_ENABLE_SUBCARRIER_RESET | TVE_VFCR_VIN_RANGE_16_235 | TVE_VFCR_BLACK_7_5_IRE | TVE_VFCR_NTSC;
			#ifdef USE_RGB2CCIR
			TVE_Regs[TVE_VINCR]		|=	TVE_VINCR_INPUT_FORMAT(INPUT_FORMAT_BT601_SLAVE);
			TVE_Regs[TVE_HDTVCR]	=	TVE_FILTER(0) | TVE_COLOR_CONVERT_REC601 | TVE_INPUT_DATA_RGB | TVE_OUTPUT_MODE_PAL_NTSC;
			TVE_CON_Reg |= RGB2CCIR_INPUT_DATA_FORMAT(0) | RGB2CCIR_RGB_SWAP_DISABLE | RGB2CCIR_INPUT_PROGRESSIVE | RGB2CCIR_CVBS_NTSC | RGB2CCIR_ENABLE;
			#else
			TVE_Regs[TVE_VINCR]		|=	TVE_VINCR_INPUT_FORMAT(INPUT_FORMAT_BT656);
			TVE_Regs[TVE_HDTVCR]	=	TVE_FILTER(0) | TVE_INPUT_DATA_YUV | TVE_OUTPUT_MODE_PAL_NTSC;
			#endif			
			break;
		case TVOUT_CVBS_PAL:
			TVE_Regs[TVE_VFCR] 		= TVE_VFCR_ENABLE_SUBCARRIER_RESET | TVE_VFCR_VIN_RANGE_16_235 | TVE_VFCR_BLACK_0_IRE | TVE_VFCR_PAL_B_N;		
			#ifdef USE_RGB2CCIR
			TVE_Regs[TVE_VINCR]		|=	TVE_VINCR_INPUT_FORMAT(INPUT_FORMAT_BT601_SLAVE);
			TVE_Regs[TVE_HDTVCR]	=	TVE_FILTER(0) | TVE_COLOR_CONVERT_REC601 | TVE_INPUT_DATA_RGB | TVE_OUTPUT_MODE_PAL_NTSC;
			TVE_CON_Reg |= RGB2CCIR_INPUT_DATA_FORMAT(0) | RGB2CCIR_RGB_SWAP_DISABLE | RGB2CCIR_INPUT_PROGRESSIVE | RGB2CCIR_CVBS_PAL | RGB2CCIR_ENABLE;
			#else
			TVE_Regs[TVE_VINCR]		|=	TVE_VINCR_INPUT_FORMAT(INPUT_FORMAT_BT656);
			TVE_Regs[TVE_HDTVCR]	=	TVE_FILTER(0) | TVE_INPUT_DATA_YUV | TVE_OUTPUT_MODE_PAL_NTSC;
			#endif			
			break;
		default:
			return -1;
	}
	
	for(i = 0; i < sizeof(TVE_Regs); i++){
//		printk(KERN_ERR "reg[%d] = 0x%02x\n", i, TVE_Regs[i]);
		ret = rk610_tv_wirte_reg(i, TVE_Regs[i]);
		if(ret < 0){
			printk(KERN_ERR "rk610_tv_wirte_reg %d err!\n", i);
			return ret;
		}
	}
//	printk(KERN_ERR "TVE_CON_Reg = 0x%02x\n", TVE_CON_Reg);
	rk610_control_send_byte(TVE_CON, TVE_CON_Reg);
	#ifdef USE_RGB2CCIR
	rk610_control_send_byte(CCIR_RESET, 0x01);
	#endif
	return 0;
}

static int rk610_cvbs_set_enable(struct rk_display_device *device, int enable)
{
	if(cvbs_monspecs.enable != enable || cvbs_monspecs.mode_set != rk610.mode)
	{
		if(enable == 0 && cvbs_monspecs.enable)
		{
			cvbs_monspecs.enable = 0;
			rk610_tv_standby(RK610_TVOUT_CVBS);
		}
		else if(enable == 1)
		{
			rk610_switch_fb(cvbs_monspecs.mode, cvbs_monspecs.mode_set);
			cvbs_monspecs.enable = 1;
		}
	}
	return 0;
}

static int rk610_cvbs_get_enable(struct rk_display_device *device)
{
	return cvbs_monspecs.enable;
}

static int rk610_cvbs_get_status(struct rk_display_device *device)
{
	if(rk610.mode < TVOUT_YPbPr_720x480p_60)
		return 1;
	else
		return 0;
}

static int rk610_cvbs_get_modelist(struct rk_display_device *device, struct list_head **modelist)
{
	*modelist = &(cvbs_monspecs.modelist);
	return 0;
}

static int rk610_cvbs_set_mode(struct rk_display_device *device, struct fb_videomode *mode)
{
	int i;

	for(i = 0; i < ARRAY_SIZE(rk610_cvbs_mode); i++)
	{
		if(fb_mode_is_equal(&rk610_cvbs_mode[i], mode))
		{	
			if( ((i + 1) != rk610.mode) )
			{
				cvbs_monspecs.mode_set = i + 1;
				cvbs_monspecs.mode = (struct fb_videomode *)&rk610_cvbs_mode[i];
			}
			return 0;
		}
	}
	
	return -1;
}

static int rk610_cvbs_get_mode(struct rk_display_device *device, struct fb_videomode *mode)
{
	*mode = *(cvbs_monspecs.mode);
	return 0;
}

static struct rk_display_ops rk610_cvbs_display_ops = {
	.setenable = rk610_cvbs_set_enable,
	.getenable = rk610_cvbs_get_enable,
	.getstatus = rk610_cvbs_get_status,
	.getmodelist = rk610_cvbs_get_modelist,
	.setmode = rk610_cvbs_set_mode,
	.getmode = rk610_cvbs_get_mode,
};

static int rk610_display_cvbs_probe(struct rk_display_device *device, void *devdata)
{
	device->owner = THIS_MODULE;
	strcpy(device->type, "TV");
	device->name = "cvbs";
	device->priority = DISPLAY_PRIORITY_TV;
	device->property = rk610.property;
	device->priv_data = devdata;
	device->ops = &rk610_cvbs_display_ops;
	return 1;
}

static struct rk_display_driver display_rk610_cvbs = {
	.probe = rk610_display_cvbs_probe,
};

int rk610_register_display_cvbs(struct device *parent)
{
	int i;
	
	memset(&cvbs_monspecs, 0, sizeof(struct rk610_monspecs));
	INIT_LIST_HEAD(&cvbs_monspecs.modelist);
	for(i = 0; i < ARRAY_SIZE(rk610_cvbs_mode); i++)
		display_add_videomode(&rk610_cvbs_mode[i], &cvbs_monspecs.modelist);
	if(rk610.mode < TVOUT_YPbPr_720x480p_60) {
		cvbs_monspecs.mode = (struct fb_videomode *)&(rk610_cvbs_mode[rk610.mode - 1]);
		cvbs_monspecs.mode_set = rk610.mode;
	}
	else {
		cvbs_monspecs.mode = (struct fb_videomode *)&(rk610_cvbs_mode[0]);
		cvbs_monspecs.mode_set = TVOUT_CVBS_NTSC;
	}
	cvbs_monspecs.ddev = rk_display_device_register(&display_rk610_cvbs, parent, NULL);
	rk610.cvbs = &cvbs_monspecs;
	if(rk610.mode < TVOUT_YPbPr_720x480p_60)
		rk_display_device_enable(cvbs_monspecs.ddev);
	return 0;
}
