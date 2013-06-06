##$_FOR_ROCKCHIP_RBOX_$
#
# Makefile for the input core drivers.
#

# Each configuration option enables a list of files.

obj-$(CONFIG_INPUT)		+= input-core.o
input-core-y := input.o input-compat.o input-mt.o ff-core.o

obj-$(CONFIG_INPUT_FF_MEMLESS)	+= ff-memless.o
obj-$(CONFIG_INPUT_POLLDEV)	+= input-polldev.o
obj-$(CONFIG_INPUT_SPARSEKMAP)	+= sparse-keymap.o

obj-$(CONFIG_INPUT_MOUSEDEV)	+= mousedev.o
obj-$(CONFIG_INPUT_JOYDEV)	+= joydev.o
obj-$(CONFIG_INPUT_EVDEV)	+= evdev.o
obj-$(CONFIG_INPUT_EVBUG)	+= evbug.o

obj-$(CONFIG_INPUT_KEYBOARD)	+= keyboard/
obj-$(CONFIG_INPUT_MOUSE)	+= mouse/
obj-$(CONFIG_INPUT_JOYSTICK)	+= joystick/
obj-$(CONFIG_INPUT_TABLET)	+= tablet/
obj-$(CONFIG_INPUT_TOUCHSCREEN)	+= touchscreen/
obj-$(CONFIG_INPUT_MISC)	+= misc/
obj-$(CONFIG_G_SENSOR_DEVICE)	+= gsensor/
obj-$(CONFIG_GYRO_SENSOR_DEVICE)	+= gyroscope/
obj-$(CONFIG_INPUT_JOGBALL)	+= jogball/
obj-$(CONFIG_LIGHT_SENSOR_DEVICE) += lightsensor/
obj-$(CONFIG_MAG_SENSORS)	+= magnetometer/

obj-$(CONFIG_SENSOR_DEVICE)	+= sensors/
#$_rbox_$_modify_$_huangzhibao_20120508: add remotectl
#$_rbox_$_modify_$_begin
obj-$(CONFIG_RK_REMOTECTL)      += remotectl/
#$_rbox_$_modify_$_end
obj-$(CONFIG_TS_AUTO)	+= ts/

obj-$(CONFIG_INPUT_APMPOWER)	+= apm-power.o
obj-$(CONFIG_INPUT_KEYRESET)	+= keyreset.o
