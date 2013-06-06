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
#include "rk2928_hdmi.h"
#include "rk2928_hdmi_hw.h"

static struct rk2928_hdmi *rk2928_hdmi = NULL;

extern irqreturn_t hdmi_irq(int irq, void *priv);

struct hdmi* rk2928_hdmi_register_hdcp_callbacks(void (*hdcp_cb)(void),
					 void (*hdcp_irq_cb)(int status),
					 int (*hdcp_power_on_cb)(void),
					 void (*hdcp_power_off_cb)(void))
{
	struct hdmi *hdmi = rk2928_hdmi->hdmi;
	
	hdmi->ops->hdcp_cb = hdcp_cb;
	hdmi->ops->hdcp_irq_cb = hdcp_irq_cb;
	hdmi->ops->hdcp_power_on_cb = hdcp_power_on_cb;
	hdmi->ops->hdcp_power_off_cb = hdcp_power_off_cb;
	return hdmi;
}

#ifdef CONFIG_HAS_EARLYSUSPEND
static void rk2928_hdmi_early_suspend(struct early_suspend *h)
{
	struct rk2928_hdmi *rk2928_hdmi = container_of(h, struct rk2928_hdmi, early_suspend);
	struct hdmi *hdmi = rk2928_hdmi->hdmi;
	
	RK2928DBG("hdmi enter early suspend \n");
	
	rk30_mux_api_set(GPIO0A7_I2C3_SDA_HDMI_DDCSDA_NAME, GPIO0A_GPIO0A7);
	rk30_mux_api_set(GPIO0A6_I2C3_SCL_HDMI_DDCSCL_NAME, GPIO0A_GPIO0A6);
	
	hdmi_submit_work(hdmi, HDMI_SUSPEND_CTL, 0, NULL);
	return;
}

static void rk2928_hdmi_early_resume(struct early_suspend *h)
{
	struct rk2928_hdmi *rk2928_hdmi = container_of(h, struct rk2928_hdmi, early_suspend);
	struct hdmi *hdmi = rk2928_hdmi->hdmi;
	
	RK2928DBG("hdmi exit early resume\n");

	rk30_mux_api_set(GPIO0A7_I2C3_SDA_HDMI_DDCSDA_NAME, GPIO0A_HDMI_DDCSDA);
	rk30_mux_api_set(GPIO0A6_I2C3_SCL_HDMI_DDCSCL_NAME, GPIO0A_HDMI_DDCSCL);
	
	clk_enable(rk2928_hdmi->hclk);

	if(hdmi->ops->hdcp_power_on_cb)
		hdmi->ops->hdcp_power_on_cb();
	hdmi_submit_work(hdmi, HDMI_RESUME_CTL, 0, NULL);
}
#endif

static inline void hdmi_io_remap(void)
{
	// Remap HDMI IO Pin
	rk30_mux_api_set(GPIO0A7_I2C3_SDA_HDMI_DDCSDA_NAME, GPIO0A_HDMI_DDCSDA);
	rk30_mux_api_set(GPIO0A6_I2C3_SCL_HDMI_DDCSCL_NAME, GPIO0A_HDMI_DDCSCL);
	rk30_mux_api_set(GPIO0B7_HDMI_HOTPLUGIN_NAME, GPIO0B_HDMI_HOTPLUGIN);
}

static struct hdmi_property rk2928_hdmi_property = {
	.videosrc = DISPLAY_SOURCE_LCDC0,
	.display = DISPLAY_MAIN,
};

static struct hdmi_ops rk2928_hdmi_ops = {
	.enable = rk2928_hdmi_enable,
	.disable = rk2928_hdmi_disable,
	.getStatus = rk2928_hdmi_detect_hotplug,
	.remove = rk2928_hdmi_removed,
	.getEdid = rk2928_hdmi_read_edid,
	.setVideo = rk2928_hdmi_config_video,
	.setAudio = rk2928_hdmi_config_audio,
	.setMute = rk2928_hdmi_control_output,
};

static int __devinit rk2928_hdmi_probe (struct platform_device *pdev)
{
	int ret;
	struct resource *res;
	struct resource *mem;
	
	rk2928_hdmi = kmalloc(sizeof(struct rk2928_hdmi), GFP_KERNEL);
	if(!rk2928_hdmi)
	{
    	dev_err(&pdev->dev, ">>rk30 hdmi kmalloc fail!");
    	return -ENOMEM;
	}
	memset(rk2928_hdmi, 0, sizeof(struct hdmi));
	platform_set_drvdata(pdev, rk2928_hdmi);
	
	rk2928_hdmi->pwr_mode = NORMAL;
	
	/* get the IRQ */
	rk2928_hdmi->irq = platform_get_irq(pdev, 0);
	if(rk2928_hdmi->irq <= 0) {
		dev_err(&pdev->dev, "failed to get hdmi irq resource (%d).\n", rk2928_hdmi->irq);
		ret = -ENXIO;
		goto err0;
	}
	
	rk2928_hdmi->hclk = clk_get(NULL,"pclk_hdmi");
	if(IS_ERR(rk2928_hdmi->hclk))
	{
		dev_err(&pdev->dev, "Unable to get hdmi hclk\n");
		ret = -ENXIO;
		goto err0;
	}
	clk_enable(rk2928_hdmi->hclk);
	
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		dev_err(&pdev->dev, "Unable to get register resource\n");
		ret = -ENXIO;
		goto err0;
	}
	rk2928_hdmi->regbase_phy = res->start;
	rk2928_hdmi->regsize_phy = (res->end - res->start) + 1;
	mem = request_mem_region(res->start, (res->end - res->start) + 1, pdev->name);
	if (!mem)
	{
    	dev_err(&pdev->dev, "failed to request mem region for hdmi\n");
    	ret = -ENOENT;
    	goto err0;
	}
	
	rk2928_hdmi->regbase = (int)ioremap(res->start, (res->end - res->start) + 1);
	if (!rk2928_hdmi->regbase) {
		dev_err(&pdev->dev, "cannot ioremap registers\n");
		ret = -ENXIO;
		goto err1;
	}
	
	#ifdef CONFIG_HAS_EARLYSUSPEND
	rk2928_hdmi->early_suspend.suspend = rk2928_hdmi_early_suspend;
	rk2928_hdmi->early_suspend.resume = rk2928_hdmi_early_resume;
	rk2928_hdmi->early_suspend.level = EARLY_SUSPEND_LEVEL_DISABLE_FB - 10;
	register_early_suspend(&rk2928_hdmi->early_suspend);
	#endif
	
	rk2928_hdmi_property.name = (char*)pdev->name;
	rk2928_hdmi_property.priv = rk2928_hdmi;
	rk2928_hdmi->hdmi = hdmi_register(&rk2928_hdmi_property, &rk2928_hdmi_ops);
	if(rk2928_hdmi->hdmi == NULL) {
		dev_err(&pdev->dev, "register hdmi device failed\n");
		ret = -ENOMEM;
		goto err2;
	}
		
	rk2928_hdmi->hdmi->dev = &pdev->dev;
	rk2928_hdmi->hdmi->xscale = 95;
	rk2928_hdmi->hdmi->yscale = 95;
	
	rk2928_hdmi_reset(rk2928_hdmi);
	hdmi_io_remap();
		
	spin_lock_init(&rk2928_hdmi->irq_lock);
	
	/* request the IRQ */
	ret = request_irq(rk2928_hdmi->irq, hdmi_irq, 0, dev_name(&pdev->dev), rk2928_hdmi);
	if (ret)
	{
		dev_err(&pdev->dev, "rk30 hdmi request_irq failed (%d).\n", ret);
		goto err3;
	}

	dev_info(&pdev->dev, "rk30 hdmi probe sucess.\n");
	return 0;
	
err3:
	hdmi_unregister(rk2928_hdmi->hdmi);
err2:
	#ifdef CONFIG_HAS_EARLYSUSPEND
	unregister_early_suspend(&rk2928_hdmi->early_suspend);
	#endif
	iounmap((void*)rk2928_hdmi->regbase);
err1:
	release_mem_region(res->start,(res->end - res->start) + 1);
	clk_disable(rk2928_hdmi->hclk);
err0:
	kfree(rk2928_hdmi);
	rk2928_hdmi = NULL;
	dev_err(&pdev->dev, "rk30 hdmi probe error.\n");
	return ret;
}

static int __devexit rk2928_hdmi_remove(struct platform_device *pdev)
{
	
	printk(KERN_INFO "rk2928 hdmi removed.\n");
	return 0;
}

static void rk2928_hdmi_shutdown(struct platform_device *pdev)
{
	struct rk2928_hdmi *rk2928_hdmi = platform_get_drvdata(pdev);
	struct hdmi *hdmi = rk2928_hdmi->hdmi;
	if(hdmi) {
		#ifdef CONFIG_HAS_EARLYSUSPEND
		unregister_early_suspend(&rk2928_hdmi->early_suspend);
		#endif
		if(!hdmi->sleep && hdmi->enable)
			disable_irq(rk2928_hdmi->irq);
	}
	printk(KERN_INFO "rk2928 hdmi shut down.\n");
}

static struct platform_driver rk2928_hdmi_driver = {
	.probe		= rk2928_hdmi_probe,
	.remove		= __devexit_p(rk2928_hdmi_remove),
	.driver		= {
		.name	= "rk2928-hdmi",
		.owner	= THIS_MODULE,
	},
	.shutdown   = rk2928_hdmi_shutdown,
};

static int __init rk2928_hdmi_init(void)
{
    return platform_driver_register(&rk2928_hdmi_driver);
}

static void __exit rk2928_hdmi_exit(void)
{
    platform_driver_unregister(&rk2928_hdmi_driver);
}


//fs_initcall(rk2928_hdmi_init);
module_init(rk2928_hdmi_init);
module_exit(rk2928_hdmi_exit);
