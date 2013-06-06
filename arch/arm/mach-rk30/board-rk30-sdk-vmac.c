//$_FOR_ROCKCHIP_RBOX_$

static int rk30_vmac_register_set(void)
{
//$_rbox_$_modify_$_chenzhi: fix the bug of CRS_DV(AR8032)
//$_rbox_$_modify_$_begin
	int value;
	//config rk30 vmac as rmii
	writel_relaxed(0x3 << 16 | 0x2, RK30_GRF_BASE + GRF_SOC_CON1);
        value = readl(RK30_GRF_BASE + GRF_SOC_CON2);
        writel(0x1 << 6 | 0x1 << 22 | value, RK30_GRF_BASE + GRF_SOC_CON2);
//$_rbox_$_modify_$_end
	return 0;
}

static int rk30_rmii_io_init(void)
{
	int err;
	rk30_mux_api_set(GPIO1D6_CIF1DATA11_NAME, GPIO1D_GPIO1D6);

	rk30_mux_api_set(GPIO1D2_CIF1CLKIN_NAME, GPIO1D_GPIO1D2);
	rk30_mux_api_set(GPIO1D1_CIF1HREF_MIIMDCLK_NAME, GPIO1D_MII_MDCLK);
	rk30_mux_api_set(GPIO1D0_CIF1VSYNC_MIIMD_NAME, GPIO1D_MII_MD);

	rk30_mux_api_set(GPIO1C7_CIFDATA9_RMIIRXD0_NAME, GPIO1C_RMII_RXD0);
	rk30_mux_api_set(GPIO1C6_CIFDATA8_RMIIRXD1_NAME, GPIO1C_RMII_RXD1);
	rk30_mux_api_set(GPIO1C5_CIFDATA7_RMIICRSDVALID_NAME, GPIO1C_RMII_CRS_DVALID);
	rk30_mux_api_set(GPIO1C4_CIFDATA6_RMIIRXERR_NAME, GPIO1C_RMII_RX_ERR);
	rk30_mux_api_set(GPIO1C3_CIFDATA5_RMIITXD0_NAME, GPIO1C_RMII_TXD0);
	rk30_mux_api_set(GPIO1C2_CIF1DATA4_RMIITXD1_NAME, GPIO1C_RMII_TXD1);
	rk30_mux_api_set(GPIO1C1_CIFDATA3_RMIITXEN_NAME, GPIO1C_RMII_TX_EN);
	rk30_mux_api_set(GPIO1C0_CIF1DATA2_RMIICLKOUT_RMIICLKIN_NAME, GPIO1C_RMII_CLKOUT);

	//phy power gpio
	err = gpio_request(PHY_PWR_EN_GPIO, "phy_power_en");
	if (err) {
		return -1;
	}
	//phy power down
	gpio_direction_output(PHY_PWR_EN_GPIO, GPIO_LOW);
	gpio_set_value(PHY_PWR_EN_GPIO, GPIO_LOW);
	return 0;
}

static int rk30_rmii_io_deinit(void)
{
	//phy power down
	gpio_direction_output(PHY_PWR_EN_GPIO, GPIO_LOW);
	gpio_set_value(PHY_PWR_EN_GPIO, GPIO_LOW);
	//free
	gpio_free(PHY_PWR_EN_GPIO);
	return 0;
}

static int rk30_rmii_power_control(int enable)
{
    struct regulator *ldo_rmii;
    
//$_rbox_$_modify_$_chenzhi_20121008: power control by pmu
//$_rbox_$_modify_$_huangzhibao_20121215: add ti pmu
//$_rbox_$_modify_$_begin    
#if defined (CONFIG_MFD_WM831X_I2C)
    if(g_pmic_type == PMIC_TYPE_WM8326)
        ldo_rmii = regulator_get(NULL, "ldo9");
#endif
#if defined (CONFIG_MFD_TPS65910)
    if(g_pmic_type == PMIC_TYPE_TPS65910)
        ldo_rmii = regulator_get(NULL, "vaux33");
#endif  
	if (ldo_rmii == NULL || IS_ERR(ldo_rmii)){
	  printk("get rmii ldo failed!\n");
	}
//$_rbox_$_modify_$_end

	if (enable) {
		//enable phy power
		printk("power on phy\n");
		rk30_mux_api_set(GPIO1D6_CIF1DATA11_NAME, GPIO1D_GPIO1D6);

		rk30_mux_api_set(GPIO1D2_CIF1CLKIN_NAME, GPIO1D_GPIO1D2);
		rk30_mux_api_set(GPIO1D1_CIF1HREF_MIIMDCLK_NAME, GPIO1D_MII_MDCLK);
		rk30_mux_api_set(GPIO1D0_CIF1VSYNC_MIIMD_NAME, GPIO1D_MII_MD);

		rk30_mux_api_set(GPIO1C7_CIFDATA9_RMIIRXD0_NAME, GPIO1C_RMII_RXD0);
		rk30_mux_api_set(GPIO1C6_CIFDATA8_RMIIRXD1_NAME, GPIO1C_RMII_RXD1);
		rk30_mux_api_set(GPIO1C5_CIFDATA7_RMIICRSDVALID_NAME, GPIO1C_RMII_CRS_DVALID);
		rk30_mux_api_set(GPIO1C4_CIFDATA6_RMIIRXERR_NAME, GPIO1C_RMII_RX_ERR);
		rk30_mux_api_set(GPIO1C3_CIFDATA5_RMIITXD0_NAME, GPIO1C_RMII_TXD0);
		rk30_mux_api_set(GPIO1C2_CIF1DATA4_RMIITXD1_NAME, GPIO1C_RMII_TXD1);
		rk30_mux_api_set(GPIO1C1_CIFDATA3_RMIITXEN_NAME, GPIO1C_RMII_TX_EN);
		rk30_mux_api_set(GPIO1C0_CIF1DATA2_RMIICLKOUT_RMIICLKIN_NAME, GPIO1C_RMII_CLKOUT);
//$_rbox_$_modify_$_chenzhi_20121008: power control by pmu
//$_rbox_$_modify_$_begin
		if(ldo_rmii && (!regulator_is_enabled(ldo_rmii))) {
		//regulator_set_voltage(ldo_rmii, 3300000, 3300000);
		  regulator_enable(ldo_rmii);
		  regulator_put(ldo_rmii);
		  mdelay(500);
		}
		gpio_direction_output(PHY_PWR_EN_GPIO, GPIO_HIGH);
		gpio_set_value(PHY_PWR_EN_GPIO, GPIO_LOW);
                mdelay(20);
		gpio_set_value(PHY_PWR_EN_GPIO, GPIO_HIGH);
//$_rbox_$_modify_$_end
	}else {
		gpio_direction_output(PHY_PWR_EN_GPIO, GPIO_LOW);
		gpio_set_value(PHY_PWR_EN_GPIO, GPIO_LOW);
//$_rbox_$_modify_$_chenzhi_20121008: power control by pmu
//$_rbox_$_modify_$_begin
		if(ldo_rmii && regulator_is_enabled(ldo_rmii)) {
		  regulator_disable(ldo_rmii);
		  regulator_put(ldo_rmii);
		  mdelay(500);
		}
//$_rbox_$_modify_$_end
	}
	return 0;
}

//$_rbox_$_modify_$_chenzhi_20120523: switch speed between 10M and 100M
//$_rbox_$_modify_$_begin
#define BIT_EMAC_SPEED     (1 << 1)
#define grf_readl(offset)	readl(RK30_GRF_BASE + offset)
#define grf_writel(v, offset)	writel(v, RK30_GRF_BASE + offset)
static int rk30_vmac_speed_switch(int speed)
{
//	printk("%s--speed=%d\n", __FUNCTION__, speed);
	if (10 == speed) {
            grf_writel((grf_readl(GRF_SOC_CON1) | (2<<16)) & (~BIT_EMAC_SPEED), GRF_SOC_CON1);
        } else {
            grf_writel(grf_readl(GRF_SOC_CON1) | (2<<16) | BIT_EMAC_SPEED, GRF_SOC_CON1);
        }
}
//$_rbox_$_modify_$_end

struct rk29_vmac_platform_data board_vmac_data = {
	.vmac_register_set = rk30_vmac_register_set,
	.rmii_io_init = rk30_rmii_io_init,
	.rmii_io_deinit = rk30_rmii_io_deinit,
	.rmii_power_control = rk30_rmii_power_control,
//$_rbox_$_modify_$_chenzhi_20121011: switch speed between 10M and 100M for rk30
	.rmii_speed_switch = rk30_vmac_speed_switch,
};
