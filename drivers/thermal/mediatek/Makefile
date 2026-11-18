obj-$(CONFIG_MTK_SOC_THERMAL)	+= auxadc_thermal.o
obj-$(CONFIG_MTK_LVTS_THERMAL)	+= lvts_thermal.o

# MediaTek / Oplus built-in device drivers
ifeq ($(CONFIG_DEVICE_MODULES_ALLOW_BUILTIN),y)
include $(src)/Makefile.mtk
endif
