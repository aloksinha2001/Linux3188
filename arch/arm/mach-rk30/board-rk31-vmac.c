//$_FOR_ROCKCHIP_RBOX_$

static int rk30_vmac_register_set(void)
{
	//config rk30 vmac as rmii
	writel_relaxed(0x3 << 16 | 0x2, RK30_GRF_BASE + GRF_SOC_CON1);
	int val = readl_relaxed(RK30_GRF_BASE + GRF_IO_CON3);
	writel_relaxed(val | 0xf << 16 | 0xf, RK30_GRF_BASE + GRF_IO_CON3);
	return 0;
}

static int rk30_rmii_io_init(void)
{
	int err;

	iomux_set(RMII_TXEN);
	iomux_set(RMII_TXD1);
	iomux_set(RMII_TXD0);
	iomux_set(RMII_RXD0);
	iomux_set(RMII_RXD1);
	iomux_set(RMII_CLKOUT);
	iomux_set(RMII_RXERR);
	iomux_set(RMII_CRS);
	iomux_set(RMII_MD);
	iomux_set(RMII_MDCLK);
	iomux_set(GPIO3_D2);
#if 0
	//iomux_set(GPIO3_B5);
	iomux_set(GPIO0_C0);

	//phy power gpio
	err = gpio_request(PHY_PWR_EN_GPIO, "phy_power_en");
	if (err) {
		return -1;
	}
	//phy power down
	gpio_direction_output(PHY_PWR_EN_GPIO, GPIO_LOW);
	gpio_set_value(PHY_PWR_EN_GPIO, GPIO_LOW);
#endif
	return 0;
}

struct regulator *ldo_33;

static int rk30_rmii_io_deinit(void)
{
#if 0
	//phy power down
	gpio_direction_output(PHY_PWR_EN_GPIO, GPIO_LOW);
	gpio_set_value(PHY_PWR_EN_GPIO, GPIO_LOW);
	//free
	gpio_free(PHY_PWR_EN_GPIO);
#else
	regulator_disable(ldo_33);
	regulator_put(ldo_33);
#endif
	return 0;
}

static int rk30_rmii_power_control(int enable)
{
	ldo_33 = regulator_get(NULL, "act_ldo5"); 

	if (ldo_33 == NULL || IS_ERR(ldo_33)){
		printk("get rmii ldo failed!\n");
		return -1;
	}

	if (enable) {
		//enable phy power
		printk("power on phy\n");

		iomux_set(RMII_TXEN);
		iomux_set(RMII_TXD1);
		iomux_set(RMII_TXD0);
		iomux_set(RMII_RXD0);
		iomux_set(RMII_RXD1);
		iomux_set(RMII_CLKOUT);
		iomux_set(RMII_RXERR);
		iomux_set(RMII_CRS);
		iomux_set(RMII_MD);
		iomux_set(RMII_MDCLK);
		iomux_set(GPIO3_D2);
#if 1
		//regulator_set_voltage(ldo_33, 3300000, 300000);
                regulator_enable(ldo_33);
                regulator_put(ldo_33);
		//gpio_direction_output(RK30_PIN3_PD2, GPIO_LOW);
		gpio_set_value(RK30_PIN3_PD2, GPIO_LOW);
		msleep(20);
		gpio_set_value(RK30_PIN3_PD2, GPIO_HIGH);
#else
		iomux_set(GPIO0_C0);

		gpio_direction_output(PHY_PWR_EN_GPIO, GPIO_HIGH);
		gpio_set_value(PHY_PWR_EN_GPIO, GPIO_HIGH);
#endif
	}else {
#if 1
                regulator_disable(ldo_33);
        	regulator_put(ldo_33);
#else
		gpio_direction_output(PHY_PWR_EN_GPIO, GPIO_LOW);
		gpio_set_value(PHY_PWR_EN_GPIO, GPIO_LOW);
#endif
	}
	return 0;
}

#define BIT_EMAC_SPEED      (1 << 1)
static int rk29_vmac_speed_switch(int speed)
{
	//printk("%s--speed=%d\n", __FUNCTION__, speed);
	if (10 == speed) {
	    writel_relaxed(readl_relaxed(RK30_GRF_BASE + GRF_SOC_CON1) & (~BIT_EMAC_SPEED), RK30_GRF_BASE + GRF_SOC_CON1);
	} else {
	    writel_relaxed(readl_relaxed(RK30_GRF_BASE + GRF_SOC_CON1) | ( BIT_EMAC_SPEED), RK30_GRF_BASE + GRF_SOC_CON1);
	}
}

struct rk29_vmac_platform_data board_vmac_data = {
	.vmac_register_set = rk30_vmac_register_set,
	.rmii_io_init = rk30_rmii_io_init,
	.rmii_io_deinit = rk30_rmii_io_deinit,
	.rmii_power_control = rk30_rmii_power_control,
	.rmii_speed_switch = rk29_vmac_speed_switch,
};
