
#ifndef _RK29_SPDIF_H_
#define _RK29_SPDIF_H_

#include <mach/hardware.h>

#define SPDIF_VALID_DATA_WIDTH_16BIT    (0)
#define SPDIF_VALID_DATA_WIDTH_20BIT    (1)
#define SPDIF_VALID_DATA_WIDTH_24BIT    (2)

#define SPDIF_HALFWORD_TX_ENABLE        (0x01<<2)
#define SPDIF_HALFWORD_TX_DISABLE       (0x00<<2)

#define SPDIF_JUSTIFIED_RIGHT           (0x00<<3)
#define SPDIF_JUSTIFIED_LEFT            (0x01<<3)

#define SPDIF_VALID_FLAG_ENABLE         (0x00<<4)
#define SPDIF_VALID_FLAG_DISABLE        (0x01<<4)

#define SPDIF_USER_DATA_ENABLE          (0x00<<5)
#define SPDIF_USER_DATA_DISABLE         (0x01<<5)

#define SPDIF_CHANNEL_STATUS_ENABLE     (0x00<<6)
#define SPDIF_CHANNEL_STATUS_DISABLE    (0x01<<6)

#define SPDIF_RESET                     (0x01<<7)
#define SPDIF_NOT_RESET                 (0x00<<7)

#define SPDIF_DMA_CTRL_ENABLE           (0x01<<5)
#define SPDIF_DMA_CTRL_DISABLE          (0x00<<5)

#define SPDIF_TRANFER_ENABLE            (0x01)
#define SPDIF_TRANFER_DISABLE           (0x00)

#define SPDIF_INT_CTRL_ENABLE           (0x01)
#define SPDIF_INT_CTRL_DISABLE          (0x00)




struct rk29xx_spdif {
	void __iomem		*regs;
	unsigned long		paddr;
	int			irq;
	u32         irq_polarity;

	dma_addr_t		dma_addr;
    unsigned int dma_ch;
    unsigned int dma_size;
    struct rk29_dma_client *client;
    int dma_inited;
	void	*priv;
    struct clk	*spdif_clk;
    struct clk	*spdif_hclk;
};

struct RK29_SPDIF_REG{
    unsigned int cfgr;
    unsigned int sdblr;
    unsigned int dmacr;
    unsigned int intcr;
    unsigned int intsr;
    unsigned int xfer;
    unsigned int smpdr;
    unsigned int vldfr;
    unsigned int usrdr;
    unsigned int chnsr;
};
extern struct rk29xx_spdif rk29_spdif;
extern void rk29_spdif_ctrl(int on_off, bool stopSPDIF); 
#endif

