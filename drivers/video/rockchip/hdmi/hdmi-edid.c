#include "rk_hdmi.h"
#include "../../edid.h"

#define hdmi_edid_error(fmt, ...) \
        printk(KERN_ERR pr_fmt(fmt), ##__VA_ARGS__)

#if 0
#define hdmi_edid_debug(fmt, ...) \
        printk(KERN_INFO pr_fmt(fmt), ##__VA_ARGS__)
#else
#define hdmi_edid_debug(fmt, ...)	
#endif

typedef enum HDMI_EDID_ERRORCODE
{
	E_HDMI_EDID_SUCCESS = 0,
	E_HDMI_EDID_PARAM,
	E_HDMI_EDID_HEAD,
	E_HDMI_EDID_CHECKSUM,
	E_HDMI_EDID_VERSION,
	E_HDMI_EDID_UNKOWNDATA,
	E_HDMI_EDID_NOMEMORY
}HDMI_EDID_ErrorCode;

static int hdmi_edid_checksum(unsigned char *buf)
{
	int i;
	int checksum = 0;
	
	for(i = 0; i < HDMI_EDID_BLOCK_SIZE; i++)
		checksum += buf[i];	
	
	checksum &= 0xff;
	
	if(checksum == 0)
		return E_HDMI_EDID_SUCCESS;
	else
		return E_HDMI_EDID_CHECKSUM;
}

/*
	@Des	Parse Detail Timing Descriptor.
	@Param	buf	:	pointer to DTD data.
	@Param	pvic:	VIC of DTD descripted.
 */
static int hdmi_edid_parse_dtd(unsigned char *block, struct fb_videomode *mode)
{
	mode->xres = H_ACTIVE;
	mode->yres = V_ACTIVE;
	mode->pixclock = PIXEL_CLOCK;
//	mode->pixclock /= 1000;
//	mode->pixclock = KHZ2PICOS(mode->pixclock);
	mode->right_margin = H_SYNC_OFFSET;
	mode->left_margin = (H_ACTIVE + H_BLANKING) -
		(H_ACTIVE + H_SYNC_OFFSET + H_SYNC_WIDTH);
	mode->upper_margin = V_BLANKING - V_SYNC_OFFSET -
		V_SYNC_WIDTH;
	mode->lower_margin = V_SYNC_OFFSET;
	mode->hsync_len = H_SYNC_WIDTH;
	mode->vsync_len = V_SYNC_WIDTH;
	if (HSYNC_POSITIVE)
		mode->sync |= FB_SYNC_HOR_HIGH_ACT;
	if (VSYNC_POSITIVE)
		mode->sync |= FB_SYNC_VERT_HIGH_ACT;
	mode->refresh = PIXEL_CLOCK/((H_ACTIVE + H_BLANKING) *
				     (V_ACTIVE + V_BLANKING));
	if (INTERLACED) {
		mode->yres *= 2;
		mode->upper_margin *= 2;
		mode->lower_margin *= 2;
		mode->vsync_len *= 2;
		mode->vmode |= FB_VMODE_INTERLACED;
	}
	mode->flag = FB_MODE_IS_DETAILED;

	hdmi_edid_debug("<<<<<<<<Detailed Time>>>>>>>>>\n");
	hdmi_edid_debug("%d KHz Refresh %d Hz",  PIXEL_CLOCK/1000, mode->refresh);
	hdmi_edid_debug("%d %d %d %d ", H_ACTIVE, H_ACTIVE + H_SYNC_OFFSET,
	       H_ACTIVE + H_SYNC_OFFSET + H_SYNC_WIDTH, H_ACTIVE + H_BLANKING);
	hdmi_edid_debug("%d %d %d %d ", V_ACTIVE, V_ACTIVE + V_SYNC_OFFSET,
	       V_ACTIVE + V_SYNC_OFFSET + V_SYNC_WIDTH, V_ACTIVE + V_BLANKING);
	hdmi_edid_debug("%sHSync %sVSync\n\n", (HSYNC_POSITIVE) ? "+" : "-",
	       (VSYNC_POSITIVE) ? "+" : "-");
	return E_HDMI_EDID_SUCCESS;
}

int hdmi_edid_parse_base(unsigned char *buf, int *extend_num, struct hdmi_edid *pedid)
{
	int rc, i;
	
	if(buf == NULL || extend_num == NULL)
		return E_HDMI_EDID_PARAM;
		
//	#ifdef DEBUG	
//	for(i = 0; i < HDMI_EDID_BLOCK_SIZE; i++)
//	{
//		hdmi_edid_debug("%02x ", buf[i]&0xff);
//		if((i+1) % 16 == 0)
//			hdmi_edid_debug("\n");
//	}
//	#endif
	
	// Check first 8 byte to ensure it is an edid base block.
	if( buf[0] != 0x00 ||
	    buf[1] != 0xFF ||
	    buf[2] != 0xFF ||
	    buf[3] != 0xFF ||
	    buf[4] != 0xFF ||
	    buf[5] != 0xFF ||
	    buf[6] != 0xFF ||
	    buf[7] != 0x00)
    {
        hdmi_edid_error("[EDID] check header error\n");
        return E_HDMI_EDID_HEAD;
    }
    
    *extend_num = buf[0x7e];
    #ifdef DEBUG
    hdmi_edid_debug("[EDID] extend block num is %d\n", buf[0x7e]);
    #endif
    
    // Checksum
    rc = hdmi_edid_checksum(buf);
    if( rc != E_HDMI_EDID_SUCCESS)
    {
    	hdmi_edid_error("[EDID] base block checksum error\n");
    	return E_HDMI_EDID_CHECKSUM;
    }

	pedid->specs = kzalloc(sizeof(struct fb_monspecs), GFP_KERNEL);
	if(pedid->specs == NULL)
		return E_HDMI_EDID_NOMEMORY;
		
	fb_edid_to_monspecs(buf, pedid->specs);
	
    return E_HDMI_EDID_SUCCESS;
}

// Parse CEA Short Video Descriptor
static int hdmi_edid_get_cea_svd(unsigned char *buf, struct hdmi_edid *pedid)
{
	int count, i, vic;
	
	count = buf[0] & 0x1F;
	for(i = 0; i < count; i++)
	{
		hdmi_edid_debug("[EDID-CEA] %02x VID %d native %d\n", buf[1 + i], buf[1 + i] & 0x7f, buf[1 + i] >> 7);
		vic = buf[1 + i] & 0x7f;
		hdmi_add_vic(vic, &pedid->modelist);
	}
	
//	struct list_head *pos;
//	struct display_modelist *modelist;
//
//	list_for_each(pos, &pedid->modelist) {
//		modelist = list_entry(pos, struct display_modelist, list);
//		printk("%s vic %d\n", __FUNCTION__, modelist->vic);
//	}	
	return 0;
}

// Parse CEA Short Audio Descriptor
static int hdmi_edid_parse_cea_sad(unsigned char *buf, struct hdmi_edid *pedid)
{
	int i, count;
	
	count = buf[0] & 0x1F;
	pedid->audio = kmalloc((count/3)*sizeof(struct hdmi_audio), GFP_KERNEL);
	if(pedid->audio == NULL)
		return E_HDMI_EDID_NOMEMORY;

	pedid->audio_num = count/3;
	for(i = 0; i < pedid->audio_num; i++)
	{
		pedid->audio[i].type = (buf[1 + i*3] >> 3) & 0x0F;
		pedid->audio[i].channel = (buf[1 + i*3] & 0x07) + 1;
		pedid->audio[i].rate = buf[1 + i*3 + 1];
		if(pedid->audio[i].type == HDMI_AUDIO_LPCM)//LPCM 
		{
			pedid->audio[i].word_length = buf[1 + i*3 + 2];
		}
//		printk("[EDID-CEA] type %d channel %d rate %d word length %d\n", 
//			pedid->audio[i].type, pedid->audio[i].channel, pedid->audio[i].rate, pedid->audio[i].word_length);
	}
	return E_HDMI_EDID_SUCCESS;
}

static int hdmi_edid_parse_3dinfo(unsigned char *buf, struct list_head *head)
{
	int i, j, len = 0, format_3d, vic_mask;
	unsigned char offset = 2, vic_2d, structure_3d;
	struct list_head *pos;
	struct display_modelist *modelist;
	
	if(buf[1] & 0xF0) {
		len = (buf[1] & 0xF0) >> 4;
		for(i = 0; i < len; i++) {
			hdmi_add_vic( (buf[offset++] | HDMI_VIDEO_EXT), head);
		}
	}
	
	if(buf[0] & 0x80) {
		//3d supported
		len += (buf[0] & 0x0F) + 2;
		if( ( (buf[0] & 0x60) == 0x40) || ( (buf[0] & 0x60) == 0x20) ) {
			format_3d = buf[offset++] << 8;
			format_3d |= buf[offset++];
		}
		if( (buf[0] & 0x60) == 0x40)
			vic_mask = 0xFFFF;
		else {
			vic_mask  = buf[offset++] << 8;
			vic_mask |= buf[offset++];
		}

		for(i = 0; i < 16; i++)
		{
			if(vic_mask & (1 << i)) {
				j = 0;
				for (pos = (head)->next; pos != (head); pos = pos->next) {
					j++;
					if(j == i) {
						modelist = list_entry(pos, struct display_modelist, list);
						modelist->format_3d = format_3d;
						break;
					}
				}
			}
		}
		while(offset < len)
		{
			vic_2d = (buf[offset] & 0xF0) >> 4;
			structure_3d = (buf[offset++] & 0x0F);
			j = 0;
			for (pos = (head)->next; pos != (head); pos = pos->next) {
				j++;
				if(j == vic_2d) {
					modelist = list_entry(pos, struct display_modelist, list);
					modelist->format_3d = format_3d;
					if(structure_3d & 0x80)
					modelist->detail_3d = (buf[offset++] & 0xF0) >> 4;
					break;
				}
			}
		}
	}
	
	return 0;
}

// Parse CEA 861 Serial Extension.
static int hdmi_edid_parse_extensions_cea(unsigned char *buf, struct hdmi_edid *pedid)
{
	unsigned int ddc_offset, native_dtd_num, cur_offset = 4, buf_offset;
//	unsigned int underscan_support, baseaudio_support;
	unsigned int tag, IEEEOUI = 0, count;
	
	if(buf == NULL)
		return E_HDMI_EDID_PARAM;
		
	// Check ces extension version
	if(buf[1] != 3)
	{
		hdmi_edid_error("[EDID-CEA] error version.\n");
		return E_HDMI_EDID_VERSION;
	}
	
	ddc_offset = buf[2];
//	underscan_support = (buf[3] >> 7) & 0x01;
//	baseaudio_support = (buf[3] >> 6) & 0x01;
	pedid->ycbcr444 = (buf[3] >> 5) & 0x01;
	pedid->ycbcr422 = (buf[3] >> 4) & 0x01;
	native_dtd_num = buf[3] & 0x0F;
//	hdmi_edid_debug("[EDID-CEA] ddc_offset %d underscan_support %d baseaudio_support %d yuv_support %d native_dtd_num %d\n", ddc_offset, underscan_support, baseaudio_support, yuv_support, native_dtd_num);
	// Parse data block
	while(cur_offset < ddc_offset)
	{
		tag = buf[cur_offset] >> 5;
		count = buf[cur_offset] & 0x1F;
		switch(tag)
		{
			case 0x02:	// Video Data Block
				hdmi_edid_debug("[EDID-CEA] It is a Video Data Block.\n");
				hdmi_edid_get_cea_svd(buf + cur_offset, pedid);
				break;
			case 0x01:	// Audio Data Block
				hdmi_edid_debug("[EDID-CEA] It is a Audio Data Block.\n");
				hdmi_edid_parse_cea_sad(buf + cur_offset, pedid);
				break;
			case 0x04:	// Speaker Allocation Data Block
				hdmi_edid_debug("[EDID-CEA] It is a Speaker Allocatio Data Block.\n");
				break;
			case 0x03:	// Vendor Specific Data Block
				hdmi_edid_debug("[EDID-CEA] It is a Vendor Specific Data Block.\n");

				IEEEOUI = buf[cur_offset + 3];
				IEEEOUI <<= 8;
				IEEEOUI += buf[cur_offset + 2];
				IEEEOUI <<= 8;
				IEEEOUI += buf[cur_offset + 1];
				hdmi_edid_debug("[EDID-CEA] IEEEOUI is 0x%08x.\n", IEEEOUI);
				if(IEEEOUI == 0x0c03)
					pedid->sink_hdmi = 1;
				pedid->cecaddress = buf[cur_offset + 5];
				pedid->cecaddress |= buf[cur_offset + 4] << 8;
				hdmi_edid_debug("[EDID-CEA] CEC Physical addres is 0x%08x.\n", pedid->cecaddress);
				if(count > 6)
					pedid->deepcolor = (buf[cur_offset + 6] >> 3) & 0x0F;					
				if(count > 7) {
					pedid->maxtmdsclock = buf[cur_offset + 7] * 5000000;
					hdmi_edid_debug("[EDID-CEA] maxtmdsclock is %d.\n", pedid->maxtmdsclock);
				}
				if(count > 8) {
					pedid->fields_present = buf[cur_offset + 8];
					hdmi_edid_debug("[EDID-CEA] fields_present is 0x%02x.\n", pedid->fields_present);
				}
				buf_offset = cur_offset + 9;		
				if(pedid->fields_present & 0x80)
				{
					pedid->video_latency = buf[buf_offset++];
					pedid->audio_latency = buf[buf_offset++];
				}
				if(pedid->fields_present & 0x40)
				{
					pedid->interlaced_video_latency = buf[buf_offset++];
					pedid->interlaced_audio_latency = buf[buf_offset++];
				}
				if(pedid->fields_present & 0x20) {
					hdmi_edid_parse_3dinfo(buf + buf_offset, &pedid->modelist);
				}
				break;		
			case 0x05:	// VESA DTC Data Block
				hdmi_edid_debug("[EDID-CEA] It is a VESA DTC Data Block.\n");
				break;
			case 0x07:	// Use Extended Tag
				hdmi_edid_debug("[EDID-CEA] It is a Use Extended Tag Data Block.\n");
				break;
			default:
				hdmi_edid_error("[EDID-CEA] unkowned data block tag.\n");
				break;
		}
		cur_offset += (buf[cur_offset] & 0x1F) + 1;
	}
#if 1	
{
	// Parse DTD
	struct fb_videomode *vmode = kmalloc(sizeof(struct fb_videomode), GFP_KERNEL);
	if(vmode == NULL)
		return E_HDMI_EDID_SUCCESS; 
	while(ddc_offset < HDMI_EDID_BLOCK_SIZE - 2)	//buf[126] = 0 and buf[127] = checksum
	{
		if(!buf[ddc_offset] && !buf[ddc_offset + 1])
			break;
		memset(vmode, 0, sizeof(struct fb_videomode));
		hdmi_edid_parse_dtd(buf + ddc_offset, vmode);		
		hdmi_add_vic(hdmi_videomode_to_vic(vmode), &pedid->modelist);
		ddc_offset += 18;
	}
	kfree(vmode);
}
#endif
	return E_HDMI_EDID_SUCCESS;
}

int hdmi_edid_parse_extensions(unsigned char *buf, struct hdmi_edid *pedid)
{
	int rc;
	
	if(buf == NULL || pedid == NULL)
		return E_HDMI_EDID_PARAM;
		
	// Checksum
    rc = hdmi_edid_checksum(buf);
    if( rc != E_HDMI_EDID_SUCCESS)
    {
    	hdmi_edid_error("[EDID] extensions block checksum error\n");
    	return E_HDMI_EDID_CHECKSUM;
    }
    
    switch(buf[0])
    {
    	case 0xF0:
    		hdmi_edid_debug("[EDID-EXTEND] It is a extensions block map.\n");
    		break;
    	case 0x02:
    		hdmi_edid_debug("[EDID-EXTEND] It is a  CEA 861 Series Extension.\n");
    		hdmi_edid_parse_extensions_cea(buf, pedid);
    		break;
    	case 0x10:
    		hdmi_edid_debug("[EDID-EXTEND] It is a Video Timing Block Extension.\n");
    		break;
    	case 0x40:
    		hdmi_edid_debug("[EDID-EXTEND] It is a Display Information Extension.\n");
    		break;
    	case 0x50:
    		hdmi_edid_debug("[EDID-EXTEND] It is a Localized String Extension.\n");
    		break;
    	case 0x60:
    		hdmi_edid_debug("[EDID-EXTEND] It is a Digital Packet Video Link Extension.\n");
    		break;
    	default:
    		hdmi_edid_error("[EDID-EXTEND] Unkowned extension.\n");
    		return E_HDMI_EDID_UNKOWNDATA;
    }
    
    return E_HDMI_EDID_SUCCESS;
}
