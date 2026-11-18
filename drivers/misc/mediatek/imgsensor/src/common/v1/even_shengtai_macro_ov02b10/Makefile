# SPDX-License-Identifier: GPL-2.0
IMGSENSOR_SUBDRV_OBJ := \
	$(subdrv-rpath)/even_shengtai_macro_ov02b10mipiraw_Sensor.o \

ifeq ($(CONFIG_MTK_IMGSENSOR_ISP4_T), m)
$(eval $(MTK_PLATFORM_KO)-objs += $(IMGSENSOR_SUBDRV_OBJ))
else ifeq ($(CONFIG_MTK_IMGSENSOR_ISP4_T), y)
obj-$(CONFIG_MTK_IMGSENSOR_ISP4_T) += $(IMGSENSOR_SUBDRV_OBJ)
endif
