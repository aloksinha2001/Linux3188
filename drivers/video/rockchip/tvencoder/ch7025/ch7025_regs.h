#ifndef __CH7025_7026_REGS_H__
#define __CH7025_7026_REGS_H__

#define DEVICE_CH7025	0x55
#define DEVICE_CH7026	0x54

#define DEVICE_ID		0x00
#define REVISION_ID		0x01
#define DEVICE_RESET	0x02
#define PAGE_SELECT		0x03

#define PWR_STATE1		0x04
	#define m_PD_DAC		(7 << 3)
	#define m_PD_DIGITAL	(1 << 1)
	#define m_PD_PWR		(1 << 0)
	#define v_PD_DAC(n)		((n) << 3)
	#define v_PD_DIGITAL(n)	((n) << 1)
	#define v_PD_PWR(n)		((n) << 0)
	
#define SYNC_CONFIG		0x07
	#define v_DEPO_O(n)		(1 << 5)
	#define v_HPO_O(n)		(1 << 4)
	#define v_VPO_O(n)		(1 << 3)
	#define v_DEPO_I(n)		(1 << 2)
	#define v_HPO_I(n)		(1 << 1)
	#define v_VPO_I(n)		(1 << 0)
	
#define DAC_MAPS		0x0A
	enum {
		CVBS_SVIDEO = 0x0,
		DUAL_CVBS,
		TRIPLE_CVBS
	};
	#define v_DACS(n)		((n) << 3)
	#define v_DACSP(n)		(n)
	
#define OUT_FORMAT		0x0D
	#define v_WRFAST(n)			((n) << 7)
	#define v_YPBPR_ENABLE(n)	((n) << 6)
	#define v_YC2RGB(n)			((n) << 5)
	enum {
		NTSC_M = 0,
		NTSC_J,
		NTSC_443,
		PAL_B_D_G_H_I,
		PAL_M,
		PAL_N,
		PAL_Nc,
		PAL_60,
		VGA,
		VGA_BYPASS
	};
	#define v_FMT(n)			(n)
	
#define HTI_HAI_HIGH	0x0F
	#define v_HVAUTO(n)				((n) << 7)
	#define v_HREPT(n)				((n) << 6)
	#define v_HTI_HIGH(n)			((n) << 3)
	#define v_HAI_HIGH(n)			(n)
#define HAI_LOW			0x10
#define HTI_LOW			0x11
#define HW_HO_HIGH		0x12
	#define v_HDTVEN(n)			((n) << 7)
	#define v_EDGE_ENH(n)		((n) << 6)
	#define v_HW_HIGH(n)		((n) << 3)
	#define v_HO_HIGH(n)		(n)
#define HO_LOW			0x13
#define HW_LOW			0x14
#define VTI_VAI_HIGH	0x15
	#define v_FIELDSW(n)		((n) << 7)
	#define v_TVBYPSS(n)		((n) << 6)
	#define v_VTI_HIGH(n)		((n) << 3)
	#define v_VAI_HIGH(n)		(n)
#define VAI_LOW 		0x16
#define VTI_LOW			0x17
#define VW_VO_HIGH		0x18
	#define v_VW_HIGH(n)		((n) << 3)
	#define v_VO_HIGH(n)		(n)
#define VO_LOW			0x19
#define VW_LOW			0x1A

#define HTO_HAO_HIGH	0x1B
	#define v_HTO_HIGH(n)		((n) << 3)
	#define v_HAO_HIGH(n)		(n)
#define HAO_LOW			0x1C
#define HTO_LOW			0x1D
#define HWO_HOO_HIGH	0x1E
	#define v_HWO_HIGH(n)		((n) << 3)
	#define v_HOO_HIGH(n)		(n)
#define HOO_LOW			0x1F
#define HWO_LOW			0x20
#define VTO_VAO_HIGH	0x21
	#define v_MANUAL_DEFLIKER(n)	((n) << 6)
	#define v_VTO_HIGH(n)			((n) << 3)
	#define v_VAO_HIGH(n)			(n)
#define VAO_LOW			0x22
#define VTO_LOW			0x23
#define VWO_VOO_HIGH	0x24
	#define v_VWO_HIGH(n)			((n) << 3)
	#define v_VOO_HIGH(n)			(n)
#define VOO_LOW			0x25
#define VWO_LOW			0x26

#define DEFLIKER_FILER		0x3D
	#define v_MONOB(n)			((n) << 7)
	#define v_CHROM_BW(n)		((n) << 6)	// 1 for increase chroma bandwith
	#define v_FLIKER_LEVEL(n)	(n)
	
#define SUBCARRIER		0x41
	#define v_CRYSTAL_EAABLE(n)	((n) << 7)
	enum {
		CRYSTAL_3686400 = 0,
		CRYSTAL_3579545,
		CRYSTAL_4000000,
		CRYSTAL_12000000,
		CRYSTAL_13000000,
		CRYSTAL_13500000,
		CRYSTAL_14318000,
		CRYSTAL_14745600,
		CRYSTAL_16000000,
		CRYSTAL_18432000,
		CRYSTAL_20000000,
		CRYSTAL_26000000,
		CRYSTAL_27000000,
		CRYSTAL_32000000,
		CRYSTAL_40000000,
		CRYSTAL_49000000	
	};
	#define v_CRYSTAL(n)		((n) << 3)
	#define v_ACIV(n)			((n) << 2)

#define A1_31_24	0x4D
#define A1_23_16	0x4E
#define A1_15_8		0x4F
#define A1_7_0		0x50

#define A2			0x51

#define HDTVMODE	0x5C
	enum {
		HDTV_480P = 0,
		HDTV_480P_DOUBLE_SAMPLE,
		HDTV_576P,
		HDTV_576P_DOUBLE_SAMPLE,
		HDTV_720P_60HZ,
		HDTV_720P_50HZ,
		HDTV_1080I_60HZ,
		HDTV_1080I_50HZ_274M,
		HDTV_1080I_50HZ_295M
	};
	#define v_HDTVMODE(n)	((n) << 3)
#endif //__CH7025_7026_REGS_H__