WiFi RK901 may be not stable
============================
put it in a folder with the kernel source /drivers/net/wireless/bcm40181 and add to the files in the folder /drivers/net/wireless/ following

Kconfig
=======
source "drivers/net/wireless/bcm40181/Kconfig"


Makefile
========
obj-$(CONFIG_BCM40181)   += bcm40181/



set through kernel config CONFIG_BCM40181=m
===========================================

my kernel options may need to compile the driver
================================================
CONFIG_CFG80211=m or y

CONFIG_USB_SUSPEND is not set

CONFIG_WIRELESS_EXT=y

CONFIG_WIFI_CONTROL_FUNC=y

CONFIG_PM_SLEEP=y

CONFIG_PM_SLEEP_SMP=y

CONFIG_MODULE_UNLOAD=y

CONFIG_HAS_WAKELOCK=y

CONFIG_HAS_EARLYSUSPEND=y

CONFIG_SYSCTL is not set

CONFIG_WLAN_80211=y
