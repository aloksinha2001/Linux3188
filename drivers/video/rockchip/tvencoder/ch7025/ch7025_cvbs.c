#include <linux/ctype.h>
#include <linux/string.h>
#include <linux/rk_fb.h>
#include "ch7025.h"

static struct ch7025_monspecs	cvbs_monspecs;

static const struct fb_videomode cvbs_mode [] = {
	//name				refresh		xres	yres	pixclock	h_bp	h_fp	v_bp	v_fp	h_pw	v_pw	polariry	PorI	flag(used for vic)
{	"720x480i@60Hz",	60,			720,	480,	27000000,	60,		16,		30,		9,		62,		6,		0,			1,		OUT_P888	},
{	"720x576i@50Hz",	50,			720,	576,	27000000,	68,		12,		39,		5,		64,		5,		0,			1,		OUT_P888	},
};

static const struct fb_videomode ch7025_cvbs_mode [] = {
{	"720x480i@60Hz",	60,			1440,	480,	27000000,	114,	38,		30,		9,		124,	6,		0,			1,		OUT_P888	},
{	"720x576i@50Hz",	50,			1440,	576,	27000000,	138,	24,		39,		5,		126,	5,		0,			1,		OUT_P888	},	
};

#define I2CWrite	ch7025_write_reg
#define I2Cwrite	ch7025_write_reg 

static int ch7025_cvbs_init(struct fb_videomode *mode)
{
	int temp;
	const struct fb_videomode	*outmode;
	
	DBG("%s start", __FUNCTION__);
	
	I2CWrite( 0x02, 0x01 );
	I2CWrite( 0x02, 0x03 );
	I2CWrite( 0x03, 0x00 );
	I2CWrite( 0x04, 0x39 );
	temp = v_DEPO_O(1) | v_DEPO_I(1);
	if(mode->sync & FB_SYNC_HOR_HIGH_ACT)
		temp |= v_HPO_I(1) | v_HPO_O(1);
	if(mode->sync & FB_SYNC_VERT_HIGH_ACT)
		temp |= v_VPO_I(1) | v_VPO_O(1);
	ch7025_write_reg( SYNC_CONFIG, temp & 0xFF );
	I2CWrite( 0x0A, 0x10 );
	if(ch7025.mode == TVOUT_CVBS_NTSC) {
		temp = NTSC_M;
		outmode = &ch7025_cvbs_mode[0];
	}
	else {
		temp = PAL_B_D_G_H_I;
		outmode = &ch7025_cvbs_mode[1];
	}
	I2CWrite( OUT_FORMAT, v_WRFAST(0) | v_YPBPR_ENABLE(0) | v_YC2RGB(0) | v_FMT(temp));

	// input video timing
	// hactive and htotal
	temp = mode->xres + mode->left_margin + mode->hsync_len + mode->right_margin;
	ch7025_write_reg(HTI_HAI_HIGH, v_HVAUTO(0) | v_HREPT(0) | v_HTI_HIGH((temp >> 8) & 0xFF) | v_HAI_HIGH((mode->xres >> 8) & 0xFF) );
	ch7025_write_reg(HAI_LOW, mode->xres & 0xFF);
	ch7025_write_reg(HTI_LOW, temp & 0xFF);
	
	// hsync and hoffset
	ch7025_write_reg(HW_HO_HIGH, v_HDTVEN(0) | v_EDGE_ENH(1) | v_HW_HIGH((mode->hsync_len >> 8) & 0xFF));
	if(ch7025.mode == TVOUT_CVBS_NTSC)
		ch7025_write_reg(HO_LOW, (mode->right_margin/2) & 0xFF);
	else
		ch7025_write_reg(HO_LOW, 0);
	ch7025_write_reg(HW_LOW, mode->hsync_len & 0xFF);
		
	// vactive and vtotal
	temp = mode->yres + mode->upper_margin + mode->vsync_len + mode->lower_margin;
	ch7025_write_reg(VTI_VAI_HIGH, v_FIELDSW(0) | v_TVBYPSS(0) | v_VTI_HIGH((temp >> 8) & 0xFF) | v_VAI_HIGH((mode->yres >> 8) & 0xFF) );
	ch7025_write_reg(VTI_LOW, temp & 0xFF);
	ch7025_write_reg(VAI_LOW, mode->yres & 0xFF);
	
	// vsync and voffset
	ch7025_write_reg(VW_VO_HIGH, v_VW_HIGH( (mode->vsync_len >> 8) & 0xFF) | v_VO_HIGH( (mode->lower_margin >> 8) & 0xFF));
	ch7025_write_reg(VO_LOW, mode->lower_margin & 0xFF);
	ch7025_write_reg(VW_LOW, mode->vsync_len & 0xFF);
	
	// output video timing
	// hactive and htotal
	temp = outmode->xres + outmode->left_margin + outmode->hsync_len + outmode->right_margin;
	ch7025_write_reg(HTO_HAO_HIGH, v_HTO_HIGH((temp >> 8) & 0xFF) | v_HAO_HIGH((outmode->xres >> 8) & 0xFF) );
	ch7025_write_reg(HAO_LOW, outmode->xres & 0xFF);
	ch7025_write_reg(HTO_LOW, temp & 0xFF);
	// hsync and hoffset
	ch7025_write_reg(HWO_HOO_HIGH, v_HW_HIGH((outmode->hsync_len >> 8) & 0xFF) |  v_HO_HIGH((outmode->right_margin >> 8) & 0xFF));
	ch7025_write_reg(HOO_LOW, outmode->right_margin & 0xFF);
	ch7025_write_reg(HWO_LOW, outmode->hsync_len & 0xFF);
	
//	// vactive and vtotal
	temp = outmode->yres + outmode->upper_margin + outmode->vsync_len + outmode->lower_margin;
	ch7025_write_reg(VTO_VAO_HIGH,  v_VTO_HIGH((temp >> 8) & 0xFF) | v_VAO_HIGH((outmode->yres >> 8) & 0xFF) );
	ch7025_write_reg(VTO_LOW, temp & 0xFF);
	ch7025_write_reg(VAO_LOW, outmode->yres & 0xFF);
	
	// vsync and voffset
	ch7025_write_reg(VWO_VOO_HIGH, v_VWO_HIGH( (outmode->vsync_len >> 8) & 0xFF) | v_VOO_HIGH( (outmode->lower_margin >> 8) & 0xFF));
	ch7025_write_reg(VOO_LOW, outmode->lower_margin & 0xFF);
	ch7025_write_reg(VWO_LOW, outmode->vsync_len & 0xFF);

	for(temp = 0x0F; temp < 0x27; temp++)
		DBG("%02x %02x", temp&0xFF, ch7025_read_reg(temp));
//	ch7025_write_reg(DEFLIKER_FILER, v_MONOB(1) | v_CHROM_BW(0) | v_FLIKER_LEVEL(4));

	I2CWrite( 0x41, 0x9A );
	I2CWrite( 0x4D, 0x04 );
	I2CWrite( 0x4E, 0x80 );
	I2CWrite( 0x51, 0x4B );
	I2CWrite( 0x52, 0x12 );
	I2CWrite( 0x53, 0x1B );
	I2CWrite( 0x55, 0xE5 );
	I2CWrite( 0x5E, 0x80 );
	I2CWrite( 0x7D, 0x62 );
	I2CWrite( 0x04, 0x38 );
	I2Cwrite( 0x06, 0x71 );
	
	/*
	NOTE: The following five repeated sentences are used here to wait memory initial complete, please don't remove...(you could refer to Appendix A of programming guide document (CH7025(26)B Programming Guide Rev2.03.pdf or later version) for detailed information about memory initialization! 
	*/
	I2Cwrite( 0x03, 0x00 );
	I2Cwrite( 0x03, 0x00 );
	I2Cwrite( 0x03, 0x00 );
	I2Cwrite( 0x03, 0x00 );
	I2Cwrite( 0x03, 0x00 );
	
	I2CWrite( 0x06, 0x70 );
	I2Cwrite( 0x02, 0x02 );
	I2Cwrite( 0x02, 0x03 );
//	I2Cwrite( 0x04, 0x00 );
	ch7025_write_reg(PWR_STATE1, v_PD_DAC(6));
	return 0;
}

static int cvbs_set_enable(struct rk_display_device *device, int enable)
{
	DBG("%s start enable %d", __FUNCTION__, enable);
	if(cvbs_monspecs.enable != enable || cvbs_monspecs.mode_set != ch7025.mode)
	{
		if(enable == 0 && cvbs_monspecs.enable)
		{
			cvbs_monspecs.enable = 0;
			ch7025_standby();
		}
		else if(enable == 1)
		{
			ch7025_switch_fb(cvbs_monspecs.mode, cvbs_monspecs.mode_set);
			ch7025_cvbs_init(cvbs_monspecs.mode);
			cvbs_monspecs.enable = 1;
		}
	}
	return 0;
}

static int cvbs_get_enable(struct rk_display_device *device)
{
	DBG("%s start", __FUNCTION__);
	return cvbs_monspecs.enable;
}

static int cvbs_get_status(struct rk_display_device *device)
{
	DBG("%s start", __FUNCTION__);
	if(ch7025.mode < TVOUT_YPbPr_720x480p_60)
		return 1;
	else
		return 0;
}

static int cvbs_get_modelist(struct rk_display_device *device, struct list_head **modelist)
{
	DBG("%s start", __FUNCTION__);
	*modelist = &(cvbs_monspecs.modelist);
	return 0;
}

static int cvbs_set_mode(struct rk_display_device *device, struct fb_videomode *mode)
{
	int i;
	DBG("%s start", __FUNCTION__);
	for(i = 0; i < ARRAY_SIZE(cvbs_mode); i++)
	{
		if(fb_mode_is_equal(&cvbs_mode[i], mode))
		{	
			if( ((i + 1) != ch7025.mode) )
			{
				cvbs_monspecs.mode_set = i + 1;
				cvbs_monspecs.mode = (struct fb_videomode *)&cvbs_mode[i];
			}
			return 0;
		}
	}
	
	return -1;
}

static int cvbs_get_mode(struct rk_display_device *device, struct fb_videomode *mode)
{
	DBG("%s start", __FUNCTION__);
	*mode = *(cvbs_monspecs.mode);
	return 0;
}

static struct rk_display_ops cvbs_display_ops = {
	.setenable = cvbs_set_enable,
	.getenable = cvbs_get_enable,
	.getstatus = cvbs_get_status,
	.getmodelist = cvbs_get_modelist,
	.setmode = cvbs_set_mode,
	.getmode = cvbs_get_mode,
};

static int display_cvbs_probe(struct rk_display_device *device, void *devdata)
{
	device->owner = THIS_MODULE;
	strcpy(device->type, "TV");
	device->name = "cvbs";
	device->priority = DISPLAY_PRIORITY_TV;
	device->property = ch7025.property;
	device->priv_data = devdata;
	device->ops = &cvbs_display_ops;
	return 1;
}

static struct rk_display_driver display_cvbs = {
	.probe = display_cvbs_probe,
};

int ch7025_register_display_cvbs(struct device *parent)
{
	int i;
	
	memset(&cvbs_monspecs, 0, sizeof(struct ch7025_monspecs));
	INIT_LIST_HEAD(&cvbs_monspecs.modelist);
	for(i = 0; i < ARRAY_SIZE(cvbs_mode); i++)
		display_add_videomode(&cvbs_mode[i], &cvbs_monspecs.modelist);
		
	if(ch7025.mode < TVOUT_YPbPr_720x480p_60) {
		cvbs_monspecs.mode = (struct fb_videomode *)&(cvbs_mode[ch7025.mode - 1]);
		cvbs_monspecs.mode_set = ch7025.mode;
	}
	else {
		cvbs_monspecs.mode = (struct fb_videomode *)&(cvbs_mode[0]);
		cvbs_monspecs.mode_set = TVOUT_CVBS_NTSC;
	}
	
	cvbs_monspecs.ddev = rk_display_device_register(&display_cvbs, parent, &ch7025);
	ch7025.cvbs = &cvbs_monspecs;
    return 0;
}