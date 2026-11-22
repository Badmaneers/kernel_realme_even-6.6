#
# Copyright (C) 2015 MediaTek Inc.
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License version 2 as
# published by the Free Software Foundation.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
# GNU General Public License for more details.
#

# Connectivity combo driver
# If KERNELRELEASE is defined, we've been invoked from the
# kernel build system and can use its language.
ifneq ($(KERNELRELEASE),)
    subdir-ccflags-y += -I$(srctree)/
    subdir-ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/base/power/include
    subdir-ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/clkbuf/src
    subdir-ccflags-y += -Werror -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/include/mt-plat
    subdir-ccflags-y += -I$(srctree)/drivers/mmc/core
    subdir-ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/eccci/
    subdir-ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/clk/mediatek/
    subdir-ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/pinctrl/mediatek/
    subdir-ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/power_throttling/

    # Do Nothing, move to standalone repo
    MODULE_NAME := connadp
    obj-$(CONFIG_MTK_COMBO) += $(MODULE_NAME).o

    $(MODULE_NAME)-objs += common/connectivity_build_in_adapter.o
    $(MODULE_NAME)-objs += common/wmt_build_in_adapter.o
    $(MODULE_NAME)-objs += power_throttling/adapter.o
    $(MODULE_NAME)-objs += power_throttling/core.o
    ifeq ($(CONFIG_CONN_PWR_DEBUG),y)
        $(MODULE_NAME)-objs += power_throttling/test.o
    endif


    ifeq ($(CONFIG_MTK_COMBO), y)
        ccflags-y += -D CFG_CONNADP_BUILD_IN
    endif

    export CONFIG_MTK_COMBO_WIFI_HIF=axi
    export BT_PLATFORM=connac1x
    export MTK_COMBO_CHIP=CONNAC
    export WLAN_CHIP_ID=6768
    export CONNAC_VER=1_0
    export MTK_ANDROID_WMT=y
    export MTK_ANDROID_EMI=y
    export WIFI_IP_SET=1
    export WIFI_ECO_VER=1
    export MTK_WLAN_SERVICE=yes
    export CFG_FM_CHIP_ID=6768
    export CFG_FM_CHIP=mt6631

    ifeq ($(CONFIG_WLAN_DRV_BUILD_IN),y)
        subdir-ccflags-y += -D MTK_WCN_REMOVE_KERNEL_MODULE
        subdir-ccflags-y += -D MTK_WCN_BUILT_IN_DRIVER
    endif

    obj-$(CONFIG_MTK_COMBO) += connfem/
    obj-$(CONFIG_MTK_COMBO) += wmt_drv/
    obj-$(CONFIG_MTK_COMBO) += wlan/adaptor/
    obj-$(CONFIG_MTK_COMBO) += wlan/core/gen4m/
    obj-$(CONFIG_MTK_COMBO_BT) += bt/mt66xx/wmt/
    obj-$(CONFIG_MTK_COMBO_GPS) += gps/gps_stp/

# Otherwise we were called directly from the command line;
# invoke the kernel build system.
else
    KERNELDIR ?= /lib/modules/$(shell uname -r)/build
    PWD  := $(shell pwd)
default:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) modules
endif
