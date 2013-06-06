#
# Makefile for RK2928 HDMI linux kernel module.
#

ccflags-$(CONFIG_RK_HDMI_DEBUG) = -DDEBUG -DHDMI_DEBUG -DHDCP_DEBUG

obj-$(CONFIG_HDMI_RK2928) += rk2928_hdmi_hw.o rk2928_hdmi.o
obj-$(CONFIG_HDCP_RK2928) += rk2928_hdmi_hdcp.o rk2928_hdcp.o
