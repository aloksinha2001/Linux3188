/*$_FOR_ROCKCHIP_RBOX_$*/
/*$_rbox_$_modify_$_huangzhibao for spdif output*/

/*
 * rk29_spdif.c.c -- RK1000 ALSA SoC audio driver
 *
 * Copyright (C) 2009 rockchip lhh
 *
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/pm.h>
#include <linux/platform_device.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <sound/initval.h>
#include <mach/gpio.h>
#include <mach/iomux.h>
#include <linux/dma-mapping.h>
#include <asm/dma.h>
#include <mach/board.h>
#include "rk29_pcm.h"
#include <asm/dma.h>
#include <linux/dma-mapping.h>
#include "rkxx_spdif.h"
#include <linux/clk.h>
#include <mach/cru.h>

#if defined (CONFIG_ARCH_RK29)
#include <mach/rk29-dma-pl330.h>
#endif

#if defined (CONFIG_ARCH_RK30)
#include <mach/dma-pl330.h>
#endif


/*
 * Debug
 */
#if 1
#define	DBG(x...)	printk(KERN_INFO x)
#else
#define	DBG(x...)
#endif

#define err(format, arg...) \
	printk(KERN_ERR "SPDIF" ": " format "\n" , ## arg)
#define info(format, arg...) \
	printk(KERN_INFO "SPDIF" ": " format "\n" , ## arg)
	

struct rk29xx_spdif rk29_spdif;
struct RK29_SPDIF_REG spdif_reg;


static struct rk29_dma_client rk29_spdif_dma_client = {
	.name = "rk29 spdif"
};

#if 0
static u32 spdif_clk_enter()
{
  u32 clk = cru_readl(CRU_CLKSEL5_CON);
  cru_writel(0x1ffff, CRU_CLKSEL5_CON);
  mdelay(1);
  return clk;
}

static void spdif_clk_exit(u32 clk)
{
  mdelay(1);	
  cru_writel(clk, CRU_CLKSEL5_CON);
  mdelay(1);
}
#else
static u32 spdif_clk_enter()
{
    struct RK29_SPDIF_REG *reg = &spdif_reg;
    int temp;
    
    temp = readl(reg->cfgr);
    temp |= SPDIF_RESET;
    writel(temp,reg->cfgr);
    mdelay(2);
    return 0;
}

static void spdif_clk_exit(u32 clk)
{
    struct RK29_SPDIF_REG *reg = &spdif_reg;
    int temp;
    
    mdelay(1);
    temp = readl(reg->cfgr);
    temp &= ~SPDIF_RESET;
    DBG("reg->cfgr = 0x%x\n",temp);
    writel(temp,reg->cfgr);
    mdelay(1);
}
#endif


#if 0
static irqreturn_t rk29_spdif_irq(int irq, void *dev_id)
{
    DBG("******************rk29xx_spdif_irq\n\n\n");

    return IRQ_HANDLED;
}
#endif

#if 1
void rk29_spdif_ctrl(int on_off, bool stopSPDIF)
{
    struct RK29_SPDIF_REG *reg = &spdif_reg;
    int temp;
    u32 clk;

    if (on_off){
        spdif_clk_enter();
        writel(SPDIF_TRANFER_DISABLE,reg->xfer);
        spdif_clk_exit(clk);
        //temp = readl(reg->cfgr);
        //temp |= SPDIF_RESET;
       //writel(temp,reg->cfgr);
        
        //mdelay(5);
        
        //temp = readl(reg->cfgr);
        //temp &= ~SPDIF_RESET;
        //DBG("reg->cfgr = 0x%x\n",temp);
        //writel(temp,reg->cfgr);
        
        temp = readl(reg->dmacr);
        temp |= SPDIF_DMA_CTRL_ENABLE;
        DBG("reg->dmacr = 0x%x\n",temp);
        writel(temp, reg->dmacr);
        
        spdif_clk_enter();
        writel(SPDIF_TRANFER_ENABLE,reg->xfer);// modify SPDIF_XFER to enable SPDIF
        spdif_clk_exit(clk);
    }else{
        spdif_clk_enter();
        writel(SPDIF_TRANFER_DISABLE,reg->xfer);// modify SPDIF_XFER to enable SPDIF
        spdif_clk_exit(clk);
        mdelay(2);
        
        temp = readl(reg->dmacr);
        temp &= ~SPDIF_DMA_CTRL_ENABLE;
        DBG("disable reg->dmacr = 0x%x\n",temp);
        writel(temp, reg->dmacr);
    }
        
}
#else
void rk29_spdif_ctrl(int on_off, bool stopSPDIF)
{
    struct RK29_SPDIF_REG *reg = &spdif_reg;
    u32 opr,xfer;
    u32 clk;

    opr  = readl(&(reg->dmacr));
    xfer = readl(&(reg->xfer));

    if (on_off){
        if ((xfer&SPDIF_TRANFER_ENABLE)==0)
        {
            clk = spdif_clk_enter();

            //if start tx & rx clk, need reset spdif
            xfer |= SPDIF_TRANFER_ENABLE;
            writel(xfer, (reg->xfer));
            spdif_clk_exit(clk);
            
            DBG("SPDIF_TRANFER_ENABLE\n");
        }

        if ((opr & SPDIF_DMA_CTRL_ENABLE) == 0)
        {
            opr  |= SPDIF_DMA_CTRL_ENABLE;
            writel(opr, (reg->dmacr));  
            
            DBG("SPDIF_DMA_CTRL_ENABLE\n");       
        }
    }else{
        opr  &= ~SPDIF_DMA_CTRL_ENABLE;        
        writel(opr, (reg->dmacr)); 
        
        DBG("SPDIF_DMA_CTRL_DISABLE\n");  
        
        if(stopSPDIF)	
        {
            clk = spdif_clk_enter();
            xfer &= ~SPDIF_TRANFER_ENABLE;
            writel(xfer, (reg->xfer));
            spdif_clk_exit(clk);
            
            DBG("SPDIF_TRANFER_DISABLE\n");
        }
    }   
}
#endif

static int __devinit rk29_spdif_probe (struct platform_device *pdev)
{
    int ret = 0;
    struct resource *res = NULL,*dma=NULL;
    void __iomem *reg_vir_base;
    struct rk29xx_spdif *dws = &rk29_spdif;
    struct RK29_SPDIF_REG *reg = &spdif_reg;
    struct clk *general_pll;
    unsigned long spdif_mclk;
    int	   irq; 
    int temp;
    int i;

    
    DBG("enter rk29_spdif_probe\n");
#if defined  (CONFIG_ARCH_RK29)
    rk29_mux_api_set(GPIO4A7_SPDIFTX_NAME, GPIO4L_SPDIF_TX);
#endif

#if defined (CONFIG_ARCH_RK30)    
    rk30_mux_api_set(GPIO1B2_SPDIFTX_NAME, GPIO1B_SPDIF_TX);
#endif

#if defined (CONFIG_ARCH_RK3188)    
    iomux_set(SPDIF_TX);
#endif

    /* get spdif clk */
   dws->spdif_clk= clk_get(&pdev->dev, "spdif");

	if (IS_ERR(dws->spdif_clk)) {
		err("failed to get spdif clk\n");
		ret = PTR_ERR(dws->spdif_clk);
		goto err0;
	}
	clk_enable(dws->spdif_clk);
	clk_set_rate(dws->spdif_clk, 11289600);
	
//#if defined (CONFIG_ARCH_RK30)	
	dws->spdif_hclk= clk_get(&pdev->dev, "hclk_spdif");
	if (IS_ERR(dws->spdif_hclk)) {
		err("failed to get spdif clk\n");
		ret = PTR_ERR(dws->spdif_hclk);
		goto err1;
	}
	clk_enable(dws->spdif_hclk);
	clk_set_rate(dws->spdif_hclk, 11289600);
//#endif  

     /* get virtual basic address of spdif register */
    res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "spdif_base");
    if (res == NULL){
        dev_err(&pdev->dev, "failed to get memory registers\n");
        ret = -ENOENT;
		goto err1;
    }
      
    if (!request_mem_region(res->start, resource_size(res),"rk29_spdif")) {
        err("Unable to request register region\n");
        return -EBUSY;
    }
    reg_vir_base = ioremap(res->start, res->end - res->start + 1);

    if (!reg_vir_base){
        release_mem_region(res->start, res->end - res->start + 1);
        ret = -EBUSY;
        goto err2;
    }

    reg->cfgr  = reg_vir_base;
    reg->sdblr = reg_vir_base + 0x4;
    reg->dmacr = reg_vir_base + 0x8;
    reg->intcr = reg_vir_base + 0xc;
    reg->intsr = reg_vir_base + 0x10;
    reg->xfer  = reg_vir_base + 0x18;
    reg->smpdr = reg_vir_base + 0x20;
    reg->vldfr = reg_vir_base + 0x60;
    reg->usrdr = reg_vir_base + 0x90;
    reg->chnsr = reg_vir_base + 0xC0;

    dma = platform_get_resource_byname(pdev, IORESOURCE_DMA, "spdif_dma");
    if (dma == NULL){
        err("rk29 spdif failed to get dma channel\n");
        ret = -ENOENT;
        goto err3;
    }

    dws->dma_ch   = dma->start;
    dws->dma_size = 4;
    dws->client   = &rk29_spdif_dma_client;
    dws->dma_addr = res->start+0x20;

    writel(SPDIF_TRANFER_DISABLE,reg->xfer);
    
    writel(SPDIF_RESET,reg->cfgr);
    
    mdelay(1);
    
    temp =      SPDIF_VALID_DATA_WIDTH_16BIT    //valid data width, 0:16bit,1:20bit,2:24bit
                |SPDIF_HALFWORD_TX_ENABLE       //halfword word transform enable, 0:disable
                |SPDIF_JUSTIFIED_RIGHT          //apb valid audio data justified,0:right
                |SPDIF_VALID_FLAG_DISABLE        //valid flag enable
                |SPDIF_USER_DATA_DISABLE         //user data enable
                |SPDIF_CHANNEL_STATUS_DISABLE   //channel status enable
                |SPDIF_NOT_RESET                //write 1 to reset mclk domain logic
                |(0x01<<16)                     //Fmclk/Fsdo
            ;

    writel(temp,reg->cfgr);
    
    temp = SPDIF_DMA_CTRL_DISABLE|0x10;         //disable spdif dma
    writel(temp, reg->dmacr);
    
    writel(SPDIF_INT_CTRL_DISABLE, reg->intcr);//disable spdif int
#if 0   
    chnsr_byte1= (0x0)|(0x0<<1)|(0x0<<2)|(0x0<<3)|(0x00<<6);//consumer|pcm|copyright?|pre-emphasis|(0x00<<6);
    chnsr_byte2= (0x0);//category code general mode??
    chnsr_byte3= (0x0)|(0x0<<4)|(0x0<<6);//
    chnsr_byte4= (48);//khz;clock acurracy 
    chnsr_byte5= (0x1)|(0x001<<1);//24 bit;  

    writel((chnsr_byte2<<24)|(chnsr_byte1<<16)|(chnsr_byte2<<8)|(chnsr_byte1),reg->chnsr);
    writel((chnsr_byte4<<24)|(chnsr_byte3<<16)|(chnsr_byte4<<8)|(chnsr_byte3),reg->chnsr+4);
    writel((chnsr_byte5<<16)|(chnsr_byte5),reg->chnsr+8);
    writel(0,reg->chnsr+0xc);
    writel(0,reg->chnsr+0x10);
    writel(0,reg->chnsr+0x14);
    writel(0,reg->chnsr+0x18);
    writel(0,reg->chnsr+0x1C);
    writel(0,reg->chnsr+0x20);
    writel(0,reg->chnsr+0x24);
    writel(0,reg->chnsr+0x28);
    writel(0,reg->chnsr+0x2C);
#endif   
    writel(SPDIF_TRANFER_DISABLE,reg->xfer);// modify SPDIF_XFER to enable SPDIF
    rk29_spdif_ctrl(0, 1);
    DBG("rk29_spdif_probe ok!!\n");
    
    return 0;
err3:
err2:
	 // clk_disable(dws->spdif_hclk);
err1:
    clk_disable(dws->spdif_clk);
err0:   
    
    return ret;
}


static int rk29_spdif_remove(struct platform_device *pdev)
{
    struct rk29xx_spdif *dws = &rk29_spdif;
    struct RK29_SPDIF_REG *reg = &spdif_reg;

    iounmap(reg->cfgr);
    clk_disable(dws->spdif_clk);

    return 0;
}


static struct platform_driver rk29_spdif_driver = {
	.probe		= rk29_spdif_probe,
	.remove		= rk29_spdif_remove,
	.driver		= {
		.name	= "rk29-spdif",
		.owner	= THIS_MODULE,
	},
};


static int __init rk29_spdif_init(void)
{
    DBG("rk29_spdif_init\n");
    return platform_driver_register(&rk29_spdif_driver);
}

static void __exit rk29_spdif_exit(void)
{
    DBG("rk29_spdif_exit\n");
    platform_driver_unregister(&rk29_spdif_driver);
}


late_initcall(rk29_spdif_init);
module_exit(rk29_spdif_exit);


MODULE_AUTHOR("  hzb@rock-chips.com");
MODULE_DESCRIPTION("Driver for rk29 spdif device");
MODULE_LICENSE("GPL");


