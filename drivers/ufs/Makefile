# SPDX-License-Identifier: GPL-2.0

# The link order is important here. ufshcd-core must initialize
# before vendor drivers.
obj-$(CONFIG_SCSI_UFSHCD)	+= core/ host/

# MediaTek / Oplus built-in device drivers
ifeq ($(CONFIG_DEVICE_MODULES_ALLOW_BUILTIN),y)
include $(src)/Makefile.mtk
endif
