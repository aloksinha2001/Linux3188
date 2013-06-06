//$_FOR_ROCKCHIP_RBOX_$
/*
 * ALSA SoC HMDI codec driver
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 *
 */

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/pm.h>
#include <linux/gpio.h>
#include <linux/platform_device.h>
#include <linux/slab.h>

#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/initval.h>
#include <sound/tlv.h>

static int hdmi_audio_hw_params(struct snd_pcm_substream *substream,
				    struct snd_pcm_hw_params *params,
				    struct snd_soc_dai *dai)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_codec *codec = rtd->codec;
//	struct hdmi_codec_data *priv = snd_soc_codec_get_drvdata(codec);

	return 0;
}

static int hdmi_audio_trigger(struct snd_pcm_substream *substream, int cmd,
				struct snd_soc_dai *dai)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_codec *codec = rtd->codec;
	struct hdmi_codec_data *priv = snd_soc_codec_get_drvdata(codec);
	int err = 0;

	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_RESUME:
	case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:

		break;
	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_SUSPEND:
	case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
		
		break;
	default:
		err = -EINVAL;
	}
	return err;
}

static int hdmi_audio_startup(struct snd_pcm_substream *substream,
				  struct snd_soc_dai *dai)
{
	return 0;
}

static int hdmi_probe(struct snd_soc_codec *codec)
{
	struct platform_device *pdev = to_platform_device(codec->dev);
	int ret = 0;

res_err:
	return ret;

}

static int hdmi_remove(struct snd_soc_codec *codec)
{
//	struct hdmi_codec_data *priv = snd_soc_codec_get_drvdata(codec);

	return 0;
}


static struct snd_soc_codec_driver hdmi_audio_codec_drv = {
	.probe = hdmi_probe,
	.remove = hdmi_remove,
};

static struct snd_soc_dai_ops hdmi_audio_codec_ops = {
	.hw_params = hdmi_audio_hw_params,
	.trigger = hdmi_audio_trigger,
	.startup = hdmi_audio_startup,
};

static struct snd_soc_dai_driver hdmi_codec_dai_drv = {
		.name = "hdmi-audio-codec",
		.playback = {
			.channels_min = 2,
			.channels_max = 8,
			.rates = SNDRV_PCM_RATE_32000 |	SNDRV_PCM_RATE_44100 | SNDRV_PCM_RATE_48000 |
					 SNDRV_PCM_RATE_88200 | SNDRV_PCM_RATE_96000 | SNDRV_PCM_RATE_176400| 
					 SNDRV_PCM_RATE_192000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S20_3LE | 
				SNDRV_PCM_FMTBIT_S24_LE,
		},
		.ops = &hdmi_audio_codec_ops,
};

static __devinit int hdmi_codec_probe(struct platform_device *pdev)
{
	int r;

	/* Register ASoC codec DAI */
	r = snd_soc_register_codec(&pdev->dev, &hdmi_audio_codec_drv,
					&hdmi_codec_dai_drv, 1);
	if (r) {
		dev_err(&pdev->dev, "can't register ASoC HDMI audio codec\n");
		return r;
	}

	return 0;
}

static int __devexit hdmi_codec_remove(struct platform_device *pdev)
{
	snd_soc_unregister_codec(&pdev->dev);
	return 0;
}


static struct platform_driver hdmi_codec_driver = {
	.probe          = hdmi_codec_probe,
	.remove         = __devexit_p(hdmi_codec_remove),
	.driver         = {
		.name   = "rockchip-hdmi-codec",
		.owner  = THIS_MODULE,
	},
};


static int __init hdmi_codec_init(void)
{
	return platform_driver_register(&hdmi_codec_driver);
}
module_init(hdmi_codec_init);

static void __exit hdmi_codec_exit(void)
{
	platform_driver_unregister(&hdmi_codec_driver);
}
module_exit(hdmi_codec_exit);

MODULE_DESCRIPTION("ASoC HDMI codec driver");
MODULE_LICENSE("GPL");
