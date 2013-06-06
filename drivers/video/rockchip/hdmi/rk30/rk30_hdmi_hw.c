#include <linux/delay.h>
#include <asm/io.h>
#include <mach/io.h>
#include "rk30_hdmi.h"
#include "rk30_hdmi_hw.h"

static char edid_result = 0;

static inline void delay100us(void)
{
	udelay(100);
}

static void rk30_hdmi_set_pwr_mode(struct rk30_hdmi *rk30_hdmi, int mode)
{
	struct hdmi *hdmi = rk30_hdmi->hdmi;
	if(rk30_hdmi->pwr_mode == mode)
		return;
	RK30DBG( "[%s] mode %d\n", __FUNCTION__, mode);	
	switch(mode)
	{
		case PWR_SAVE_MODE_A:
			HDMIWrReg(SYS_CTRL, 0x10);
			break;
		case PWR_SAVE_MODE_B:
			HDMIWrReg(SYS_CTRL, 0x20);
			break;
		case PWR_SAVE_MODE_D:
			// reset PLL A&B
			HDMIWrReg(SYS_CTRL, 0x4C);
			delay100us();
			// release PLL A reset
			HDMIWrReg(SYS_CTRL, 0x48);
			delay100us();
			// release PLL B reset
			HDMIWrReg(SYS_CTRL, 0x40);
			break;
		case PWR_SAVE_MODE_E:
			HDMIWrReg(SYS_CTRL, 0x80);
			break;
	}
	rk30_hdmi->pwr_mode = mode;
	if(mode != PWR_SAVE_MODE_A)
		msleep(10);
	RK30DBG( "[%s] curmode %02x\n", __FUNCTION__, HDMIRdReg(SYS_CTRL));
}

int rk30_hdmi_detect_hotplug(struct hdmi *hdmi)
{
	struct rk30_hdmi *rk30_hdmi = hdmi->property->priv;
	int value =	HDMIRdReg(HPD_MENS_STA);
	
	RK30DBG( "[%s] value %02x\n", __FUNCTION__, value);
	#if 0
	// When HPD and TMDS_CLK was high, HDMI is actived.
	value &= m_HOTPLUG_STATUS | m_MSEN_STATUS;
	if(value  == (m_HOTPLUG_STATUS | m_MSEN_STATUS) )
		return HDMI_HPD_ACTIVED;
	else if(value)
		return HDMI_HPD_INSERT;
	else
		return HDMI_HPD_REMOVED;
	#else
	// When HPD was high, HDMI is actived.
	if(value & m_HOTPLUG_STATUS)
		return HDMI_HPD_ACTIVED;
	else if(value & m_MSEN_STATUS)
		return HDMI_HPD_INSERT;
	else
		return HDMI_HPD_REMOVED;
	#endif
}

#define HDMI_EDID_DDC_CLK	90000
int rk30_hdmi_read_edid(struct hdmi *hdmi, int block, unsigned char *buff)
{
	struct rk30_hdmi *rk30_hdmi = hdmi->property->priv;
	int value, ret = -1, ddc_bus_freq = 0;
	char interrupt = 0, trytime = 2;
	unsigned long flags;
	
	RK30DBG( "[%s] block %d\n", __FUNCTION__, block);
	spin_lock_irqsave(&rk30_hdmi->irq_lock, flags);
	edid_result = 0;
	spin_unlock_irqrestore(&rk30_hdmi->irq_lock, flags);
	//Before Phy parameter was set, DDC_CLK is equal to PLLA freq which is 30MHz.
	//Set DDC I2C CLK which devided from DDC_CLK to 100KHz.
	ddc_bus_freq = (30000000/HDMI_EDID_DDC_CLK)/4;
	HDMIWrReg(DDC_BUS_FREQ_L, ddc_bus_freq & 0xFF);
	HDMIWrReg(DDC_BUS_FREQ_H, (ddc_bus_freq >> 8) & 0xFF);
	
	// Enable edid interrupt
	HDMIWrReg(INTR_MASK1, m_INT_HOTPLUG | m_INT_MSENS | m_INT_EDID_ERR | m_INT_EDID_READY);
	
	while(trytime--) {
		// Config EDID block and segment addr
		HDMIWrReg(EDID_WORD_ADDR, (block%2) * 0x80);
		HDMIWrReg(EDID_SEGMENT_POINTER, block/2);	
	
		value = 100;
		while(value--)
		{
			spin_lock_irqsave(&rk30_hdmi->irq_lock, flags);
			interrupt = edid_result;
			edid_result = 0;
			spin_unlock_irqrestore(&rk30_hdmi->irq_lock, flags);
			if(interrupt & (m_INT_EDID_ERR | m_INT_EDID_READY))
				break;
			msleep(10);
		}
		RK30DBG( "[%s] edid read value %d\n", __FUNCTION__, value);
		if(interrupt & m_INT_EDID_READY)
		{
			for(value = 0; value < HDMI_EDID_BLOCK_SIZE; value++) 
				buff[value] = HDMIRdReg(DDC_READ_FIFO_ADDR);
			ret = 0;
			
			RK30DBG( "[%s] edid read sucess\n", __FUNCTION__);
#ifdef HDMI_DEBUG
			for(value = 0; value < 128; value++) {
				printk("0x%02x ,", buff[value]);
				if( (value + 1) % 16 == 0)
					printk("\n");
			}
#endif
			break;
		}		
		if(interrupt & m_INT_EDID_ERR)
			dev_err(hdmi->dev, "[%s] edid read error\n", __FUNCTION__);
		
		RK30DBG( "[%s] edid try times %d\n", __FUNCTION__, trytime);
		msleep(100);
	}
	// Disable edid interrupt
	HDMIWrReg(INTR_MASK1, m_INT_HOTPLUG | m_INT_MSENS);
	return ret;
}

static inline void rk30_hdmi_config_phy_reg(struct rk30_hdmi *rk30_hdmi, int reg, int value)
{
	HDMIWrReg(reg, value);
	HDMIWrReg(SYS_CTRL, 0x2C);
	delay100us();
	HDMIWrReg(SYS_CTRL, 0x20);
	msleep(1);
}

static void rk30_hdmi_config_phy(struct rk30_hdmi *rk30_hdmi, unsigned int tmdsclk)
{
	HDMIWrReg(DEEP_COLOR_MODE, 0x22);	// tmds frequency same as input dlck
	rk30_hdmi_set_pwr_mode(rk30_hdmi, PWR_SAVE_MODE_B);
	switch(tmdsclk)
	{
		case 148500000:
			rk30_hdmi_config_phy_reg(rk30_hdmi, 0x158, 0x0E);
			rk30_hdmi_config_phy_reg(rk30_hdmi, 0x15c, 0x00);
			rk30_hdmi_config_phy_reg(rk30_hdmi, 0x160, 0x60);
			rk30_hdmi_config_phy_reg(rk30_hdmi, 0x164, 0x00);
			rk30_hdmi_config_phy_reg(rk30_hdmi, 0x168, 0xDA);
			rk30_hdmi_config_phy_reg(rk30_hdmi, 0x16c, 0xA1);
			rk30_hdmi_config_phy_reg(rk30_hdmi, 0x170, 0x0e);
			rk30_hdmi_config_phy_reg(rk30_hdmi, 0x174, 0x22);
			rk30_hdmi_config_phy_reg(rk30_hdmi, 0x178, 0x00);
			break;
			
		case 74250000:
			rk30_hdmi_config_phy_reg(rk30_hdmi, 0x158, 0x06);
			rk30_hdmi_config_phy_reg(rk30_hdmi, 0x15c, 0x00);
			rk30_hdmi_config_phy_reg(rk30_hdmi, 0x160, 0x60);
			rk30_hdmi_config_phy_reg(rk30_hdmi, 0x164, 0x00);
			rk30_hdmi_config_phy_reg(rk30_hdmi, 0x168, 0xCA);
			rk30_hdmi_config_phy_reg(rk30_hdmi, 0x16c, 0xA3);
			rk30_hdmi_config_phy_reg(rk30_hdmi, 0x170, 0x0e);
			rk30_hdmi_config_phy_reg(rk30_hdmi, 0x174, 0x20);
			rk30_hdmi_config_phy_reg(rk30_hdmi, 0x178, 0x00);
			break;
			
		case 27000000:
			rk30_hdmi_config_phy_reg(rk30_hdmi, 0x158, 0x02);
			rk30_hdmi_config_phy_reg(rk30_hdmi, 0x15c, 0x00);
			rk30_hdmi_config_phy_reg(rk30_hdmi, 0x160, 0x60);
			rk30_hdmi_config_phy_reg(rk30_hdmi, 0x164, 0x00);
			rk30_hdmi_config_phy_reg(rk30_hdmi, 0x168, 0xC2);
			rk30_hdmi_config_phy_reg(rk30_hdmi, 0x16c, 0xA2);
			rk30_hdmi_config_phy_reg(rk30_hdmi, 0x170, 0x0e);
			rk30_hdmi_config_phy_reg(rk30_hdmi, 0x174, 0x20);
			rk30_hdmi_config_phy_reg(rk30_hdmi, 0x178, 0x00);
			break;
		default:
			printk(KERN_ERR "%s not support such tmds clk %d\n", __FUNCTION__, tmdsclk);
			break;
	}
}

int rk30_hdmi_config_vsi(struct hdmi *hdmi, unsigned char vic_3d, unsigned char format)
{
	struct rk30_hdmi *rk30_hdmi = hdmi->property->priv;
	char info[SIZE_VSI_INFOFRAME];
	int i, temp;
	
	RK30DBG( "[%s] vic_3d %d format %d.\n", __FUNCTION__, vic_3d, format);
	memset(info, 0, SIZE_VSI_INFOFRAME);
	HDMIMskReg(temp, CONTROL_PACKET_AUTO_SEND, m_CONTROL_PACKET_AUTO_VS, 0);
	HDMIWrReg(CONTROL_PACKET_BUF_INDEX, INFOFRAME_VSI);
	// Header Bytes
	info[0] = 0x81;
	info[1] = 0x01;
	// PB1 - PB3 contain the 24bit IEEE Registration Identifier
	info[4] = 0x03;
	info[5] = 0x0c;
	info[6] = 0x00;
	// PB4 - HDMI_Video_Format into bits 7:5
	info[7] = format << 5;
	// PB5 - Depending on the video format, this byte will contain either the HDMI_VIC
	// code in buts 7:0, OR the 3D_Structure in bits 7:4.
	switch(format)
	{
		case HDMI_VIDEO_FORMAT_4Kx2K:
			// This is a 2x4K mode, set the HDMI_VIC in buts 7:0.  Values
			// are from HDMI 1.4 Spec, 8.2.3.1 (Table 8-13).
			info[2] = 0x06 - 1;
			info[8] = vic_3d;
			info[9] = 0;
			break;
		case HDMI_VIDEO_FORMAT_3D:
			// This is a 3D mode, set the 3D_Structure in buts 7:4
			// Bits 3:0 are reseved so set to 0.  Values are from HDMI 1.4
			// Spec, Appendix H (Table H-2).
			info[8] = vic_3d << 4;
			// Add the Extended data field when the 3D format is Side-by-Side(Half).
			// See Spec Table H-3 for details.			
			if ((info[8] >> 4) == HDMI_3D_SIDE_BY_SIDE_HALF)
			{
				info[2] = 0x06;
				info[9] = 0x00;
			}
			else 
			{
			   	info[2] = 0x06 - 1;
			}
			break;
		default:
			info[2] = 0x06 - 2;
			info[8] = 0;
			info[9] = 0;
			break;
	}
	info[3] = info[0] + info[1] + info[2];
	// Calculate InfoFrame ChecKsum
	for (i = 4; i < SIZE_VSI_INFOFRAME; i++)
	{
    	info[3] += info[i];
	}
	info[3] = 0x100 - info[3];
	
	for(i = 0; i < SIZE_VSI_INFOFRAME; i++) {
		HDMIWrReg(CONTROL_PACKET_HB0 + i*4, info[i]);
		RK30DBG( "[%s] info[%d] is 0x%02x.\n", __FUNCTION__, i, info[i]);
	}
	HDMIMskReg(temp, CONTROL_PACKET_AUTO_SEND, m_CONTROL_PACKET_AUTO_VS, m_CONTROL_PACKET_AUTO_VS);
	return 0;
}

static void rk30_hdmi_config_avi(struct rk30_hdmi *rk30_hdmi, unsigned char vic, unsigned char color_output)
{
	int i, clolorimetry, aspect_ratio;
	char info[SIZE_AVI_INFOFRAME];
	
	memset(info, 0, SIZE_AVI_INFOFRAME);
	HDMIWrReg(CONTROL_PACKET_BUF_INDEX, INFOFRAME_AVI);
	info[0] = 0x82;
	info[1] = 0x02;
	info[2] = 0x0D;	
	info[3] = info[0] + info[1] + info[2];

	if(color_output == HDMI_COLOR_YCbCr444)	
		info[4] = (AVI_COLOR_MODE_YCBCR444 << 5);
	else if(color_output == HDMI_COLOR_YCbCr422)
		info[4] = (AVI_COLOR_MODE_YCBCR422 << 5);
	else
		info[4] = (AVI_COLOR_MODE_RGB << 5);
	info[4] |= (1 << 4);	//Enable active format data bits is present in info[2]
	
	switch(vic)
	{
		case HDMI_720x480i_60HZ_4_3:
		case HDMI_720x576i_50HZ_4_3:
		case HDMI_720x480p_60HZ_4_3:
		case HDMI_720x576p_50HZ_4_3:				
			aspect_ratio = AVI_CODED_FRAME_ASPECT_4_3;
			clolorimetry = AVI_COLORIMETRY_SMPTE_170M;
			break;
		case HDMI_720x480i_60HZ_16_9:
		case HDMI_720x576i_50HZ_16_9:
		case HDMI_720x480p_60HZ_16_9:
		case HDMI_720x576p_50HZ_16_9:
			aspect_ratio = AVI_CODED_FRAME_ASPECT_16_9;
			clolorimetry = AVI_COLORIMETRY_SMPTE_170M;
			break;
		default:
			aspect_ratio = AVI_CODED_FRAME_ASPECT_16_9;
			clolorimetry = AVI_COLORIMETRY_ITU709;
	}

	if(color_output == HDMI_COLOR_RGB)
		clolorimetry = AVI_COLORIMETRY_NO_DATA;
	
	info[5] = (clolorimetry << 6) | (aspect_ratio << 4) | ACTIVE_ASPECT_RATE_SAME_AS_CODED_FRAME;
	info[6] = 0;
	info[7] = vic;
	info[8] = 0;

	// Calculate AVI InfoFrame ChecKsum
	for (i = 4; i < SIZE_AVI_INFOFRAME; i++)
	{
    	info[3] += info[i];
	}
	info[3] = 0x100 - info[3];
	
	for(i = 0; i < SIZE_AVI_INFOFRAME; i++)
		HDMIWrReg(CONTROL_PACKET_HB0 + i*4, info[i]);
}
 
static char coeff_csc[][24] = {
		//G			B			R			Bias
	{	//CSC_RGB_0_255_TO_ITU601_16_235
		0x11, 0xb6, 0x02, 0x0b, 0x10, 0x55, 0x00, 0x80, 	//Cr
		0x02, 0x59, 0x01, 0x32, 0x00, 0x75, 0x00, 0x10, 	//Y
		0x11, 0x5b, 0x10, 0xb0, 0x02, 0x0b, 0x00, 0x80, 	//Cb
	},
	{	//CSC_RGB_0_255_TO_ITU709_16_235
		0x11, 0xdb, 0x02, 0x0b, 0x10, 0x30, 0x00, 0x80,		//Cr
		0x02, 0xdc, 0x00, 0xda, 0x00, 0x4a, 0x00, 0x10, 	//Y
		0x11, 0x93, 0x10, 0x78, 0x02, 0x0b, 0x00, 0x80, 	//Cb
	},
		//Y			Cr			Cb			Bias
	{	//CSC_ITU601_16_235_TO_RGB_16_235
		0x04, 0x00, 0x05, 0x7c, 0x00, 0x00, 0x02, 0xaf, 	//R
		0x04, 0x00, 0x12, 0xcb, 0x11, 0x58, 0x00, 0x84, 	//G
		0x04, 0x00, 0x00, 0x00, 0x06, 0xee, 0x02, 0xde,		//B
	},
	{	//CSC_ITU709_16_235_TO_RGB_16_235
		0x04, 0x00, 0x06, 0x29, 0x00, 0x00, 0x02, 0xc5, 	//R
		0x04, 0x00, 0x11, 0xd6, 0x10, 0xbb, 0x00, 0x52, 	//G
		0x04, 0x00, 0x00, 0x00, 0x07, 0x44, 0x02, 0xe8, 	//B
	},
	{	//CSC_ITU601_16_235_TO_RGB_0_255
		0x04, 0xa8, 0x05, 0x7c, 0x00, 0x00, 0x02, 0xc2, 	//R
		0x04, 0xa8, 0x12, 0xcb, 0x11, 0x58, 0x00, 0x72, 	//G
		0x04, 0xa8, 0x00, 0x00, 0x06, 0xee, 0x02, 0xf0, 	//B
	},
	{	//CSC_ITU709_16_235_TO_RGB_0_255
		0x04, 0xa8, 0x06, 0x29, 0x00, 0x00, 0x02, 0xd8, 	//R
		0x04, 0xa8, 0x11, 0xd6, 0x10, 0xbb, 0x00, 0x40, 	//G
		0x04, 0xa8, 0x00, 0x00, 0x07, 0x44, 0x02, 0xfb, 	//B
	},
	
};

static void rk30_hdmi_config_csc(struct rk30_hdmi *rk30_hdmi, struct hdmi_video *vpara)
{
	int i, mode;
	char *coeff = NULL;
		
	if( ((vpara->color_input == HDMI_COLOR_RGB) && (vpara->color_output == HDMI_COLOR_RGB)) ||
		((vpara->color_input != HDMI_COLOR_RGB) && (vpara->color_output != HDMI_COLOR_RGB) ))
	{
		return;
	}
	switch(vpara->vic)
	{
		case HDMI_720x480i_60HZ_4_3:
		case HDMI_720x576i_50HZ_4_3:
		case HDMI_720x480p_60HZ_4_3:
		case HDMI_720x576p_50HZ_4_3:
		case HDMI_720x480i_60HZ_16_9:
		case HDMI_720x576i_50HZ_16_9:
		case HDMI_720x480p_60HZ_16_9:
		case HDMI_720x576p_50HZ_16_9:
			if(vpara->color_input == HDMI_COLOR_RGB)
				mode = CSC_RGB_0_255_TO_ITU601_16_235;
			else if(vpara->sink_hdmi == OUTPUT_HDMI)
				mode = CSC_ITU601_16_235_TO_RGB_16_235;
			else
				mode = CSC_ITU601_16_235_TO_RGB_0_255;
			break;
		default:
			if(vpara->color_input == HDMI_COLOR_RGB)
				mode = CSC_RGB_0_255_TO_ITU709_16_235;
			else if(vpara->sink_hdmi == OUTPUT_HDMI)
				mode = CSC_ITU709_16_235_TO_RGB_16_235;
			else
				mode = CSC_ITU709_16_235_TO_RGB_0_255;
			break;
	}
	
	coeff = coeff_csc[mode];
	
	HDMIWrReg(CSC_CONFIG1, v_CSC_MODE(CSC_MODE_MANUAL) | v_CSC_BRSWAP_DIABLE(1));
	
	for(i = 0; i < 24; i++)
		HDMIWrReg(CSC_PARA_C0_H + i*4, coeff[i]);
		
	HDMIWrReg(AV_CTRL2, v_CSC_ENABLE(1));
}

int rk30_hdmi_config_video(struct hdmi *hdmi, struct hdmi_video *vpara)
{
	struct rk30_hdmi *rk30_hdmi = hdmi->property->priv;
	int value, vsync_offset;
	struct fb_videomode *mode;
	
	RK30DBG( "[%s]\n", __FUNCTION__);
	if(vpara == NULL) {
		dev_err(hdmi->dev, "[%s] input parameter error\n", __FUNCTION__);
		return -1;
	}
	if(rk30_hdmi->pwr_mode == PWR_SAVE_MODE_E)
		rk30_hdmi_set_pwr_mode(rk30_hdmi, PWR_SAVE_MODE_D);
	if(rk30_hdmi->pwr_mode == PWR_SAVE_MODE_D || rk30_hdmi->pwr_mode == PWR_SAVE_MODE_A)
		rk30_hdmi_set_pwr_mode(rk30_hdmi, PWR_SAVE_MODE_B);
	
	if(hdmi->ops->hdcp_power_off_cb)
		hdmi->ops->hdcp_power_off_cb();
	
	HDMIWrReg(CONTROL_PACKET_AUTO_SEND, 0x00);
	
	// Input video mode is RGB24bit, Data enable signal from external
	HDMIMskReg(value, AV_CTRL1,  m_DE_SIGNAL_SELECT, EXTERNAL_DE)	
	HDMIMskReg(value, VIDEO_CTRL1, m_VIDEO_OUTPUT_MODE | m_VIDEO_INPUT_DEPTH | m_VIDEO_INPUT_COLOR_MODE, \
		v_VIDEO_OUTPUT_MODE(vpara->color_output) | v_VIDEO_INPUT_DEPTH(VIDEO_INPUT_DEPTH_8BIT) | vpara->color_input)
	HDMIWrReg(DEEP_COLOR_MODE, 0x20);
	// color space convert
	rk30_hdmi_config_csc(rk30_hdmi, vpara);
	// Set HDMI Mode
	HDMIWrReg(HDCP_CTRL, v_HDMI_DVI(vpara->sink_hdmi));

	// Set ext video
	mode = (struct fb_videomode *)hdmi_vic_to_videomode(vpara->vic);
	if(mode == NULL)
	{
		dev_err(hdmi->dev, "[%s] not found vic %d\n", __FUNCTION__, vpara->vic);
		return -ENOENT;
	}
	rk30_hdmi->tmdsclk = mode->pixclock;

	if( (vpara->vic == HDMI_720x480p_60HZ_4_3) || (vpara->vic == HDMI_720x480p_60HZ_16_9) )
		vsync_offset = 6;
	else
		vsync_offset = 0;
	value = v_VSYNC_OFFSET(vsync_offset) | v_EXT_VIDEO_ENABLE(1) | v_INTERLACE(mode->vmode);
	if(mode->sync & FB_SYNC_HOR_HIGH_ACT)
		value |= v_HSYNC_POLARITY(1);
	if(mode->sync & FB_SYNC_VERT_HIGH_ACT)
		value |= v_VSYNC_POLARITY(1);
	HDMIWrReg(EXT_VIDEO_PARA, value);
	value = mode->left_margin + mode->xres + mode->right_margin + mode->hsync_len;
	HDMIWrReg(EXT_VIDEO_PARA_HTOTAL_L, value & 0xFF);
	HDMIWrReg(EXT_VIDEO_PARA_HTOTAL_H, (value >> 8) & 0xFF);
	
	value = mode->left_margin + mode->right_margin + mode->hsync_len;
	HDMIWrReg(EXT_VIDEO_PARA_HBLANK_L, value & 0xFF);
	HDMIWrReg(EXT_VIDEO_PARA_HBLANK_H, (value >> 8) & 0xFF);
	
	value = mode->left_margin + mode->hsync_len;
	HDMIWrReg(EXT_VIDEO_PARA_HDELAY_L, value & 0xFF);
	HDMIWrReg(EXT_VIDEO_PARA_HDELAY_H, (value >> 8) & 0xFF);
	
	value = mode->hsync_len;
	HDMIWrReg(EXT_VIDEO_PARA_HSYNCWIDTH_L, value & 0xFF);
	HDMIWrReg(EXT_VIDEO_PARA_HSYNCWIDTH_H, (value >> 8) & 0xFF);
	
	value = mode->upper_margin + mode->yres + mode->lower_margin + mode->vsync_len;
	HDMIWrReg(EXT_VIDEO_PARA_VTOTAL_L, value & 0xFF);
	HDMIWrReg(EXT_VIDEO_PARA_VTOTAL_H, (value >> 8) & 0xFF);
	
	value = mode->upper_margin + mode->vsync_len + mode->lower_margin;
	HDMIWrReg(EXT_VIDEO_PARA_VBLANK_L, value & 0xFF);
	
	value = mode->upper_margin + mode->vsync_len + vsync_offset;
	HDMIWrReg(EXT_VIDEO_PARA_VDELAY, value & 0xFF);
	
	value = mode->vsync_len;
	HDMIWrReg(EXT_VIDEO_PARA_VSYNCWIDTH, value & 0xFF);
	
	if(vpara->sink_hdmi == OUTPUT_HDMI) {
		rk30_hdmi_config_avi(rk30_hdmi, vpara->vic, vpara->color_output);
		RK30DBG( "[%s] sucess output HDMI.\n", __FUNCTION__);
		if(vpara->format_3d != HDMI_3D_NONE)
			rk30_hdmi_config_vsi(hdmi, vpara->format_3d, HDMI_VIDEO_FORMAT_3D);
		else if(vpara->vic & HDMI_VIDEO_EXT)
			rk30_hdmi_config_vsi(hdmi, vpara->vic & HDMI_VIC_MASK, HDMI_VIDEO_FORMAT_4Kx2K);
		else
			rk30_hdmi_config_vsi(hdmi, vpara->vic, HDMI_VIDEO_FORMAT_NORMAL);
	}
	else {
		RK30DBG( "[%s] sucess output DVI.\n", __FUNCTION__);	
	}
	
	rk30_hdmi_config_phy(rk30_hdmi, rk30_hdmi->tmdsclk);
	rk30_hdmi_control_output(hdmi, HDMI_VIDEO_MUTE | HDMI_AUDIO_MUTE);
	return 0;
}

static void rk30_hdmi_config_aai(struct rk30_hdmi *rk30_hdmi)
{
	int i;
	char info[SIZE_AUDIO_INFOFRAME];
	
	memset(info, 0, SIZE_AUDIO_INFOFRAME);
	
	info[0] = 0x84;
	info[1] = 0x01;
	info[2] = 0x0A;
	
	info[3] = info[0] + info[1] + info[2];	
	for (i = 4; i < SIZE_AUDIO_INFOFRAME; i++)
    	info[3] += info[i];
    	
	info[3] = 0x100 - info[3];
	
	HDMIWrReg(CONTROL_PACKET_BUF_INDEX, INFOFRAME_AAI);
	for(i = 0; i < SIZE_AUDIO_INFOFRAME; i++)
		HDMIWrReg(CONTROL_PACKET_HB0 + i*4, info[i]);
}

int rk30_hdmi_config_audio(struct hdmi *hdmi, struct hdmi_audio *audio)
{
	struct rk30_hdmi *rk30_hdmi = hdmi->property->priv;
	int value, rate, N;
	char word_length, channel;
	
	if(audio->channel < 3)
		channel = I2S_CHANNEL_1_2;
	else if(audio->channel < 5)
		channel = I2S_CHANNEL_3_4;
	else if(audio->channel < 7)
		channel = I2S_CHANNEL_5_6;
	else
		channel = I2S_CHANNEL_7_8;
	
	switch(audio->rate)
	{
		case HDMI_AUDIO_FS_32000:
			rate = AUDIO_32K;
			N = N_32K;
			break;
		case HDMI_AUDIO_FS_44100:
			rate = AUDIO_441K;
			N = N_441K;
			break;
		case HDMI_AUDIO_FS_48000:
			rate = AUDIO_48K;
			N = N_48K;
			break;
		case HDMI_AUDIO_FS_88200:
			rate = AUDIO_882K;
			N = N_882K;
			break;
		case HDMI_AUDIO_FS_96000:
			rate = AUDIO_96K;
			N = N_96K;
			break;
		case HDMI_AUDIO_FS_176400:
			rate = AUDIO_1764K;
			N = N_1764K;
			break;
		case HDMI_AUDIO_FS_192000:
			rate = AUDIO_192K;
			N = N_192K;
			break;
		default:
			dev_err(hdmi->dev, "[%s] not support such sample rate %d\n", __FUNCTION__, audio->rate);
			return -ENOENT;
	}
//	switch(audio->word_length)
//	{
//		case HDMI_AUDIO_WORD_LENGTH_16bit:
//			word_length = 0x02;
//			break;
//		case HDMI_AUDIO_WORD_LENGTH_20bit:
//			word_length = 0x0a;
//			break;
//		case HDMI_AUDIO_WORD_LENGTH_24bit:
//			word_length = 0x0b;
//			break;
//		default:
//			dev_err(hdmi->dev, "[%s] not support such word length %d\n", __FUNCTION__, audio->word_length);
//			return -ENOENT;
//	}
	//set_audio_if I2S
	HDMIWrReg(AUDIO_CTRL1, 0x00); //internal CTS, disable down sample, i2s input, disable MCLK
	HDMIWrReg(AUDIO_CTRL2, 0x40); 
	HDMIWrReg(I2S_AUDIO_CTRL, v_I2S_MODE(I2S_MODE_STANDARD) | v_I2S_CHANNEL(channel) );	
	HDMIWrReg(I2S_INPUT_SWAP, 0x00); //no swap
	HDMIMskReg(value, AV_CTRL1, m_AUDIO_SAMPLE_RATE, v_AUDIO_SAMPLE_RATE(rate))	
//	HDMIWrReg(SRC_NUM_AUDIO_LEN, word_length);
		
    //Set N value 6144, fs=48kHz
    HDMIWrReg(N_1, N & 0xFF);
    HDMIWrReg(N_2, (N >> 8) & 0xFF);
    HDMIWrReg(LR_SWAP_N3, (N >> 16) & 0x0F); 
    
    rk30_hdmi_config_aai(rk30_hdmi);
    return 0;
}

static void rk30_hdmi_audio_reset(struct rk30_hdmi *rk30_hdmi)
{
	int value;
	
	HDMIMskReg(value, VIDEO_SETTING2, m_AUDIO_RESET, AUDIO_CAPTURE_RESET)
	msleep(1);
	HDMIMskReg(value, VIDEO_SETTING2, m_AUDIO_RESET, 0)
}

int rk30_hdmi_control_output(struct hdmi *hdmi, int enable)
{
	struct rk30_hdmi *rk30_hdmi = hdmi->property->priv;
	
	RK30DBG( "[%s] %d\n", __FUNCTION__, enable);
	if(enable & (HDMI_VIDEO_MUTE | HDMI_AUDIO_MUTE) ) {
		HDMIWrReg(VIDEO_SETTING2, 0x03);
	}
	else {
		if(rk30_hdmi->pwr_mode == PWR_SAVE_MODE_B) {
			//  Switch to power save mode_d
			rk30_hdmi_set_pwr_mode(rk30_hdmi, PWR_SAVE_MODE_D);
		}
		if(rk30_hdmi->pwr_mode == PWR_SAVE_MODE_D) {
			//  Switch to power save mode_e
			rk30_hdmi_set_pwr_mode(rk30_hdmi, PWR_SAVE_MODE_E);
		}
		HDMIWrReg(VIDEO_SETTING2, enable);
		rk30_hdmi_audio_reset(rk30_hdmi);
	}
	
	return 0;
}

int rk30_hdmi_removed(struct hdmi *hdmi)
{
	struct rk30_hdmi *rk30_hdmi = hdmi->property->priv;
	
	if(rk30_hdmi->pwr_mode == PWR_SAVE_MODE_E)
	{
		HDMIWrReg(VIDEO_SETTING2, 0x00);
		rk30_hdmi_audio_reset(rk30_hdmi);
		rk30_hdmi_set_pwr_mode(rk30_hdmi, PWR_SAVE_MODE_D);
	}
	if(rk30_hdmi->pwr_mode == PWR_SAVE_MODE_D)
		rk30_hdmi_set_pwr_mode(rk30_hdmi, PWR_SAVE_MODE_B);
	if(rk30_hdmi->pwr_mode == PWR_SAVE_MODE_B)
	{
		HDMIWrReg(INTR_MASK1, m_INT_HOTPLUG | m_INT_MSENS);
		HDMIWrReg(INTR_MASK2, 0);
		HDMIWrReg(INTR_MASK3, 0);
		HDMIWrReg(INTR_MASK4, 0);
		// Disable color space convertion
		HDMIWrReg(AV_CTRL2, v_CSC_ENABLE(0));
		HDMIWrReg(CSC_CONFIG1, v_CSC_MODE(CSC_MODE_AUTO) | v_CSC_BRSWAP_DIABLE(1));
		if(hdmi->ops->hdcp_power_off_cb)
			hdmi->ops->hdcp_power_off_cb();
		rk30_hdmi_set_pwr_mode(rk30_hdmi, PWR_SAVE_MODE_A);
	}
	rk30_hdmi->tmdsclk = 0;
	dev_printk(KERN_INFO , hdmi->dev , "Removed.\n");
	return HDMI_ERROR_SUCESS;
}

int rk30_hdmi_enable(struct hdmi *hdmi)
{
	struct rk30_hdmi *rk30_hdmi = hdmi->property->priv;

	enable_irq(rk30_hdmi->irq);
	return 0;
}

int rk30_hdmi_disable(struct hdmi *hdmi)
{
	struct rk30_hdmi *rk30_hdmi = hdmi->property->priv;

	disable_irq(rk30_hdmi->irq);
	return 0;
}

irqreturn_t hdmi_irq(int irq, void *priv)
{		
	struct rk30_hdmi *rk30_hdmi = priv;
	struct hdmi *hdmi = rk30_hdmi->hdmi;
	
	char interrupt1 = 0, interrupt2 = 0, interrupt3 = 0, interrupt4 = 0;
	
	if(rk30_hdmi->pwr_mode == PWR_SAVE_MODE_A)
	{
		HDMIWrReg(SYS_CTRL, 0x20);
		rk30_hdmi->pwr_mode = PWR_SAVE_MODE_B;
		
		RK30DBG( "hdmi irq wake up\n");
		// HDMI was inserted when system is sleeping, irq was triggered only once
		// when wake up. So we need to check hotplug status.
		if(HDMIRdReg(HPD_MENS_STA) & (m_HOTPLUG_STATUS | m_MSEN_STATUS)) {	
			hdmi_submit_work(hdmi, HDMI_HPD_CHANGE, 10, NULL);
		}
	}
	else
	{
		interrupt1 = HDMIRdReg(INTR_STATUS1);
		interrupt2 = HDMIRdReg(INTR_STATUS2);
		interrupt3 = HDMIRdReg(INTR_STATUS3);
		interrupt4 = HDMIRdReg(INTR_STATUS4);
		HDMIWrReg(INTR_STATUS1, interrupt1);
		HDMIWrReg(INTR_STATUS2, interrupt2);
		HDMIWrReg(INTR_STATUS3, interrupt3);
		HDMIWrReg(INTR_STATUS4, interrupt4);
#if 0
		RK30DBG( "[%s] interrupt1 %02x interrupt2 %02x interrupt3 %02x interrupt4 %02x\n",\
			 __FUNCTION__, interrupt1, interrupt2, interrupt3, interrupt4);
#endif
		if(interrupt1 & (m_INT_HOTPLUG | m_INT_MSENS))
		{
//			if(hdmi->state == HDMI_SLEEP)
//				hdmi->state = WAIT_HOTPLUG;
			interrupt1 &= ~(m_INT_HOTPLUG | m_INT_MSENS);
			hdmi_submit_work(hdmi, HDMI_HPD_CHANGE, 10, NULL);
		}
		else if(interrupt1 & (m_INT_EDID_READY | m_INT_EDID_ERR)) {
			spin_lock(&rk30_hdmi->irq_lock);
			edid_result = interrupt1;
			spin_unlock(&rk30_hdmi->irq_lock);
		}
//		else if(hdmi->state == HDMI_SLEEP) {
//			RK30DBG( "hdmi return to sleep mode\n");
//			HDMIWrReg(SYS_CTRL, 0x10);
//			rk30_hdmi->pwr_mode = PWR_SAVE_MODE_A;
//		}
		if(interrupt2 && hdmi->ops->hdcp_irq_cb)
			hdmi->ops->hdcp_irq_cb(interrupt2);
	}
	return IRQ_HANDLED;
}

