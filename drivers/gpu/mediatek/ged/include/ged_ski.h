/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2020 MediaTek Inc.
 */

#ifndef __GED_SKI_H__
#define __GED_SKI_H__

#include <linux/kobject.h>
#include <linux/sysfs.h>

#include "ged_base.h"
#include "ged_sysfs.h"

GED_ERROR ged_ski_init(void);
void ged_ski_exit(void);

extern struct kobject *kernel_kobj;

#endif
