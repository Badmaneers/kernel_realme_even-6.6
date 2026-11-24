#
# Makefile for kernel Silead Fingerprint drivers.
#

ifeq ($(CONFIG_OPLUS_FINGERPRINT_MTK),y)
	ccflags-y += -DBSP_SIL_PLAT_MTK
else ifeq ($(CONFIG_OPLUS_FINGERPRINT_QCOM),y)
	ccflags-y += -DBSP_SIL_PLAT_QCOM
endif

ifeq ($(CONFIG_OPLUS_FINGERPRINT_QCOM),y)
 obj-y	+= silead_fp_platform.o
else ifeq ($(CONFIG_OPLUS_FINGERPRINT_MTK),y)
obj-y	+= silead_fp.o
obj-$(CONFIG_SILEAD_FP_SPI)	+= silead_fp.o
endif




