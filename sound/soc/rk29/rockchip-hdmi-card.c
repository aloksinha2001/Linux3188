//$_FOR_ROCKCHIP_RBOX_$
/*
 * rockchip-hdmi-card.c
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

#include <linux/delay.h>
#include <sound/pcm.h>
#include <sound/soc.h>

#include "rk29_pcm.h"
#include "rk29_i2s.h"

#ifdef HDMI_DEBUG
#define DBG(format, ...) \
		printk(KERN_INFO "HDMI Card: " format "\n", ## __VA_ARGS__)
#else
#define DBG(format, ...)
#endif

#define DRV_NAME "rockchip-hdmi-audio"

static int rockchip_hdmi_dai_hw_params(struct snd_pcm_substream *substream,
		struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	unsigned int pll_out = 0; 
	int div_bclk,div_mclk;
	int ret;
	struct clk	*general_pll;
	  
    DBG("Enter::%s----%d\n",__FUNCTION__,__LINE__);    
    {
        /* set cpu DAI configuration */
         #if defined (CONFIG_SND_RK29_CODEC_SOC_SLAVE) 
			DBG("Set cpu_dai master\n");    
            ret = snd_soc_dai_set_fmt(cpu_dai, SND_SOC_DAIFMT_I2S |
                            SND_SOC_DAIFMT_NB_NF | SND_SOC_DAIFMT_CBM_CFM);
        #endif	
        #if defined (CONFIG_SND_RK29_CODEC_SOC_MASTER)  
		    ret = snd_soc_dai_set_fmt(cpu_dai, SND_SOC_DAIFMT_I2S |
                            SND_SOC_DAIFMT_NB_NF | SND_SOC_DAIFMT_CBS_CFS);	
			DBG("Set cpu_dai slave\n");   				
        #endif		
        if (ret < 0)
            return ret;
    }
	
    switch(params_rate(params)) {
        case 8000:
        case 16000:
        case 24000:
        case 32000:
        case 48000:
        case 96000:
            pll_out = 12288000;
            break;
        case 11025:
        case 22050:
        case 44100:
        case 88200:
            pll_out = 11289600;
            break;
        case 176400:
			pll_out = 11289600*2;
        	break;
        case 192000:
        	pll_out = 12288000*2;
        	break;
        default:
            DBG("Enter:%s, %d, Error rate=%d\n",__FUNCTION__,__LINE__,params_rate(params));
            return -EINVAL;
            break;
	}
	DBG("Enter:%s, %d, rate=%d\n",__FUNCTION__,__LINE__,params_rate(params));
	#if defined (CONFIG_SND_RK29_CODEC_SOC_MASTER) 	
		snd_soc_dai_set_sysclk(cpu_dai, 0, pll_out, 0);
	#endif	
	
	#if defined (CONFIG_SND_RK29_CODEC_SOC_SLAVE)

		div_bclk = 63;
		div_mclk = pll_out/(params_rate(params)*64) - 1;
		
//		DBG("func is%s,gpll=%ld,pll_out=%ld,div_mclk=%ld\n",
//				__FUNCTION__,clk_get_rate(general_pll),pll_out,div_mclk);
		snd_soc_dai_set_sysclk(cpu_dai, 0, pll_out, 0);
		snd_soc_dai_set_clkdiv(cpu_dai, ROCKCHIP_DIV_BCLK,div_bclk);
		snd_soc_dai_set_clkdiv(cpu_dai, ROCKCHIP_DIV_MCLK, div_mclk);
//		DBG("Enter:%s, %d, LRCK=%d\n",__FUNCTION__,__LINE__,(pll_out/4)/params_rate(params));		
	#endif
	return 0;
}

static struct snd_soc_ops rockchip_hdmi_dai_ops = {
	.hw_params = rockchip_hdmi_dai_hw_params,
};

static struct snd_soc_dai_link rockchip_hdmi_dai = {
	.name = "HDMI",
	.stream_name = "HDMI",
#if defined(CONFIG_SND_RK29_SOC_I2S_8CH)
	.cpu_dai_name = "rk29_i2s.0",
#elif defined(CONFIG_SND_RK29_SOC_I2S_2CH)
	.cpu_dai_name = "rk29_i2s.1",
#else	
	.cpu_dai_name = "rk29_i2s.2",
#endif
	.platform_name = "rockchip-audio",
	.codec_name = "rockchip-hdmi-codec",
	.codec_dai_name = "hdmi-audio-codec",
	.ops = &rockchip_hdmi_dai_ops,
};

static struct snd_soc_card snd_soc_rockchip_hdmi = {
	.name = "ROCKCHIP HDMI",
	.dai_link = &rockchip_hdmi_dai,
	.num_links = 1,
};

static __devinit int rockchip_hdmi_probe(struct platform_device *pdev)
{
	struct snd_soc_card *card = &snd_soc_rockchip_hdmi;
	int ret;

	DBG("%s", __FUNCTION__);

	card->dev = &pdev->dev;

	ret = snd_soc_register_card(card);
	if (ret) {
		dev_err(&pdev->dev, "snd_soc_register_card failed (%d)\n", ret);
		card->dev = NULL;
		return ret;
	}
	return 0;
}

static int __devexit rockchip_hdmi_remove(struct platform_device *pdev)
{
	struct snd_soc_card *card = platform_get_drvdata(pdev);

	snd_soc_unregister_card(card);
	card->dev = NULL;
	return 0;
}

static struct platform_driver rockchip_hdmi_driver = {
	.driver = {
		.name = "rockchip-hdmi-audio",
		.owner = THIS_MODULE,
	},
	.probe = rockchip_hdmi_probe,
	.remove = __devexit_p(rockchip_hdmi_remove),
};

static int __init rockchip_hdmi_init(void)
{
	return platform_driver_register(&rockchip_hdmi_driver);
}
module_init(rockchip_hdmi_init);

static void __exit rockchip_hdmi_exit(void)
{
	platform_driver_unregister(&rockchip_hdmi_driver);
}
module_exit(rockchip_hdmi_exit);

MODULE_DESCRIPTION("Rockchip HDMI machine ASoC driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:" DRV_NAME);
