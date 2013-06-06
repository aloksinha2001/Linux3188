#include <linux/module.h>
#include <linux/init.h>
#include <linux/i2c.h>
#include <linux/device.h>
#include <linux/delay.h>
#include <mach/gpio.h>
#include <linux/err.h>
#include <linux/clk.h>
#include <mach/iomux.h>
#include <linux/display-sys.h>
#include <mach/board.h>
#if 0
#define	DBG(x...)	printk(KERN_INFO x)
#else
#define	DBG(x...)
#endif

struct i2c_client * rk1000_control_client = NULL;




int reg_send_data(struct i2c_client *client, const char start_reg,
				const char *buf, int count, unsigned int scl_rate)
{
    int ret; 
    
    ret = i2c_master_reg8_send(client, start_reg, buf, (int)count, 20*1000);
    return ret;  
}


static int rk1000_control_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
    int ret;
	struct clk *iis_clk;
	struct rkdisplay_platform_data *tv_data;
	/* reg[0x00] = 0x88, --> ADC_CON
	   reg[0x01] = 0x0d, --> CODEC_CON
	   reg[0x02] = 0x22, --> I2C_CON
	   reg[0x03] = 0x00, --> TVE_CON
	 */
	char data[4] = {0x88, 0x0d, 0x22, 0x00};
	
    printk("rk1000_control_probe\n");
    
    #if defined(CONFIG_SND_RK29_SOC_I2S_8CH)        
	iis_clk = clk_get_sys("rk29_i2s.0", "i2s");
	#elif defined(CONFIG_SND_RK29_SOC_I2S_2CH)
	iis_clk = clk_get_sys("rk29_i2s.1", "i2s");
	#else
	iis_clk = clk_get_sys("rk29_i2s.2", "i2s");
	#endif
	if (IS_ERR(iis_clk)) {
		printk("failed to get i2s clk\n");
		ret = PTR_ERR(iis_clk);
	}else{
		DBG("got i2s clk ok!\n");
		clk_enable(iis_clk);
		clk_set_rate(iis_clk, 11289600);
		#if defined(CONFIG_ARCH_RK29)
		rk29_mux_api_set(GPIO2D0_I2S0CLK_MIIRXCLKIN_NAME, GPIO2H_I2S0_CLK);
		#elif defined(CONFIG_ARCH_RK30)
        rk30_mux_api_set(GPIO0B0_I2S8CHCLK_NAME, GPIO0B_I2S_8CH_CLK);
        #elif defined(CONFIG_ARCH_RK3066B)||defined(CONFIG_ARCH_RK3188)
		iomux_set(I2S0_CLK);
		#endif
		clk_put(iis_clk);
	}
    
    if(client->dev.platform_data) {
		tv_data = client->dev.platform_data;
		if(tv_data->io_reset_pin != INVALID_GPIO) {
	    	ret = gpio_request(tv_data->io_reset_pin, "rk1000 reset");
		    if (ret){   
		        printk("rk1000_control_probe request gpio fail\n");
		        //goto err1;
		    }
		    
		    gpio_set_value(tv_data->io_reset_pin, GPIO_LOW);
		    gpio_direction_output(tv_data->io_reset_pin, GPIO_LOW);
		    mdelay(2);
		    gpio_set_value(tv_data->io_reset_pin, GPIO_HIGH);
		}
	}
    
    rk1000_control_client = client;

#ifdef CONFIG_SND_SOC_RK1000
    data[1] = 0x00;
#endif

	ret = i2c_master_reg8_send(client,0,data, 4, 20 * 1000);
	
	printk("rk1000_control_probe ok\n");
    return 0;
}

int rk1000_control_write_block(u8 addr, u8 *buf, u8 len)
{
	int i;
	int ret = 0;
	struct i2c_client *client;
    
    client = rk1000_control_client;

	if(client == NULL){
		printk("rk1000_tv_i2c_client not init!\n");
		return -1;
	}

	for(i=0; i<len; i++){
		ret = i2c_master_reg8_send(client, addr+i, buf+i, 1 ,200*1000);
		if(ret != 1){
			printk("rk1000_control_set_reg err, addr=0x%.x, val=0x%.x", addr+i, buf[i]);
			break;
		}
	}

	return ret;
}
EXPORT_SYMBOL(rk1000_control_write_block);



static int rk1000_control_remove(struct i2c_client *client)
{
	return 0;
}

static const struct i2c_device_id rk1000_control_id[] = {
	{ "rk1000_control", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, rk1000_control_id);

static struct i2c_driver rk1000_control_driver = {
	.driver = {
		.name = "rk1000_control",
	},
	.probe = rk1000_control_probe,
	.remove = rk1000_control_remove,
	.id_table = rk1000_control_id,
};

static int __init rk1000_control_init(void)
{
	return i2c_add_driver(&rk1000_control_driver);
}

static void __exit rk1000_control_exit(void)
{
	i2c_del_driver(&rk1000_control_driver);
}

subsys_initcall_sync(rk1000_control_init);
//module_init(rk1000_control_init);
module_exit(rk1000_control_exit);


MODULE_DESCRIPTION("RK1000 control driver");
MODULE_AUTHOR("Rock-chips, <www.rock-chips.com>");
MODULE_LICENSE("GPL");

