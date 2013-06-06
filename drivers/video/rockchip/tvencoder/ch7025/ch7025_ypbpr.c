#include <linux/ctype.h>
#include <linux/string.h>
#include <linux/rk_fb.h>
#include "ch7025.h"

static struct ch7025_monspecs	ypbpr_monspecs;

static const struct fb_videomode ypbpr_mode [] = {
	//h_bp	left_margin
	//h_fp	right_margin
	//v_bp	upper_margin
	//v_fp	lower_margin
	//name				refresh		xres	yres	pixclock	h_bp	h_fp	v_bp	v_fp	h_pw	v_pw	polariry	PorI	flag(used for vic)
{	"720x480p@60Hz",	60,			720,	480,	27000000,	60,		16,		30,		9,		62,		6,		0,			0,		OUT_P888	},
{	"720x576p@50Hz",	50,			720,	576,	27000000,	68,		12,		39,		5,		64,		5,		0,			0,		OUT_P888	},
{	"1280x720p@50Hz",	50,			1280,	720,	74250000,	440,	220,	20,		5,		40,		5,		FB_SYNC_HOR_HIGH_ACT | FB_SYNC_VERT_HIGH_ACT,			0,		OUT_P888	},
{	"1280x720p@60Hz",	60,			1280,	720,	74250000,	220,	110,	20,		5,		40,		5,		FB_SYNC_HOR_HIGH_ACT | FB_SYNC_VERT_HIGH_ACT,			0,		OUT_P888	},
};

static const struct fb_videomode ch7205_ypbpr_mode [] = {
	//h_bp	left_margin
	//h_fp	right_margin
	//v_bp	upper_margin
	//v_fp	lower_margin
	//name				refresh		xres	yres	pixclock	h_bp	h_fp	v_bp	v_fp	h_pw	v_pw	polariry	PorI	flag(used for vic)
{	"720x480p@60Hz",	60,			720,	480,	27000000,	60,		16,		30,		9,		62,		6,		0,			0,		OUT_P888	},
{	"720x576p@50Hz",	50,			720,	576,	27000000,	68,		12,		39,		5,		64,		5,		0,			0,		OUT_P888	},
//{	"1280x720p@50Hz",	50,			960,	720,	74250000,	220+320,	440,	20,		5,		40,		5,		FB_SYNC_HOR_HIGH_ACT | FB_SYNC_VERT_HIGH_ACT,			0,		OUT_P888	},
//{	"1280x720p@60Hz",	60,			960,	720,	74250000,	220+320,	110,	20,		5,		40,		5,		FB_SYNC_HOR_HIGH_ACT | FB_SYNC_VERT_HIGH_ACT,			0,		OUT_P888	},
{	"720x576p@50Hz",	50,			720,	576,	27000000,	68,		12,		39,		5,		64,		5,		0,			0,		OUT_P888	},
{	"720x480p@60Hz",	60,			720,	480,	27000000,	60,		16,		30,		9,		62,		6,		0,			0,		OUT_P888	},
};

#define I2CWrite	ch7025_write_reg
#define I2Cwrite	ch7025_write_reg

static void ch7025_ypbpr_init(const struct fb_videomode *outmode)
{	
	const struct fb_videomode *mode = &(ch7205_ypbpr_mode[ypbpr_monspecs.mode_set - 3]);
	int temp;
	DBG("%s start", __FUNCTION__);
	if(mode == NULL)
		printk(KERN_ERR "%s input mode error\n", __FUNCTION__);
	
	I2CWrite( 0x02, 0x01 );
	I2CWrite( 0x02, 0x03 );
	I2CWrite( 0x03, 0x00 );
	I2CWrite( 0x04, 0x39 );
	
	temp = v_DEPO_O(1) | v_DEPO_I(1);
	if(mode->sync & FB_SYNC_HOR_HIGH_ACT)
		temp |= v_HPO_I(1);
	if(mode->sync & FB_SYNC_VERT_HIGH_ACT)
		temp |= v_VPO_I(1);
	if(outmode->sync & FB_SYNC_HOR_HIGH_ACT)
		temp |= v_HPO_O(1);
	if(outmode->sync & FB_SYNC_VERT_HIGH_ACT)
		temp |= v_VPO_O(1);
	ch7025_write_reg( SYNC_CONFIG, temp & 0xFF );
	I2CWrite( 0x09, 0x80 );
	ch7025_write_reg( OUT_FORMAT, v_YPBPR_ENABLE(1) );
	
	// input video timing
	// hactive and htotal
	temp = mode->xres + mode->left_margin + mode->hsync_len + mode->right_margin;
	ch7025_write_reg(HTI_HAI_HIGH, v_HVAUTO(0) | v_HREPT(0) | v_HTI_HIGH((temp >> 8) & 0xFF) | v_HAI_HIGH((mode->xres >> 8) & 0xFF) );
	ch7025_write_reg(HAI_LOW, mode->xres & 0xFF);
	ch7025_write_reg(HTI_LOW, temp & 0xFF);
	
	// hsync and hoffset
	ch7025_write_reg(HW_HO_HIGH, v_HDTVEN(1) | v_EDGE_ENH(1) | v_HW_HIGH((mode->hsync_len >> 8) & 0xFF) |  v_HO_HIGH((mode->right_margin >> 8) & 0xFF));
	ch7025_write_reg(HO_LOW, mode->right_margin & 0xFF);
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
	
#ifdef USE_CRYSTAL	
	I2CWrite( SUBCARRIER, v_CRYSTAL_EAABLE(1) | v_CRYSTAL(CRYSTAL_12000000) );
#endif
	ch7025_write_reg( 0x4D, 0x04 );
	switch(ch7025.mode) {
		case TVOUT_YPbPr_720x480p_60:
#ifdef USE_CRYSTAL				
			I2CWrite( 0x4E, 0x80 );
			I2CWrite( 0x51, 0x4B );
			I2CWrite( 0x52, 0x12 );
			I2CWrite( 0x53, 0x1B );
			I2CWrite( 0x55, 0xE5 );
#endif
			I2CWrite( 0x51, 0x54 );
			I2CWrite( 0x52, 0x1B );
			I2CWrite( 0x53, 0x1A );
			temp = HDTV_480P;
			break;
		case TVOUT_YPbPr_720x576p_50:
#ifdef USE_CRYSTAL		
			I2CWrite( 0x4E, 0x80 );
			I2CWrite( 0x51, 0x4B );
			I2CWrite( 0x52, 0x12 );
			I2CWrite( 0x53, 0x1B );
			I2CWrite( 0x55, 0xE5 );
#endif
			I2CWrite( 0x51, 0x54 );
			I2CWrite( 0x52, 0x1B );
			I2CWrite( 0x53, 0x1A );
			temp = HDTV_576P;
			break;
		case TVOUT_YPbPr_1280x720p_50:
//			I2CWrite( 0x51, 0x3A );
//			I2CWrite( 0x52, 0x2D );
//			I2CWrite( 0x53, 0x08 );
			I2CWrite( 0x4D, 0x02 );
			I2CWrite( 0x4E, 0xC0 );
			I2CWrite( 0x51, 0x54 );
			I2CWrite( 0x52, 0x1B );
			I2CWrite( 0x53, 0x0A );
			temp = HDTV_720P_50HZ;
			break;
		case TVOUT_YPbPr_1280x720p_60:
//			I2CWrite( 0x51, 0x3A );
//			I2CWrite( 0x52, 0x2D );
//			I2CWrite( 0x53, 0x08 );
			I2CWrite( 0x4D, 0x02 );
			I2CWrite( 0x4E, 0xC0 );
			I2CWrite( 0x51, 0x54 );
			I2CWrite( 0x52, 0x1B );
			I2CWrite( 0x53, 0x0A );
			temp = HDTV_720P_60HZ;
			break;
		default:
			printk(KERN_ERR "CH7025/7026 Unkown mode");
			return;
	}
	ch7025_write_reg(HDTVMODE, v_HDTVMODE(temp));
//	I2CWrite( 0x5E, 0x80 );
	ch7025_write_reg( 0x7D, 0x62 );
	I2CWrite( 0x04, 0x38 );
	I2Cwrite( 0x06, 0x71 );
	
//	I2CWrite( 0x03, 0x01 );
//	I2CWrite( 0x04, (1 << 5) | 4 );
	/*
	NOTE: The following five repeated sentences are used here to wait memory initial complete, please don't remove...(you could refer 
	
	to Appendix A of programming guide document (CH7025(26)B Programming Guide Rev2.03.pdf or later version) for detailed information 
	
	about memory initialization! 
	*/
	I2Cwrite( 0x03, 0x00 );
	I2Cwrite( 0x03, 0x00 );
	I2Cwrite( 0x03, 0x00 );
	I2Cwrite( 0x03, 0x00 );
	I2Cwrite( 0x03, 0x00 );
	
	I2CWrite( 0x06, 0x70 );
	I2Cwrite( 0x02, 0x02 );
	I2Cwrite( 0x02, 0x03 );
	I2Cwrite( 0x04, 0x00 );

}

static int ypbpr_set_enable(struct rk_display_device *device, int enable)
{
	DBG("%s  %d", __FUNCTION__, enable);
	if(ypbpr_monspecs.enable != enable || (ypbpr_monspecs.mode_set != ch7025.mode))
	{
		if(enable == 0 && ypbpr_monspecs.enable)
		{
			ypbpr_monspecs.enable = 0;
			ch7025_standby();
		}
		else if(enable == 1)
		{
//			ch7025_switch_fb(ypbpr_monspecs.mode, ypbpr_monspecs.mode_set);
			ch7025_switch_fb(&ch7205_ypbpr_mode[ypbpr_monspecs.mode_set - 3], ypbpr_monspecs.mode_set);
			ch7025_ypbpr_init(ypbpr_monspecs.mode);
			ypbpr_monspecs.enable = 1;
		}
	}
	return 0;
}

static int ypbpr_get_enable(struct rk_display_device *device)
{
	DBG("%s start", __FUNCTION__);
	return ypbpr_monspecs.enable;
}

static int ypbpr_get_status(struct rk_display_device *device)
{
	DBG("%s start", __FUNCTION__);
	if(ch7025.mode > TVOUT_CVBS_PAL)
		return 1;
	else
		return 0;
}

static int ypbpr_get_modelist(struct rk_display_device *device, struct list_head **modelist)
{
	DBG("%s start", __FUNCTION__);
	*modelist = &(ypbpr_monspecs.modelist);
	return 0;
}

static int ypbpr_set_mode(struct rk_display_device *device, struct fb_videomode *mode)
{
	int i;

	DBG("%s start", __FUNCTION__);
	for(i = 0; i < ARRAY_SIZE(ypbpr_mode); i++)
	{
		if(fb_mode_is_equal(&ypbpr_mode[i], mode))
		{	
			if( ((i + 3) != ch7025.mode) )
			{
				ypbpr_monspecs.mode_set = i + 3;
				ypbpr_monspecs.mode = (struct fb_videomode *)&ypbpr_mode[i];
			}
			return 0;
		}
	}
	
	return -1;
}

static int ypbpr_get_mode(struct rk_display_device *device, struct fb_videomode *mode)
{
	DBG("%s start", __FUNCTION__);
	*mode = *(ypbpr_monspecs.mode);
	return 0;
}

static struct rk_display_ops ypbpr_display_ops = {
	.setenable = ypbpr_set_enable,
	.getenable = ypbpr_get_enable,
	.getstatus = ypbpr_get_status,
	.getmodelist = ypbpr_get_modelist,
	.setmode = ypbpr_set_mode,
	.getmode = ypbpr_get_mode,
};

static int display_ypbpr_probe(struct rk_display_device *device, void *devdata)
{
	device->owner = THIS_MODULE;
	strcpy(device->type, "YPbPr");
	device->name = "ypbpr";
	device->priority = DISPLAY_PRIORITY_YPbPr;
	device->property = ch7025.property;
	device->priv_data = devdata;
	device->ops = &ypbpr_display_ops;
	return 1;
}

static struct rk_display_driver display_cvbs = {
	.probe = display_ypbpr_probe,
};

int ch7025_register_display_ypbpr(struct device *parent)
{
	int i;
	
	memset(&ypbpr_monspecs, 0, sizeof(struct ch7025_monspecs));
	INIT_LIST_HEAD(&ypbpr_monspecs.modelist);
	for(i = 0; i < ARRAY_SIZE(ypbpr_mode); i++)
		display_add_videomode(&ypbpr_mode[i], &ypbpr_monspecs.modelist);
		
	if(ch7025.mode > TVOUT_CVBS_PAL) {
		ypbpr_monspecs.mode = (struct fb_videomode *)&(ypbpr_mode[ch7025.mode - 3]);
		ypbpr_monspecs.mode_set = ch7025.mode;
	}
	else {
		ypbpr_monspecs.mode = (struct fb_videomode *)&(ypbpr_mode[3]);
		ypbpr_monspecs.mode_set = TVOUT_YPbPr_1280x720p_60;
	}
	
	ypbpr_monspecs.ddev = rk_display_device_register(&display_cvbs, parent, &ch7025);
	ch7025.ypbpr = &ypbpr_monspecs;
    
    return 0;
}