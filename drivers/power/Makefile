# SPDX-License-Identifier: GPL-2.0-only
obj-$(CONFIG_POWER_RESET)	+= reset/
obj-$(CONFIG_POWER_SUPPLY)	+= supply/

# MediaTek / Oplus built-in device drivers
ifeq ($(CONFIG_DEVICE_MODULES_ALLOW_BUILTIN),y)
include $(src)/Makefile.mtk
endif
