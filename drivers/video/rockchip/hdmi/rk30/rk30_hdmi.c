#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/init.h>
#include <linux/dma-mapping.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/clk.h>

#include <mach/board.h>
#include <mach/io.h>
#include <mach/gpio.h>
#include <mach/iomux.h>

#include <linux/rk_fb.h>

#include "rk30_hdmi.h"
#include "rk30_hdmi_hw.h"
#ifdef CONFIG_RK_HDMI_GPIO_CEC
#include "../softcec/gpio-cec.h"
#endif

static struct rk30_hdmi *rk30_hdmi;

extern irqreturn_t hdmi_irq(int irq, void *priv);

struct hdmi* rk30_hdmi_register_hdcp_callbacks( 
					 void (*hdcp_cb)(void),
					 void (*hdcp_irq_cb)(int status),
					 int (*hdcp_power_on_cb)(void),
					 void (*hdcp_power_off_cb)(void))
{
	struct hdmi *hdmi = rk30_hdmi->hdmi;
	
	hdmi->ops->hdcp_cb = hdcp_cb;
	hdmi->ops->hdcp_irq_cb = hdcp_irq_cb;
	hdmi->ops->hdcp_power_on_cb = hdcp_power_on_cb;
	hdmi->ops->hdcp_power_off_cb = hdcp_power_off_cb;
	
	return hdmi;
}

#ifdef CONFIG_HAS_EARLYSUSPEND
static void rk30_hdmi_early_suspend(struct early_suspend *h)
{
	struct rk30_hdmi *rk30_hdmi = container_of(h, struct rk30_hdmi, early_suspend);
	struct hdmi *hdmi = rk30_hdmi->hdmi;
	
	RK30DBG("hdmi enter early suspend pwr %d\n", rk30_hdmi->pwr_mode);
	// When HDMI 1.1V and 2.5V power off, DDC channel will be pull down, current is produced
	// from VCC_IO which is pull up outside soc. We need to switch DDC IO to GPIO.
	rk30_mux_api_set(GPIO0A2_HDMII2CSDA_NAME, GPIO0A_GPIO0A2);
	rk30_mux_api_set(GPIO0A1_HDMII2CSCL_NAME, GPIO0A_GPIO0A1);
	hdmi_submit_work(hdmi, HDMI_SUSPEND_CTL, 0, NULL);
	return;
}

static void rk30_hdmi_early_resume(struct early_suspend *h)
{
	struct rk30_hdmi *rk30_hdmi = container_of(h, struct rk30_hdmi, early_suspend);
	struct hdmi *hdmi = rk30_hdmi->hdmi;
	
	RK30DBG("hdmi exit early resume\n");

	rk30_mux_api_set(GPIO0A2_HDMII2CSDA_NAME, GPIO0A_HDMI_I2C_SDA);
	rk30_mux_api_set(GPIO0A1_HDMII2CSCL_NAME, GPIO0A_HDMI_I2C_SCL);
	
	clk_enable(rk30_hdmi->hclk);
	// internal hclk = hdmi_hclk/20
	HDMIWrReg(0x800, HDMI_INTERANL_CLK_DIV);
	if(hdmi->ops->hdcp_power_on_cb)
		hdmi->ops->hdcp_power_on_cb();		
	hdmi_submit_work(hdmi, HDMI_RESUME_CTL, 0, NULL);
	
	return;
}
#endif

static inline void hdmi_io_remap(void)
{
	unsigned int value;
	
	// Remap HDMI IO Pin
	rk30_mux_api_set(GPIO0A2_HDMII2CSDA_NAME, GPIO0A_HDMI_I2C_SDA);
	rk30_mux_api_set(GPIO0A1_HDMII2CSCL_NAME, GPIO0A_HDMI_I2C_SCL);
	rk30_mux_api_set(GPIO0A0_HDMIHOTPLUGIN_NAME, GPIO0A_HDMI_HOT_PLUG_IN);
		
	// Select LCDC0 as video source and enabled.
	value = (HDMI_SOURCE_DEFAULT << 14) | (1 << 30);
	writel(value, GRF_SOC_CON0 + RK30_GRF_BASE);
	
	// internal hclk = hdmi_hclk/20
	HDMIWrReg(0x800, HDMI_INTERANL_CLK_DIV);
}
#ifdef CONFIG_RK_HDMI_GPIO_CEC
int rk30_hdmi_config_cec(struct hdmi *hdmi)
{
	GPIO_CecSetDevicePA(hdmi->edid.cecaddress);
	GPIO_CecEnumerate();
	return HDMI_ERROR_SUCESS;
}
#endif
static struct hdmi_property rk30_hdmi_property = {
	.videosrc = HDMI_SOURCE_DEFAULT,
	.display = DISPLAY_MAIN,
};

static struct hdmi_ops rk30_hdmi_ops = {
	.enable = rk30_hdmi_enable,
	.disable = rk30_hdmi_disable,
	.getStatus = rk30_hdmi_detect_hotplug,
	.remove = rk30_hdmi_removed,
	.getEdid = rk30_hdmi_read_edid,
	.setVideo = rk30_hdmi_config_video,
	.setAudio = rk30_hdmi_config_audio,
	.setMute = rk30_hdmi_control_output,
	.setVSI = rk30_hdmi_config_vsi,
	#ifdef CONFIG_RK_HDMI_GPIO_CEC
	.setCEC = rk30_hdmi_config_cec,
	#endif
};
extern int GPIO_CecInit(int gpio);
static int __devinit rk30_hdmi_probe (struct platform_device *pdev)
{
	int ret;
	struct resource *res;
	struct resource *mem;
	
	rk30_hdmi = kmalloc(sizeof(struct rk30_hdmi), GFP_KERNEL);
	if(!rk30_hdmi)
	{
    	dev_err(&pdev->dev, ">>rk30 hdmi kmalloc fail!");
    	return -ENOMEM;
	}
	memset(rk30_hdmi, 0, sizeof(struct hdmi));
	platform_set_drvdata(pdev, rk30_hdmi);
	
	rk30_hdmi->pwr_mode = PWR_SAVE_MODE_A;
	
	/* get the IRQ */
	rk30_hdmi->irq = platform_get_irq(pdev, 0);
	if(rk30_hdmi->irq <= 0) {
		dev_err(&pdev->dev, "failed to get hdmi irq resource (%d).\n", rk30_hdmi->irq);
		ret = -ENXIO;
		goto err0;
	}
	
	rk30_hdmi->hclk = clk_get(NULL,"hclk_hdmi");
	if(IS_ERR(rk30_hdmi->hclk))
	{
		dev_err(&pdev->dev, "Unable to get hdmi hclk\n");
		ret = -ENXIO;
		goto err0;
	}
	clk_enable(rk30_hdmi->hclk);
	
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		dev_err(&pdev->dev, "Unable to get register resource\n");
		ret = -ENXIO;
		goto err0;
	}
	rk30_hdmi->regbase_phy = res->start;
	rk30_hdmi->regsize_phy = (res->end - res->start) + 1;
	mem = request_mem_region(res->start, (res->end - res->start) + 1, pdev->name);
	if (!mem)
	{
    	dev_err(&pdev->dev, "failed to request mem region for hdmi\n");
    	ret = -ENOENT;
    	goto err0;
	}
	
	rk30_hdmi->regbase = (int)ioremap(res->start, (res->end - res->start) + 1);
	if (!rk30_hdmi->regbase) {
		dev_err(&pdev->dev, "cannot ioremap registers\n");
		ret = -ENXIO;
		goto err1;
	}
	
	#ifdef CONFIG_HAS_EARLYSUSPEND
	rk30_hdmi->early_suspend.suspend = rk30_hdmi_early_suspend;
	rk30_hdmi->early_suspend.resume = rk30_hdmi_early_resume;
	rk30_hdmi->early_suspend.level = EARLY_SUSPEND_LEVEL_DISABLE_FB - 10;
	register_early_suspend(&rk30_hdmi->early_suspend);
	#endif
	
	rk30_hdmi_property.name = (char*)pdev->name;
	rk30_hdmi_property.priv = rk30_hdmi;
	rk30_hdmi->hdmi = hdmi_register(&rk30_hdmi_property, &rk30_hdmi_ops);
	if(rk30_hdmi->hdmi == NULL) {
		dev_err(&pdev->dev, "register hdmi device failed\n");
		ret = -ENOMEM;
		goto err2;
	}
		
	rk30_hdmi->hdmi->dev = &pdev->dev;
   // ADB: disable HDMI scaling by default
	rk30_hdmi->hdmi->xscale = 100;
	rk30_hdmi->hdmi->yscale = 100;
	
	hdmi_io_remap();
		
	spin_lock_init(&rk30_hdmi->irq_lock);
	
	/* request the IRQ */
	ret = request_irq(rk30_hdmi->irq, hdmi_irq, 0, dev_name(&pdev->dev), rk30_hdmi);
	if (ret)
	{
		dev_err(&pdev->dev, "rk30 hdmi request_irq failed (%d).\n", ret);
		goto err3;
	}
	
	#ifdef CONFIG_RK_HDMI_GPIO_CEC
	GPIO_CecInit(RK30_PIN2_PA0);
	#endif
	dev_info(&pdev->dev, "rk30 hdmi probe sucess.\n");
	return 0;
	
err3:
	hdmi_unregister(rk30_hdmi->hdmi);
err2:
	#ifdef CONFIG_HAS_EARLYSUSPEND
	unregister_early_suspend(&rk30_hdmi->early_suspend);
	#endif
	iounmap((void*)rk30_hdmi->regbase);
err1:
	release_mem_region(res->start,(res->end - res->start) + 1);
	clk_disable(rk30_hdmi->hclk);
err0:
	kfree(rk30_hdmi);
	rk30_hdmi = NULL;
	dev_err(&pdev->dev, "rk30 hdmi probe error.\n");
	return ret;
}

static int __devexit rk30_hdmi_remove(struct platform_device *pdev)
{
	struct rk30_hdmi *rk30_hdmi = platform_get_drvdata(pdev);
	struct hdmi *hdmi = rk30_hdmi->hdmi;
	
	printk(KERN_INFO "rk30 hdmi removed.\n");
	return 0;
}

static void rk30_hdmi_shutdown(struct platform_device *pdev)
{
	struct rk30_hdmi *rk30_hdmi = platform_get_drvdata(pdev);
	struct hdmi *hdmi = rk30_hdmi->hdmi;
	if(hdmi) {
		#ifdef CONFIG_HAS_EARLYSUSPEND
		unregister_early_suspend(&rk30_hdmi->early_suspend);
		#endif
		if(!hdmi->sleep && hdmi->enable)
			disable_irq(rk30_hdmi->irq);
	}
	printk(KERN_INFO "rk30 hdmi shut down.\n");
}

static struct platform_driver rk30_hdmi_driver = {
	.probe		= rk30_hdmi_probe,
	.remove		= __devexit_p(rk30_hdmi_remove),
	.driver		= {
		.name	= "rk30-hdmi",
		.owner	= THIS_MODULE,
	},
	.shutdown   = rk30_hdmi_shutdown,
};

static int __init rk30_hdmi_init(void)
{
    return platform_driver_register(&rk30_hdmi_driver);
}

static void __exit rk30_hdmi_exit(void)
{
    platform_driver_unregister(&rk30_hdmi_driver);
}


//fs_initcall(rk30_hdmi_init);
module_init(rk30_hdmi_init);
module_exit(rk30_hdmi_exit);
