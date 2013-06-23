# bcm40181
DHDCFLAGS = -Wall -Wstrict-prototypes -Dlinux -DBCMDRIVER                     \
	-DBCMDONGLEHOST -DUNRELEASEDCHIP -DBCMDMA32 -DWLBTAMP -DBCMFILEIMAGE  \
	-DDHDTHREAD -DDHD_GPL -DDHD_SCHED -DDHD_DEBUG -DBDC -DTOE \
	-DDHD_BCMEVENTS -DSHOW_EVENTS -DDONGLEOVERLAYS -DBCMDBG               \
	-DMMC_SDIO_ABORT -DBCMSDIO -DBCMLXSDMMC -DBCMPLATFORM_BUS -DWLP2P     \
	-DNEW_COMPAT_WIRELESS -DWIFI_ACT_FRAME -DARP_OFFLOAD_SUPPORT          \
	-DKEEP_ALIVE -DCSCAN  -DPKT_FILTER_SUPPORT     \
	-DEMBEDDED_PLATFORM  -DPNO_SUPPORT          \
	-Idrivers/net/wireless/bcm40181 -Idrivers/net/wireless/bcm40181/include

DHDOFILES = dhd_linux.o linux_osl.o bcmutils.o bcmevent.o dhd_common.o \
	    dhd_custom_gpio.o siutils.o sbutils.o aiutils.o hndpmu.o dhd_bta.o \
	    dhd_linux_sched.o bcmwifi.o dhd_cdc.o dhd_sdio.o \
	    bcmsdh_sdmmc.o bcmsdh.o bcmsdh_linux.o bcmsdh_sdmmc_linux.o \
	    wldev_common.o wl_android.o dhd_linux_mon.o

#-DDHD_DEBUG -DSDTEST -DANDROID_WIRELESS_PATCH -DWLP2P -DCONFIG_FIRST_SCAN WL_IW_USE_ISCAN CSCAN
#	-DCONFIG_PRESCANNED -DGET_CUSTOM_MAC_ENABLE -DENABLE_INSMOD_NO_FW_LOAD -DSDTEST -DSCAN_DBG
#	-DBCMSDIO -DBCMLXSDMMC -DBCMPLATFORM_BUS -DSDIO_ISR_THREAD -DCSCAN\
#	-DCUSTOMER_HW2 -DCUSTOM_OOB_GPIO_NUM=2 -DOOB_INTR_ONLY -DHW_OOB       \

obj-$(CONFIG_BCM40181) += bcm40181.o
bcm40181-objs += $(DHDOFILES)
ifneq ($(CONFIG_WIRELESS_EXT),)
bcm40181-objs += wl_iw.o
DHDCFLAGS += -DSOFTAP
endif
ifneq ($(CONFIG_CFG80211),)
bcm40181-objs += wl_cfg80211.o wl_cfgp2p.o dhd_linux_mon.o
DHDCFLAGS += -DWL_CFG80211
endif
EXTRA_CFLAGS = $(DHDCFLAGS)
ifeq ($(CONFIG_BCM40181),m)
EXTRA_LDFLAGS += --strip-debug
endif
