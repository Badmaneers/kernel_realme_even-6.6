# SPDX-License-Identifier: GPL-2.0
obj-$(CONFIG_DMABUF_HEAPS_DEFERRED_FREE) += deferred-free-helper.o
obj-$(CONFIG_DMABUF_HEAPS_PAGE_POOL)	+= page_pool.o
obj-$(CONFIG_DMABUF_HEAPS_SYSTEM)	+= system_heap.o
obj-$(CONFIG_DMABUF_HEAPS_CMA)		+= cma_heap.o

# MediaTek / Oplus built-in device drivers
ifeq ($(CONFIG_DEVICE_MODULES_ALLOW_BUILTIN),y)
include $(src)/Makefile.mtk
endif
