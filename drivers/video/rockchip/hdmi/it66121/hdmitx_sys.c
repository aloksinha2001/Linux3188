///*****************************************
//  Copyright (C) 2009-2014
//  ITE Tech. Inc. All Rights Reserved
//  Proprietary and Confidential
///*****************************************
//   @file   <hdmitx_sys.c>
//   @author Jau-Chih.Tseng@ite.com.tw
//   @date   2012/07/05
//   @fileversion: ITE_HDMITX_SAMPLE_3.11
//******************************************/

///////////////////////////////////////////////////////////////////////////////
// This is the sample program for CAT6611 driver usage.
///////////////////////////////////////////////////////////////////////////////
#include "it66121.h"

static HDMITXDEV InstanceData =
{

    0,      // BYTE I2C_DEV ;
    HDMI_TX_I2C_SLAVE_ADDR,    // BYTE I2C_ADDR ;

    /////////////////////////////////////////////////
    // Interrupt Type
    /////////////////////////////////////////////////
    0x40,      // BYTE bIntType ; // = 0 ;
    /////////////////////////////////////////////////
    // Video Property
    /////////////////////////////////////////////////
    INPUT_SIGNAL_TYPE ,// BYTE bInputVideoSignalType ; // for Sync Embedded,CCIR656,InputDDR

    /////////////////////////////////////////////////
    // Audio Property
    /////////////////////////////////////////////////
    I2S_FORMAT, // BYTE bOutputAudioMode ; // = 0 ;
    FALSE , // BYTE bAudioChannelSwap ; // = 0 ;
    0x01, // BYTE bAudioChannelEnable ;
    INPUT_SAMPLE_FREQ ,// BYTE bAudFs ;
    0, // unsigned long TMDSClock ;
    FALSE, // BYTE bAuthenticated:1 ;
    FALSE, // BYTE bHDMIMode: 1;
    FALSE, // BYTE bIntPOL:1 ; // 0 = Low Active
    FALSE, // BYTE bHPD:1 ;
};

static unsigned char CommunBuff[128] ;

static void ConfigAVIInfoFrame(BYTE VIC, BYTE pixelrep, BYTE aspec, BYTE Colorimetry, BYTE bOutputColorMode)
{
    AVI_InfoFrame *AviInfo;
    AviInfo = (AVI_InfoFrame *)CommunBuff ;

    AviInfo->pktbyte.AVI_HB[0] = AVI_INFOFRAME_TYPE|0x80 ;
    AviInfo->pktbyte.AVI_HB[1] = AVI_INFOFRAME_VER ;
    AviInfo->pktbyte.AVI_HB[2] = AVI_INFOFRAME_LEN ;

    switch(bOutputColorMode)
    {
    case F_MODE_YUV444:
        // AviInfo->info.ColorMode = 2 ;
        AviInfo->pktbyte.AVI_DB[0] = (2<<5)|(1<<4);
        break ;
    case F_MODE_YUV422:
        // AviInfo->info.ColorMode = 1 ;
        AviInfo->pktbyte.AVI_DB[0] = (1<<5)|(1<<4);
        break ;
    case F_MODE_RGB444:
    default:
        // AviInfo->info.ColorMode = 0 ;
        AviInfo->pktbyte.AVI_DB[0] = (0<<5)|(1<<4);
        break ;
    }
    AviInfo->pktbyte.AVI_DB[1] = 8 ;
    AviInfo->pktbyte.AVI_DB[1] |= (aspec != HDMI_16x9)?(1<<4):(2<<4); // 4:3 or 16:9
    AviInfo->pktbyte.AVI_DB[1] |= (Colorimetry != HDMI_ITU709)?(1<<6):(2<<6); // 4:3 or 16:9
    AviInfo->pktbyte.AVI_DB[2] = 0 ;
    AviInfo->pktbyte.AVI_DB[3] = VIC ;
    AviInfo->pktbyte.AVI_DB[4] =  pixelrep & 3 ;
    AviInfo->pktbyte.AVI_DB[5] = 0 ;
    AviInfo->pktbyte.AVI_DB[6] = 0 ;
    AviInfo->pktbyte.AVI_DB[7] = 0 ;
    AviInfo->pktbyte.AVI_DB[8] = 0 ;
    AviInfo->pktbyte.AVI_DB[9] = 0 ;
    AviInfo->pktbyte.AVI_DB[10] = 0 ;
    AviInfo->pktbyte.AVI_DB[11] = 0 ;
    AviInfo->pktbyte.AVI_DB[12] = 0 ;

    HDMITX_EnableAVIInfoFrame(TRUE, (unsigned char *)AviInfo);
}

static void ConfigAudioInfoFrm(void)
{
    int i ;

    Audio_InfoFrame *AudioInfo ;
    AudioInfo = (Audio_InfoFrame *)CommunBuff ;

    HDMITX_DEBUG_PRINTF(("ConfigAudioInfoFrm(%d)\n",2));

    AudioInfo->pktbyte.AUD_HB[0] = AUDIO_INFOFRAME_TYPE ;
    AudioInfo->pktbyte.AUD_HB[1] = 1 ;
    AudioInfo->pktbyte.AUD_HB[2] = AUDIO_INFOFRAME_LEN ;
    AudioInfo->pktbyte.AUD_DB[0] = 1 ;
    for( i = 1 ;i < AUDIO_INFOFRAME_LEN ; i++ )
    {
        AudioInfo->pktbyte.AUD_DB[i] = 0 ;
    }
    HDMITX_EnableAudioInfoFrame(TRUE, (unsigned char *)AudioInfo);
}

int it66121_initial(void)
{
	unsigned char VendorID0, VendorID1, DeviceID0, DeviceID1;
	
	Switch_HDMITX_Bank(0);
	VendorID0 = HDMITX_ReadI2C_Byte(REG_TX_VENDOR_ID0);
	VendorID1 = HDMITX_ReadI2C_Byte(REG_TX_VENDOR_ID1);
	DeviceID0 = HDMITX_ReadI2C_Byte(REG_TX_DEVICE_ID0);
	DeviceID1 = HDMITX_ReadI2C_Byte(REG_TX_DEVICE_ID1);
	DBG("Reg[0-3] = 0x[%02x].[%02x].[%02x].[%02x]",
			   VendorID0, VendorID1, DeviceID0, DeviceID1);
	if( (VendorID0 == 0x54) && (VendorID1 == 0x49) &&
		(DeviceID0 == 0x12) && (DeviceID1 == 0x16) )
	{
		HDMITX_InitTxDev(&InstanceData);
		InitHDMITX();
		return 0;
	}
	printk(KERN_ERR "IT66121: Device not found!\n");
	return 1;
}

static int HPDStatus = 0;

int it66121_poll_status(struct hdmi *hdmi)
{
	char HPDChangeStatus;
	CheckHDMITX((BYTE*)&HPDStatus, &HPDChangeStatus);
	if(HPDChangeStatus)
		hdmi_submit_work(hdmi, HDMI_HPD_CHANGE, 10, NULL);
	return HDMI_ERROR_SUCESS;
}

int it66121_detect_hotplug(struct hdmi *hdmi)
{
	return HPDStatus?HDMI_HPD_ACTIVED:HDMI_HPD_REMOVED;
}

int it66121_insert(struct hdmi *hdmi)
{	
	HDMITX_PowerOn();
	return HDMI_ERROR_SUCESS;
}

int it66121_remove(struct hdmi *hdmi)
{
	HDMITX_DEBUG_PRINTF(("HPD OFF HDMITX_DisableVideoOutput()\n"));
    HDMITX_DisableVideoOutput();
    HDMITX_PowerDown();
	return HDMI_ERROR_SUCESS;
}


int it66121_read_edid(struct hdmi *hdmi, int block, unsigned char *buff)
{
	return (getHDMITX_EDIDBlock(block, buff) == TRUE)?HDMI_ERROR_SUCESS:HDMI_ERROR_FALSE;;
}

int it66121_config_video(struct hdmi *hdmi, struct hdmi_video *vpara)
{
	struct it66121 *it66121 = hdmi->property->priv;
	VIDEOPCLKLEVEL level ;
    struct fb_videomode *vmode;
	char bHDMIMode, pixelrep, bInputColorMode, bOutputColorMode, aspec, Colorimetry;
	
	vmode = (struct fb_videomode*)hdmi_vic_to_videomode(vpara->vic);
	if(vmode == NULL)
		return HDMI_ERROR_FALSE;
	
	it66121->tmdsclk = vmode->pixclock;
	bHDMIMode = hdmi->edid.sink_hdmi;
	
	if(vmode->xres == 1280 || vmode->xres == 1920)
    {
    	aspec = HDMI_16x9;
    	Colorimetry = HDMI_ITU709;
    }
    else
    {
    	aspec = HDMI_4x3;
    	Colorimetry = HDMI_ITU601;
    }
    
    if(vmode->xres == 1440)
    	pixelrep = 1;
    else if(vmode->xres == 2880)
    	pixelrep = 3;
    else
    	pixelrep = 0;
	bInputColorMode = vpara->color_input & 0xFF;
	switch(vpara->color_output)
    {
	    case HDMI_COLOR_YCbCr444:
	        bOutputColorMode = F_MODE_YUV444 ;
	        break ;
	    case HDMI_COLOR_YCbCr422:	
	        bOutputColorMode = F_MODE_YUV422 ;
	        break ;
	    case HDMI_COLOR_RGB:
	        bOutputColorMode = F_MODE_RGB444 ;
	        break ;	
	    default:
	        bOutputColorMode = F_MODE_RGB444 ;
	        break ;
    }
	
    HDMITX_DisableAudioOutput();
	HDMITX_EnableHDCP(FALSE);

    if( it66121->tmdsclk > 80000000L )
    {
        level = PCLK_HIGH ;
    }
    else if(it66121->tmdsclk > 20000000L)
    {
        level = PCLK_MEDIUM ;
    }
    else
    {
        level = PCLK_LOW ;
    }
#ifdef IT6615
	HDMITX_DEBUG_PRINTF(("OutputColorDepth = %02X\n",(int)OutputColorDepth)) ;
    setHDMITX_ColorDepthPhase(OutputColorDepth,0);
#endif

	setHDMITX_VideoSignalType(InstanceData.bInputVideoSignalType);
    #ifdef SUPPORT_SYNCEMBEDDED
	if(InstanceData.bInputVideoSignalType & T_MODE_SYNCEMB)
	{
	    setHDMITX_SyncEmbeddedByVIC(VIC,InstanceData.bInputVideoSignalType);
	}
    #endif

    HDMITX_DEBUG_PRINTF(("level = %d, ,bInputColorMode=%x,bOutputColorMode=%x,bHDMIMode=%x\n",(int)level,(int)bInputColorMode,(int)bOutputColorMode ,(int)bHDMIMode)) ;
	HDMITX_EnableVideoOutput(level,bInputColorMode,bOutputColorMode ,bHDMIMode);

    if( bHDMIMode )
    {
        #ifdef OUTPUT_3D_MODE
        ConfigfHdmiVendorSpecificInfoFrame(OUTPUT_3D_MODE);
        #endif
        ConfigAVIInfoFrame(vpara->vic, pixelrep, aspec, Colorimetry, bOutputColorMode);
    }
	else
	{
		HDMITX_EnableAVIInfoFrame(FALSE ,NULL);
        HDMITX_EnableVSInfoFrame(FALSE,NULL);
	}
    setHDMITX_AVMute(FALSE);
    DumpHDMITXReg() ;
	
	return HDMI_ERROR_SUCESS;
}

int it66121_config_audio(struct hdmi *hdmi, struct hdmi_audio *audio)
{
    struct it66121 *it66121 = hdmi->property->priv;
    unsigned long ulAudioSampleFS;
	unsigned char word_length;
	
	switch(audio->rate)
	{
		case HDMI_AUDIO_FS_32000:
			ulAudioSampleFS = 32000;
			break;
		case HDMI_AUDIO_FS_44100:
			ulAudioSampleFS = 44100;
			break;
		case HDMI_AUDIO_FS_48000:
			ulAudioSampleFS = 48000;
			break;
		case HDMI_AUDIO_FS_88200:
			ulAudioSampleFS = 88200;
			break;
		case HDMI_AUDIO_FS_96000:
			ulAudioSampleFS = 96000;
			break;
		case HDMI_AUDIO_FS_176400:
			ulAudioSampleFS = 176400;
			break;
		case HDMI_AUDIO_FS_192000:
			ulAudioSampleFS = 192000;
		default:
			printk("[%s] not support such sample rate\n", __FUNCTION__);
			return HDMI_ERROR_FALSE;
	}
	switch(audio->word_length)
	{
		case HDMI_AUDIO_WORD_LENGTH_16bit:
			word_length = 16;
			break;
		case HDMI_AUDIO_WORD_LENGTH_20bit:
			word_length = 20;
			break;
		case HDMI_AUDIO_WORD_LENGTH_24bit:
			word_length = 24;
			break;
		default:
			word_length = 24;
			break;
	}
    HDMITX_EnableAudioOutput(
    CONFIG_INPUT_AUDIO_TYPE,
    CONFIG_INPUT_AUDIO_SPDIF,
    ulAudioSampleFS,
    audio->channel,
    NULL, // pointer to cahnnel status.
    it66121->tmdsclk);
    ConfigAudioInfoFrm();
	return HDMI_ERROR_SUCESS;
}

int it66121_set_output(struct hdmi *hdmi, int enable)
{
	return HDMI_ERROR_SUCESS;
}