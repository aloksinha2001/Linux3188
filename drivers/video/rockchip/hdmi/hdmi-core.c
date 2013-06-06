#include "rk_hdmi.h"

struct hdmi_delayed_work {
	struct delayed_work work;
	struct hdmi *hdmi;
	int event;
	void *data;
};

static int hdmi_number = 0;
static void hdmi_work_queue(struct work_struct *work);

struct delayed_work *hdmi_submit_work(struct hdmi *hdmi, int event, int delay, void *data)
{
	struct hdmi_delayed_work *work;

	DBG("%s event %04x delay %d", __FUNCTION__, event, delay);
	
	work = kmalloc(sizeof(struct hdmi_delayed_work), GFP_ATOMIC);

	if (work) {
		INIT_DELAYED_WORK(&work->work, hdmi_work_queue);
		work->hdmi = hdmi;
		work->event = event;
		work->data = data;
		queue_delayed_work(hdmi->workqueue,
				   &work->work,
				   msecs_to_jiffies(delay));
	} else {
		printk(KERN_WARNING "HDMI: Cannot allocate memory to "
				    "create work\n");
		return 0;
	}
	
	return &work->work;
}

static void hdmi_send_uevent(struct hdmi *hdmi, int uevent)
{
	char *envp[3];
	
	envp[0] = "INTERFACE=HDMI";
	envp[1] = kmalloc(32, GFP_KERNEL);
	if(envp[1] == NULL)	return;
	sprintf(envp[1], "SCREEN=%d", hdmi->ddev->property);
	envp[2] = NULL;
	kobject_uevent_env(&hdmi->ddev->dev->kobj, uevent, envp);
	kfree(envp[1]);
}

static inline void hdmi_wq_set_output(struct hdmi *hdmi, int mute)
{
	DBG("%s mute %d", __FUNCTION__, mute);
	if(hdmi->ops->setMute)
		hdmi->ops->setMute(hdmi, mute);
}

static inline void hdmi_wq_set_audio(struct hdmi *hdmi)
{
	DBG("%s", __FUNCTION__);
	if(hdmi->ops->setAudio)
		hdmi->ops->setAudio(hdmi, &hdmi->audio);
}

static void hdmi_wq_set_video(struct hdmi *hdmi)
{
	struct hdmi_video	video;
	
	DBG("%s", __FUNCTION__);
	
	hdmi_set_lcdc(hdmi);
	video.vic = hdmi->vic;
	video.color_input = HDMI_COLOR_RGB;
	video.sink_hdmi = hdmi->edid.sink_hdmi;
	video.format_3d = hdmi->mode_3d;
	// For DVI, output RGB
	if(hdmi->edid.sink_hdmi == 0)
		video.color_output = HDMI_COLOR_RGB;
	else {
		if(hdmi->edid.ycbcr444)
			video.color_output = HDMI_COLOR_YCbCr444;
		else if(hdmi->edid.ycbcr422)
			video.color_output = HDMI_COLOR_YCbCr422;
		else
			video.color_output = HDMI_COLOR_RGB;
	}
	if(hdmi->ops->setVideo)
		hdmi->ops->setVideo(hdmi, &video);
}

static void hdmi_wq_parse_edid(struct hdmi *hdmi)
{
	struct hdmi_edid *pedid;
	unsigned char *buff = NULL;
	int rc = HDMI_ERROR_SUCESS, extendblock = 0, i;
	
	if(hdmi == NULL) return;
		
	DBG("%s", __FUNCTION__);
	
	pedid = &(hdmi->edid);
	fb_destroy_modelist(&pedid->modelist);
	memset(pedid, 0, sizeof(struct hdmi_edid));
	INIT_LIST_HEAD(&pedid->modelist);
	
	buff = kmalloc(HDMI_EDID_BLOCK_SIZE, GFP_KERNEL);
	if(buff == NULL) {		
		dev_err(hdmi->dev, "[%s] can not allocate memory for edid buff.\n", __FUNCTION__);
		rc = HDMI_ERROR_FALSE;
		goto out;
	}
	
	if(hdmi->ops->getEdid == NULL) {
		rc = HDMI_ERROR_FALSE;
		goto out;
	}
	
	// Read base block edid.
	memset(buff, 0 , HDMI_EDID_BLOCK_SIZE);
	rc = hdmi->ops->getEdid(hdmi, 0, buff);
	if(rc) {
		dev_err(hdmi->dev, "[HDMI] read edid base block error\n");
		goto out;
	}
	
	rc = hdmi_edid_parse_base(buff, &extendblock, pedid);
	if(rc) {
		dev_err(hdmi->dev, "[HDMI] parse edid base block error\n");
		goto out;
	}
	
	for(i = 1; i < extendblock + 1; i++) {
		memset(buff, 0 , HDMI_EDID_BLOCK_SIZE);
		rc = hdmi->ops->getEdid(hdmi, i, buff);
		if(rc) {
			printk("[HDMI] read edid block %d error\n", i);	
			goto out;
		}

		rc = hdmi_edid_parse_extensions(buff, pedid);
		if(rc) {
			dev_err(hdmi->dev, "[HDMI] parse edid block %d error\n", i);
			continue;
		}
	}
out:
	if(buff)
		kfree(buff);
	rc = hdmi_ouputmode_select(hdmi, rc);
}

static void hdmi_wq_insert(struct hdmi *hdmi)
{
	DBG("%s", __FUNCTION__);
	hdmi_send_uevent(hdmi, KOBJ_ADD);
	if(hdmi->ops->insert)
		hdmi->ops->insert(hdmi);
	hdmi_wq_parse_edid(hdmi);
	hdmi_wq_set_video(hdmi);
	hdmi_wq_set_audio(hdmi);
	hdmi_wq_set_output(hdmi, hdmi->mute);
	if(hdmi->ops->hdcp_cb) {
		hdmi->ops->hdcp_cb();
	}
	if(hdmi->ops->setCEC)
		hdmi->ops->setCEC(hdmi);
}

static void hdmi_wq_remove(struct hdmi *hdmi)
{
	struct list_head *pos, *n;
	DBG("%s", __FUNCTION__);
	
	if(hdmi->ops->remove)
		hdmi->ops->remove(hdmi);

	list_for_each_safe(pos, n, &hdmi->edid.modelist) {
		list_del(pos);
		kfree(pos);
	}

	if(hdmi->edid.audio)
		kfree(hdmi->edid.audio);

	if(hdmi->edid.specs) {
		if(hdmi->edid.specs->modedb)
			kfree(hdmi->edid.specs->modedb);
		kfree(hdmi->edid.specs);
	}
	memset(&hdmi->edid, 0, sizeof(struct hdmi_edid));
	hdmi_init_modelist(hdmi);
	hdmi->mute	= HDMI_AV_UNMUTE;
	hdmi->hotplug = HDMI_HPD_REMOVED;
	hdmi->mode_3d = HDMI_3D_NONE;
	rk_fb_switch_screen(NULL, 0, hdmi->lcdc->id);
	hdmi_send_uevent(hdmi, KOBJ_REMOVE);
	#ifdef CONFIG_SWITCH
	switch_set_state(&(hdmi->switchdev), 0);
	#endif
}

static void hdmi_work_queue(struct work_struct *work)
{
	struct hdmi_delayed_work *hdmi_w =
		container_of(work, struct hdmi_delayed_work, work.work);
	struct hdmi *hdmi = hdmi_w->hdmi;
	int event = hdmi_w->event;
	int hpd = HDMI_HPD_REMOVED;
	
	mutex_lock(&hdmi->lock);
	
	DBG("\nhdmi_work_queue() - evt= %x %d",
		(event & 0xFF00) >> 8,
		event & 0xFF);
	
	switch(event) {
		case HDMI_ENABLE_CTL:
			if(!hdmi->enable) {
				if(!hdmi->sleep && hdmi->ops->enable)
					hdmi->ops->enable(hdmi);
				hdmi->enable = 1;
			}
			break;
		case HDMI_RESUME_CTL:
			if(hdmi->sleep) {
				if(hdmi->enable && hdmi->ops->enable)
					hdmi->ops->enable(hdmi);
				hdmi->sleep = 0;
			}
			break;
		case HDMI_DISABLE_CTL:
			if(hdmi->enable) {
				if(!hdmi->sleep) {
					if(hdmi->ops->disable)
						hdmi->ops->disable(hdmi);
					hdmi_wq_remove(hdmi);
				}
				hdmi->enable = 0;
			}
			break;
		case HDMI_SUSPEND_CTL:
			if(!hdmi->sleep) {
				if(hdmi->enable) {
					if(hdmi->ops->disable)
						hdmi->ops->disable(hdmi);
					hdmi_wq_remove(hdmi);
				}
				hdmi->sleep = 1;
			}
			break;
		case HDMI_HPD_CHANGE:
			if(hdmi->ops->getStatus)
				hpd = hdmi->ops->getStatus(hdmi);
			DBG("hdmi_work_queue() - hpd is %d hotplug is %d", hpd, hdmi->hotplug);
			if(hpd != hdmi->hotplug) {
				hdmi->hotplug = hpd;
				if(hpd == HDMI_HPD_ACTIVED) {
					hdmi_wq_insert(hdmi);
				}
				else if(hdmi->hotplug == HDMI_HPD_REMOVED) {
					hdmi_wq_remove(hdmi);
				}
			}
			break;
		case HDMI_SET_VIDEO:
			hdmi_wq_set_output(hdmi, HDMI_VIDEO_MUTE | HDMI_AUDIO_MUTE);
			hdmi_wq_set_video(hdmi);
			hdmi_wq_set_audio(hdmi);
			hdmi_wq_set_output(hdmi, hdmi->mute);
			if(hdmi->ops->hdcp_cb) {
				hdmi->ops->hdcp_cb();
			}
			break;
		case HDMI_SET_AUDIO:
			if((hdmi->mute & HDMI_AUDIO_MUTE) == 0) {
				hdmi_wq_set_output(hdmi, HDMI_AUDIO_MUTE);
				hdmi_wq_set_audio(hdmi);
				hdmi_wq_set_output(hdmi, hdmi->mute);
			}
			break;
		case HDMI_SET_3D:
			if(hdmi->ops->setVSI) {
				if(hdmi->mode_3d != HDMI_3D_NONE)
					hdmi->ops->setVSI(hdmi, hdmi->mode_3d, HDMI_VIDEO_FORMAT_3D);
				else if( (hdmi->vic & HDMI_TYPE_MASK) == 0)
					hdmi->ops->setVSI(hdmi, hdmi->vic, HDMI_VIDEO_FORMAT_NORMAL);
			}
			break;
		default:
			printk(KERN_ERR "HDMI: hdmi_work_queue() unkown event\n");
			break;
	}
	
	if(hdmi_w->data)
		kfree(hdmi_w->data);
	kfree(hdmi_w);
	
	DBG("hdmi_work_queue() - exit\n");
	mutex_unlock(&hdmi->lock);
}

struct hdmi *hdmi_register(struct hdmi_property *property, struct hdmi_ops *ops)
{
	struct hdmi *hdmi;
	char name[32];
	
	if(property == NULL || ops == NULL) {
		printk(KERN_ERR "HDMI: %s invalid parameter\n", __FUNCTION__);
		return NULL;
	}
	
	DBG("hdmi_register() - video source %d display %d",
		 property->videosrc,  property->display);
	
	hdmi = kmalloc(sizeof(struct hdmi), GFP_KERNEL);
	if(!hdmi)
	{
    	printk(KERN_ERR "HDMI: no memory to allocate hdmi device.\n");
    	return NULL;
	}
	memset(hdmi, 0, sizeof(struct hdmi));
	mutex_init(&hdmi->lock);
	
	hdmi->property = property;
	hdmi->ops = ops;
	hdmi->enable = true;
	hdmi->mute = HDMI_AV_UNMUTE;
	hdmi->hotplug = HDMI_HPD_REMOVED;
	hdmi->autoset = HDMI_AUTO_CONFIG;
	hdmi->xscale = HDMI_DEFAULT_SCALE;
	hdmi->yscale = HDMI_DEFAULT_SCALE;
	hdmi->vic = HDMI_VIDEO_DEFAULT_MODE;
	hdmi->mode_3d = HDMI_3D_NONE;
	hdmi->audio.type = HDMI_AUDIO_DEFAULT_TYPE;
	hdmi->audio.channel = HDMI_AUDIO_DEFAULT_CHANNEL;
	hdmi->audio.rate = HDMI_AUDIO_DEFAULT_RATE;
	hdmi->audio.word_length = HDMI_AUDIO_DEFAULT_WORDLENGTH;
	hdmi_init_modelist(hdmi);
	
#ifndef CONFIG_ARCH_RK29	
	if(hdmi->property->videosrc == DISPLAY_SOURCE_LCDC0)
		hdmi->lcdc = rk_get_lcdc_drv("lcdc0");
	else
		hdmi->lcdc = rk_get_lcdc_drv("lcdc1");
#endif
	memset(name, 0, 32);
	sprintf(name, "hdmi-%s", hdmi->property->name);
	hdmi->workqueue = create_singlethread_workqueue(name);
	if (hdmi->workqueue == NULL) {
		printk(KERN_ERR "HDMI,: create workqueue failed.\n");
		goto err_create_wq;
	}
	hdmi->ddev = hdmi_register_display_sysfs(hdmi, NULL);
	if(hdmi->ddev == NULL) {
		printk(KERN_ERR "HDMI : register display sysfs failed.\n");
		goto err_register_display; 
	}
	rk_display_device_enable(hdmi->ddev);
	hdmi->id = hdmi_number++;
	#ifdef CONFIG_SWITCH
	if(hdmi->id == 0)
		hdmi->switchdev.name="hdmi";
	else {
		hdmi->switchdev.name = kzalloc(32, GFP_KERNEL);
		memset(hdmi->switchdev.name, 0, 32);
		sprintf(hdmi->switchdev.name, "hdmi%d", hdmi->id);
	}
	switch_dev_register(&(hdmi->switchdev));
	#endif
		
	return hdmi;
	
err_register_display:
	destroy_workqueue(hdmi->workqueue);	
err_create_wq:
	kfree(hdmi);
	return NULL;
}

void hdmi_unregister(struct hdmi *hdmi)
{
	if(hdmi) {
		flush_workqueue(hdmi->workqueue);
		destroy_workqueue(hdmi->workqueue);
		#ifdef CONFIG_SWITCH
		switch_dev_unregister(&(hdmi->switchdev));
		#endif
		hdmi_unregister_display_sysfs(hdmi);
		fb_destroy_modelist(&hdmi->edid.modelist);
		if(hdmi->edid.audio)
			kfree(hdmi->edid.audio);
		if(hdmi->edid.specs)
		{
			if(hdmi->edid.specs->modedb)
				kfree(hdmi->edid.specs->modedb);
			kfree(hdmi->edid.specs);
		}
		kfree(hdmi);
		hdmi = NULL;
	}
}
