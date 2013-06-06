#
# Makefile for HDMI linux kernel module.
#

ccflags-$(CONFIG_RK_HDMI_DEBUG) = -DDEBUG -DHDMI_DEBUG

obj-$(CONFIG_RK_HDMI) += hdmi-core.o  hdmi-lcdc.o hdmi-edid.o hdmi-sysfs.o
obj-$(CONFIG_HDMI_RK30) += rk30/
obj-$(CONFIG_HDMI_RK2928) += rk2928/
obj-$(CONFIG_SII902XA) += sii902xa/
obj-$(CONFIG_IT66121) += it66121/
obj-$(CONFIG_RK_HDMI_GPIO_CEC) += softcec/