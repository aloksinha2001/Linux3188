#rtl8192c packed Makefile
# (gwl)

rk_cfg := wifi_power.o wifi_power_usb.o wifi_power_ops.o

rkcfg-y += $(rk_cfg)
ifeq ($(CONFIG_ARCH_RK29),y)
	obj-$(CONFIG_RTL8192CU) += 8192c_rk29.o rkcfg.o
else
	obj-$(CONFIG_RTL8192CU) += 8192c_rk30.o rkcfg.o
endif

.PHONY: clean

clean:
	find . -name '*.o*' -exec rm -f {} \; 
