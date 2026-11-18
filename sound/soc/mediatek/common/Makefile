# SPDX-License-Identifier: GPL-2.0
# platform driver
snd-soc-mtk-common-objs := mtk-afe-platform-driver.o mtk-afe-fe-dai.o mtk-dsp-sof-common.o mtk-soundcard-driver.o
obj-$(CONFIG_SND_SOC_MEDIATEK) += snd-soc-mtk-common.o

obj-$(CONFIG_SND_SOC_MTK_BTCVSD) += mtk-btcvsd.o

# MediaTek / Oplus built-in device drivers
ifeq ($(CONFIG_DEVICE_MODULES_ALLOW_BUILTIN),y)
include $(src)/Makefile.mtk
endif
