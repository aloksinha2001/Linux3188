/*
 * Copyright (C) 2010-2012 ARM Limited. All rights reserved.
 * 
 * This program is free software and is provided to you under the terms of the GNU General Public License version 2
 * as published by the Free Software Foundation, and any use by you of this program is subject to the terms of such GNU licence.
 * 
 * A copy of the licence is included with the program, and can also be obtained from Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#ifndef __ARCH_CONFIG_H__
#define __ARCH_CONFIG_H__

#include "mach/irqs.h"
#include "mach/io.h"

/* Configuration for the EB platform with ZBT memory enabled */
//RK3066

static _mali_osk_resource_t arch_configuration [] =
{
/*	{
		.type = PMU,
		.description = "Mali-400 PMU",
		.base = RK30_GPU_PHYS + 0x2000,
		.irq = 0,	//IRQ_PMU,
		.mmu_id = 0
	},*/
	{
		.type = MALI400GP,
		.description = "Mali-400 GP",
		.base = RK30_GPU_PHYS,
		.irq = IRQ_GPU_GP,
		.mmu_id = 1
	},
	{
		.type = MALI400PP,
		.base = RK30_GPU_PHYS + 0x8000,
		.irq = IRQ_GPU_PP,
		.description = "Mali-400 PP 0",
		.mmu_id = 2
	},
	{
		.type = MALI400PP,
		.base = RK30_GPU_PHYS + 0xA000,
		.irq = IRQ_GPU_PP,
		.description = "Mali-400 PP 1",
		.mmu_id = 3
	},
	{
		.type = MALI400PP,
		.base = RK30_GPU_PHYS + 0xC000,
		.irq = IRQ_GPU_PP,
		.description = "Mali-400 PP 2",
		.mmu_id = 4
	},
	{
		.type = MALI400PP,
		.base = RK30_GPU_PHYS + 0xE000,
		.irq = IRQ_GPU_PP,
		.description = "Mali-400 PP 3",
		.mmu_id = 5
	},
	{
		.type = MMU,
		.base = RK30_GPU_PHYS + 0x3000,
		.irq = IRQ_GPU_MMU,
		.description = "Mali-400 MMU for GP",
		.mmu_id = 1
	},
	{
		.type = MMU,
		.base = RK30_GPU_PHYS + 0x4000,
		.irq = IRQ_GPU_MMU,
		.description = "Mali-400 MMU for PP 0",
		.mmu_id = 2
	},
	{
		.type = MMU,
		.base = RK30_GPU_PHYS + 0x5000,
		.irq = IRQ_GPU_MMU,
		.description = "Mali-400 MMU for PP 1",
		.mmu_id = 3
	},
	{
		.type = MMU,
		.base = RK30_GPU_PHYS + 0x6000,
		.irq = IRQ_GPU_MMU,
		.description = "Mali-400 MMU for PP 2",
		.mmu_id = 4
	},
	{
		.type = MMU,
		.base = RK30_GPU_PHYS + 0x7000,
		.irq = IRQ_GPU_MMU,
		.description = "Mali-400 MMU for PP 3",
		.mmu_id = 5
	},
	{
		.type = OS_MEMORY,
		.description = "System memory",
// .alloc_order = 1, /* Lowest preference for this memory */
		.alloc_order = 0, /* Highest preference for this memory */
		.size = 192 * 1024 * 1024, /*  MB */
		.flags = _MALI_CPU_WRITEABLE | _MALI_CPU_READABLE | _MALI_PP_READABLE | _MALI_PP_WRITEABLE |_MALI_GP_READABLE | _MALI_GP_WRITEABLE
	},
	{
		.type = MEM_VALIDATION,.description = "memory validation",
		.base = 0x60000000,.size = 0x00800000,
		.flags = _MALI_CPU_WRITEABLE | _MALI_CPU_READABLE | _MALI_PP_WRITEABLE | _MALI_PP_READABLE |_MALI_GP_READABLE | _MALI_GP_WRITEABLE
	},
	{
		.type = MALI400L2,
		.base = RK30_GPU_PHYS + 0x1000,
		.description = "Mali-400 L2 cache"
	},
};

#endif /* __ARCH_CONFIG_H__ */
