#ifndef _SII902X_H
#define _SII902X_H

#include "../rk_hdmi.h"
#include "siHdmiTx_902x_TPI.h"

#ifndef INVALID_GPIO
#define INVALID_GPIO	-1
#endif

#define sii902x_SCL_RATE	100*1000

struct sii902x {
	int irq;
	int io_irq_pin;
	int io_pwr_pin;
	int io_rst_pin;
	struct i2c_client *client;
	struct hdmi *hdmi;
	struct work_struct	irq_work;
	struct delayed_work delay_work;
	struct workqueue_struct *workqueue;
};

extern void sii902x_irq_poll(struct hdmi *hdmi);
extern int sii902x_sys_init(void);
extern int sii902x_detect_hotplug(struct hdmi *hdmi);
extern int sii902x_insert(struct hdmi *hdmi);
extern int sii902x_remove(struct hdmi *hdmi);
extern int sii902x_read_edid(struct hdmi *hdmi, int block, unsigned char *buff);
extern int sii902x_config_video(struct hdmi *hdmi, struct hdmi_video *vpara);
extern int sii902x_config_audio(struct hdmi *hdmi, struct hdmi_audio *audio);
extern int sii902x_control_output(struct hdmi *hdmi, int enable);
extern int sii902x_config_vsi(struct hdmi *hdmi, unsigned char vic_3d, unsigned char format);
extern int sii902x_config_cec(struct hdmi *hdmi);
#endif