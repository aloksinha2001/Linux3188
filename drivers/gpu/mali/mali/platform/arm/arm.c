/* Copyright (C) 2010, 2012 ARM Limited. All rights reserved.
 *
 * This program is free software and is provided to you under the terms of the GNU General Public License version 2
 * as published by the Free Software Foundation, and any use by you of this program is subject to the terms of such GNU licence.
 *
 * A copy of the licence is included with the program, and can also be obtained from Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */
#include "arm.h"

/* platform functions start */
#ifdef MALI_CFG_REGULATOR

void mali_regulator_set_voltage(int min_uV, int max_uV)
{
	int voltage;
	min_uV = mali_gpu_vol;
	max_uV = mali_gpu_vol;
#if MALI_VOLTAGE_LOCK
	if (mali_vol_lock_flag == MALI_FALSE) {
		if (min_uV < MALI_BOTTOMLOCK_VOL || max_uV < MALI_BOTTOMLOCK_VOL) {
			min_uV = MALI_BOTTOMLOCK_VOL;
			max_uV = MALI_BOTTOMLOCK_VOL;
		}
	} else if (_mali_osk_atomic_read(&voltage_lock_status) > 0 ) {
		if (min_uV < mali_lock_vol || max_uV < mali_lock_vol) {
		
			min_uV = mali_lock_vol;
			max_uV = mali_lock_vol;
			
		}
	}
#endif

	_mali_osk_lock_wait(mali_dvfs_lock, _MALI_OSK_LOCKMODE_RW);

	if( IS_ERR_OR_NULL(g3d_regulator) )
	{
		MALI_DEBUG_PRINT(1, ("error on mali_regulator_set_voltage : g3d_regulator is null\n"));
		return;
	}
	MALI_DEBUG_PRINT(2, ("= regulator_set_voltage: %d, %d \n",min_uV, max_uV));

	regulator_set_voltage(g3d_regulator,min_uV,max_uV);
	voltage = regulator_get_voltage(g3d_regulator);

	mali_gpu_vol = voltage;
	MALI_DEBUG_PRINT(1, ("= regulator_get_voltage: %d \n",mali_gpu_vol));

	_mali_osk_lock_signal(mali_dvfs_lock, _MALI_OSK_LOCKMODE_RW);
}
#endif

unsigned long mali_clk_get_rate(void) {
	return clk_get_rate(mali_clock);
}

mali_bool mali_clk_put(void) {
	MALI_DEBUG_PRINT(4, ("mali_clk_put() called\n"));
	// mali clock get always.
	if (mali_clock != NULL) {
		clk_put(mali_clock);
	}
	return MALI_TRUE;
}

mali_bool mali_clk_get(void) {
	MALI_DEBUG_PRINT(4, ("mali_clk_get() called\n"));
	// mali clock get always.
	if (mali_clock == NULL) {
		mali_clock = clk_get(NULL, GPUCLK_NAME);

		if (IS_ERR(mali_clock)) {
			MALI_PRINT( ("MALI Error : failed to get source mali clock\n"));
			return MALI_FALSE;
		}
	}

	return MALI_TRUE;
}

mali_bool mali_clk_set_rate(unsigned int clk, unsigned int mhz) {
	unsigned long rate = 0;

	clk = mali_gpu_clk;

	MALI_DEBUG_PRINT(4, ("mali_clk_set_rate() called\n"));
	//_mali_osk_lock_wait(mali_dvfs_lock, _MALI_OSK_LOCKMODE_RW);

	if (mali_clk_get() == MALI_FALSE)
		return MALI_FALSE;

	rate = (unsigned long) clk * (unsigned long) mhz;
	MALI_DEBUG_PRINT(3, ("= clk_set_rate : %d , %d \n",clk, mhz ));

	if (clk_enable(mali_clock) < 0)
		return MALI_FALSE;

	clk_set_rate(mali_clock, rate);
	rate = clk_get_rate(mali_clock);

		mali_gpu_clk = (int) (rate / mhz);

	GPU_MHZ = mhz;
	MALI_DEBUG_PRINT(3, ("= clk_get_rate: %d \n",mali_gpu_clk));

//	mali_clk_put(MALI_FALSE);

	//_mali_osk_lock_signal(mali_dvfs_lock, _MALI_OSK_LOCKMODE_RW);

	return MALI_TRUE;
}

mali_bool init_mali_clock(void) {
	mali_bool ret = MALI_TRUE;

	gpu_power_state = 0;
	MALI_DEBUG_PRINT(4, ("init_mali_clock() called\n"));
	if (mali_clock != 0)
		return ret; // already initialized

	mali_dvfs_lock = _mali_osk_lock_init(
			_MALI_OSK_LOCKFLAG_NONINTERRUPTABLE | _MALI_OSK_LOCKFLAG_ONELOCK, 0,
			0);
	if (mali_dvfs_lock == NULL)
		return _MALI_OSK_ERR_FAULT;

	if (mali_clk_get() == MALI_FALSE)
		return MALI_FALSE;

	clk_set_rate(mali_clock, GPUMINCLK);

	if (mali_clk_set_rate(mali_gpu_clk, GPU_MHZ) == MALI_FALSE) {
		ret = MALI_FALSE;
		goto err_clock_get;
	}

	MALI_PRINT(("init_mali_clock mali_clock %p \n", mali_clock));

#ifdef MALI_CFG_REGULATOR
#if USING_MALI_PMM
	g3d_regulator = regulator_get(&mali_gpu_device.dev, "vdd_core");
#else
	g3d_regulator = regulator_get(NULL, "vdd_core");
#endif

	if (IS_ERR(g3d_regulator))
	{
		MALI_PRINT( ("MALI Error : failed to get vdd_g3d\n"));
		ret = MALI_FALSE;
		goto err_regulator;
	}

//	regulator_enable(g3d_regulator);

	MALI_DEBUG_PRINT(1, ("= regulator_enable -> use cnt: %d \n",mali_regulator_get_usecount()));
	mali_regulator_set_voltage(mali_gpu_vol, mali_gpu_vol);
#endif

	MALI_DEBUG_PRINT(2, ("MALI Clock is set at mali driver\n"));

	MALI_DEBUG_PRINT(3,
			("::clk_put:: %s mali_parent_clock - normal\n", __FUNCTION__));
	MALI_DEBUG_PRINT(3,
			("::clk_put:: %s mpll_clock  - normal\n", __FUNCTION__));

//	mali_clk_put(MALI_FALSE);

	return MALI_TRUE;

#ifdef MALI_CFG_REGULATOR
	err_regulator:
	regulator_put(g3d_regulator);
#endif

	err_clock_get: 
//	mali_clk_put(MALI_TRUE);

	return ret;
}

mali_bool deinit_mali_clock(void) {
	if (mali_clock == 0)
		return MALI_TRUE;

#ifdef MALI_CFG_REGULATOR
	if (g3d_regulator)
	{
		regulator_put(g3d_regulator);
		g3d_regulator=NULL;
	}
#endif
	clk_set_rate(mali_clock, GPUMINCLK);
	clk_disable(mali_clock);
	mali_clk_put();

	return MALI_TRUE;
}

_mali_osk_errcode_t enable_mali_clocks(void) {
	int err;
	err = clk_enable(mali_clock);
	MALI_DEBUG_PRINT(3,
			("enable_mali_clocks mali_clock %p error %d \n", mali_clock, err));

	mali_clk_set_rate(mali_gpu_clk, GPU_MHZ);
	MALI_SUCCESS;
}

_mali_osk_errcode_t disable_mali_clocks(void) {
	clk_disable(mali_clock);
	MALI_DEBUG_PRINT(3, ("disable_mali_clocks mali_clock %p \n", mali_clock));

	MALI_SUCCESS;
}

_mali_osk_errcode_t gpu_power_domain_control(int bpower_on) {
	if (bpower_on) {
		u32 timeout;
//		pmu_set_power_domain(PD_GPU,1);
		timeout = 10;
		while (!pmu_power_domain_is_on(PD_GPU)){
			if (timeout == 0) {
				MALI_PRINTF(("Power domain  enable failed.\n"));
				return -ETIMEDOUT;
			}
			timeout--;
			_mali_osk_time_ubusydelay(100);
		}
		
		
	} else {
		u32 timeout;
//		pmu_set_power_domain(PD_GPU,0);
		/* Wait max 1ms */
		timeout = 10;
		while (pmu_power_domain_is_on(PD_GPU)) {
			if (timeout == 0) {
				MALI_PRINTF(("Power domain  disable failed.\n" ));
				return -ETIMEDOUT;
			}
			timeout--;
			_mali_osk_time_ubusydelay(100);
		}

	}

	MALI_SUCCESS;
}

_mali_osk_errcode_t mali_platform_init(void) {
	MALI_DEBUG_PRINT(4, ("mali_platform_init() called\n"));
	MALI_CHECK(init_mali_clock(), _MALI_OSK_ERR_FAULT);
#if MALI_VOLTAGE_LOCK
	_mali_osk_atomic_init(&voltage_lock_status, 0);
#endif

	MALI_SUCCESS;
}

_mali_osk_errcode_t mali_platform_powerdown(u32 cores) {
	MALI_DEBUG_PRINT(3,
			("power down is called in mali_platform_powerdown state %x core %x \n", gpu_power_state, cores));

	if (gpu_power_state != 0) // power down after state is 0
			{
		gpu_power_state = gpu_power_state & (~cores);
		if (gpu_power_state == 0) {
			MALI_DEBUG_PRINT( 3, ("disable clock\n"));
			disable_mali_clocks();
		}
	} else {
		MALI_PRINT(
				("mali_platform_powerdown gpu_power_state == 0 and cores %x \n", cores));
	}

	MALI_SUCCESS;
}

_mali_osk_errcode_t mali_platform_powerup(u32 cores) {
	MALI_DEBUG_PRINT(3,
			("power up is called in mali_platform_powerup state %x core %x \n", gpu_power_state, cores));

	if (gpu_power_state == 0) // power up only before state is 0
			{
		gpu_power_state = gpu_power_state | cores;

		if (gpu_power_state != 0) {
			MALI_DEBUG_PRINT(4, ("enable clock \n"));
			enable_mali_clocks();
		}
	} else {
		gpu_power_state = gpu_power_state | cores;
	}

	MALI_SUCCESS;
}
void mali_gpu_utilization_handler(u32 utilization) {
	if (bPoweroff == 0) {
	}
}

#if MALI_POWER_MGMT_TEST_SUITE
u32 pmu_get_power_up_down_info(void)
{
	return 4095;
}

#endif


#if MALI_VOLTAGE_LOCK
int mali_voltage_lock_push(int lock_vol)
{
	int prev_status = _mali_osk_atomic_read(&voltage_lock_status);

	if (prev_status < 0) {
		MALI_PRINT(("gpu voltage lock status is not valid for push\n"));
		return -1;
	}
	if (prev_status == 0) {
		mali_lock_vol = lock_vol;
		if (mali_gpu_vol < mali_lock_vol)
		mali_regulator_set_voltage(mali_lock_vol, mali_lock_vol);
	} else {
		MALI_PRINT(("gpu voltage lock status is already pushed, current lock voltage : %d\n", mali_lock_vol));
		return -1;
	}

	return _mali_osk_atomic_inc_return(&voltage_lock_status);
}

int mali_voltage_lock_pop(void)
{
	if (_mali_osk_atomic_read(&voltage_lock_status) <= 0) {
		MALI_PRINT(("gpu voltage lock status is not valid for pop\n"));
		return -1;
	}
	return _mali_osk_atomic_dec_return(&voltage_lock_status);
}

int mali_voltage_lock_init(void)
{
	mali_vol_lock_flag = MALI_TRUE;

	MALI_SUCCESS;
}
#endif
/* platform functions end */

int mali_platform_device_register(void) {
	int err = -1;
	MALI_DEBUG_PRINT(4, ("mali_platform_device_register() called\n"));
	/* FIXME: Not sure if this is the place. */
	gpu_power_domain_control(1);
	mali_platform_init();

	err = platform_device_add_resources(&mali_gpu_device,
			mali_gpu_resources_m400_mp4,
			sizeof(mali_gpu_resources_m400_mp4)
					/ sizeof(mali_gpu_resources_m400_mp4[0]));

	if (0 == err) {
		err = platform_device_add_data(&mali_gpu_device, &mali_gpu_data,
				sizeof(mali_gpu_data));
		if (0 == err) {
			/* Register the platform device */
			err = platform_device_register(&mali_gpu_device);
			if (0 == err) {
#ifdef CONFIG_PM_RUNTIME
				pm_runtime_set_autosuspend_delay(&(mali_gpu_device.dev), 1000);
				pm_runtime_use_autosuspend(&(mali_gpu_device.dev));
				pm_runtime_enable(&(mali_gpu_device.dev));
#endif

				return 0;
			}
		}

		platform_device_unregister(&mali_gpu_device);
	}

	return err;
}

void mali_platform_device_unregister(void) {
	MALI_DEBUG_PRINT(4, ("mali_platform_device_unregister() called\n"));
	deinit_mali_clock();
	gpu_power_domain_control(0);
	platform_device_unregister(&mali_gpu_device);
}

void mali_platform_device_release(struct device *device) {
	MALI_DEBUG_PRINT(4, ("mali_platform_device_release() called\n"));
}
