/*
 * Copyright (C) 2010-2011 ARM Limited. All rights reserved.
 *
 * This program is free software and is provided to you under the terms of the GNU General Public License version 2
 * as published by the Free Software Foundation, and any use by you of this program is subject to the terms of such GNU licence.
 *
 * A copy of the licence is included with the program, and can also be obtained from Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

/**
 * @file mali_platform.c
 * Platform specific Mali driver functions for a default platform
 */
#include "mali_kernel_common.h"
#include "mali_osk.h"
#include "mali_platform.h"

#include <linux/err.h>
#include <linux/module.h>
#include <linux/clk.h>
#include <mach/irqs.h>
#include <mach/system.h>

int mali_clk_div = 3;
module_param(mali_clk_div, int, S_IRUSR | S_IWUSR | S_IWGRP | S_IRGRP | S_IROTH);
MODULE_PARM_DESC(mali_clk_div, "Clock divisor for mali");

struct clk *h_pd_gpu, *h_gpu;
int mali_clk_flag=0;

_mali_osk_errcode_t mali_platform_init(void)
{
	unsigned long rate,rtmp,rtmp1;
//	int clk_div;
//	int mali_used = 0;
	struct clk *h_tmp;

	//get mali ahb clock
	h_pd_gpu = clk_get(NULL, "pd_gpu");
	if(!h_pd_gpu){
		MALI_PRINT(("try to get pd_gpu clock failed!\n"));
		MALI_ERROR(1);
	}
	//get mali clk
	h_gpu = clk_get(NULL, "gpu");
	if(!h_gpu){
		MALI_PRINT(("try to get gpu clock failed!\n"));
		MALI_ERROR(1);
	}
	rate = clk_get_rate(h_gpu) / 1000;
	//set mali parent clock
	if((h_tmp = clk_get_parent(h_gpu)))
	if((rtmp = clk_get_rate(h_tmp))){
		MALI_PRINT(("get mali new parent clock:%d Hz\n", rtmp));
		if((h_tmp = clk_get(NULL, "codec_pll")))
		if((rtmp1 = clk_get_rate(h_tmp))){
		    MALI_PRINT(("get mali old parent clock:%d Hz\n", rtmp1));
		    rtmp1 = rtmp1 / 1000;
		    rate = (rtmp/rtmp1)*rate;
		}
	}

	//set mali clock
// = 49875000
//	rate = clk_get_rate(h_ve_pll);

//	if(!script_parser_fetch("mali_para", "mali_used", &mali_used, 1)) {
//		if (mali_used == 1) {
//			if (!script_parser_fetch("mali_para", "mali_clkdiv", &clk_div, 1)) {
//				if (clk_div > 0) {
//					pr_info("mali: use config clk_div %d\n", clk_div);
//					mali_clk_div = clk_div;
//				}
//			}
//		}
//	}

//	pr_info("mali: clk_div %d\n", mali_clk_div);
//	rate /= mali_clk_div;

//	rate = (unsigned long)49875000;
	
	if(clk_set_rate(h_gpu, rate)){
		MALI_PRINT(("try to set gpu clock failed!\n"));
	}

//	if(clk_reset(h_mali_clk,0)){
//		MALI_PRINT(("try to reset release failed!\n"));
//	}

	MALI_PRINT(("mali clock set completed, clock is  %d Hz\n", rate));


	/*enable mali axi/apb clock*/
	if(mali_clk_flag == 0)
	{
		MALI_PRINT(("enable mali clock\n"));
		if(clk_enable(h_gpu))
		    MALI_PRINT(("try to enable mali clock failed!\n"));
		else
		    mali_clk_flag = 1;
//		    MALI_PRINT(("try to disable h_pd_gpu\n"));
//		    clk_disable(h_pd_gpu);


//		clk_disable(h_gpu);
	}

    MALI_SUCCESS;
}

_mali_osk_errcode_t mali_platform_deinit(void)
{
	/*close mali axi/apb clock*/
	if(mali_clk_flag == 1)
	{
		//MALI_PRINT(("disable mali clock\n"));
		mali_clk_flag = 0;
	       clk_disable(h_gpu);
	       clk_enable(h_pd_gpu);
	}

    MALI_SUCCESS;
}

_mali_osk_errcode_t mali_platform_power_mode_change(mali_power_mode power_mode)
{
    MALI_SUCCESS;
}

void mali_gpu_utilization_handler(u32 utilization)
{
}

void set_mali_parent_power_domain(void* dev)
{
}


