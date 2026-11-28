// SPDX-License-Identifier: GPL-2.0
/*
 * GPU sysfs backend — table-driven, add nodes by editing gpu_nodes[]
 */

#include <mt-plat/mtk_gpu_utility.h>
#include <gpufreq_v2.h>
#include "ged_dvfs.h"
#include "ged_gpufreq_v2.h"
#include "ged_ski.h"

static struct kobject *gpu_kobj;

struct gpu_node {
	const char *name;
	umode_t mode;
	ssize_t (*show)(struct kobject *, struct kobj_attribute *, char *);
	ssize_t (*store)(struct kobject *, struct kobj_attribute *,
			const char *, size_t);
};

static ssize_t gpu_available_governor_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf);
static ssize_t gpu_busy_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf);
static ssize_t gpu_clock_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf);
static ssize_t gpu_freq_table_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf);
static ssize_t gpu_governor_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf);
static ssize_t gpu_max_freq_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf);
static ssize_t gpu_max_freq_store(struct kobject *kobj,
		struct kobj_attribute *attr, const char *buf, size_t count);
static ssize_t gpu_min_freq_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf);
static ssize_t gpu_min_freq_store(struct kobject *kobj,
		struct kobj_attribute *attr, const char *buf, size_t count);
static ssize_t gpu_model_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf);
static ssize_t gpu_temp_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf);

static const struct gpu_node gpu_nodes[] = {
	{ "gpu_available_governor", 0440, gpu_available_governor_show, NULL },
	{ "gpu_busy",               0440, gpu_busy_show,               NULL },
	{ "gpu_clock",              0440, gpu_clock_show,              NULL },
	{ "gpu_freq_table",         0440, gpu_freq_table_show,         NULL },
	{ "gpu_governor",           0440, gpu_governor_show,           NULL },
	{ "governor",               0440, gpu_governor_show,           NULL },
	{ "gpu_max_clock",          0660, gpu_max_freq_show,           gpu_max_freq_store },
	{ "gpu_min_clock",          0660, gpu_min_freq_show,           gpu_min_freq_store },
	{ "max_freq",               0660, gpu_max_freq_show,           gpu_max_freq_store },
	{ "min_freq",               0660, gpu_min_freq_show,           gpu_min_freq_store },
	{ "gpu_model",              0440, gpu_model_show,              NULL },
	{ "gpu_tmu",                0440, gpu_temp_show,               NULL },
};

static struct kobj_attribute gpu_attrs[ARRAY_SIZE(gpu_nodes)];

static ssize_t gpu_available_governor_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "Default\n");
}

static ssize_t gpu_busy_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	unsigned int gpu_loading = 0;

	mtk_get_gpu_loading(&gpu_loading);

	return scnprintf(buf, PAGE_SIZE, "%u\n", gpu_loading);
}

static ssize_t gpu_clock_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	unsigned int gpu_freq = ged_get_cur_freq();

	return scnprintf(buf, PAGE_SIZE, "%u\n", gpu_freq / 1000);
}

static ssize_t gpu_freq_table_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	int table_num = ged_get_opp_num();
	char temp[1024] = {0};
	int i;
	int pos = 0;

	for (i = 0; i < table_num && pos < (int)sizeof(temp); i++) {
		unsigned int freq_mhz = ged_get_freq_by_idx(i) / 1000;

		pos += scnprintf(temp + pos, sizeof(temp) - pos, "%u ", freq_mhz);
	}

	return scnprintf(buf, PAGE_SIZE, "%s\n", temp);
}

static ssize_t gpu_governor_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "Default\n");
}

static int ski_find_oppidx(int freq_input)
{
	int oppidx = ged_get_oppidx_by_freq((unsigned int)freq_input);

	if (oppidx < 0 && freq_input > 0 && freq_input < 10000)
		oppidx = ged_get_oppidx_by_freq((unsigned int)(freq_input * 1000));

	return oppidx;
}

static ssize_t gpu_max_freq_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	unsigned int ceil_freq = ged_get_freq_by_idx(ged_get_cur_limit_idx_ceil());
	unsigned int custom_freq = ged_get_freq_by_idx(ged_dvfs_get_custom_ceiling_gpu_freq());
	unsigned int max_clock = (custom_freq < ceil_freq) ? custom_freq : ceil_freq;

	return scnprintf(buf, PAGE_SIZE, "%u\n", max_clock / 1000);
}

static ssize_t gpu_max_freq_store(struct kobject *kobj,
		struct kobj_attribute *attr, const char *buf, size_t count)
{
	int max_freq = 0;
	int oppidx;
	char acBuffer[GED_SYSFS_MAX_BUFF_SIZE];

	if (count > 0 && count < GED_SYSFS_MAX_BUFF_SIZE &&
	    scnprintf(acBuffer, GED_SYSFS_MAX_BUFF_SIZE, "%s", buf) &&
	    kstrtoint(acBuffer, 0, &max_freq) == 0 && max_freq > 0) {
		oppidx = ski_find_oppidx(max_freq);
		if (oppidx >= 0) {
			mtk_custom_upbound_gpu_freq((unsigned int)oppidx);
			return count;
		}
		GED_LOGE("SKI: set max clock failed (%d not support)!\n", max_freq);
	}

	return -EINVAL;
}

static ssize_t gpu_min_freq_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	unsigned int floor_freq = ged_get_freq_by_idx(ged_get_cur_limit_idx_floor());
	unsigned int custom_freq = ged_get_freq_by_idx(ged_dvfs_get_custom_boost_gpu_freq());
	unsigned int min_clock = (custom_freq > floor_freq) ? custom_freq : floor_freq;

	return scnprintf(buf, PAGE_SIZE, "%u\n", min_clock / 1000);
}

static ssize_t gpu_min_freq_store(struct kobject *kobj,
		struct kobj_attribute *attr, const char *buf, size_t count)
{
	int min_freq = 0;
	int oppidx;
	char acBuffer[GED_SYSFS_MAX_BUFF_SIZE];

	if (count > 0 && count < GED_SYSFS_MAX_BUFF_SIZE &&
	    scnprintf(acBuffer, GED_SYSFS_MAX_BUFF_SIZE, "%s", buf) &&
	    kstrtoint(acBuffer, 0, &min_freq) == 0 && min_freq > 0) {
		oppidx = ski_find_oppidx(min_freq);
		if (oppidx >= 0) {
			mtk_custom_boost_gpu_freq((unsigned int)oppidx);
			return count;
		}
		GED_LOGE("SKI: set min clock failed (%d not support)!\n", min_freq);
	}

	return -EINVAL;
}

static ssize_t gpu_model_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
#if defined(CONFIG_MTK_GPU_MT6768_SUPPORT) || defined(CONFIG_MACH_MT6768)
	return scnprintf(buf, PAGE_SIZE, "Mali-G52 MC2\n");
#else
	return scnprintf(buf, PAGE_SIZE, "UNKNOWN\n");
#endif
}

#ifdef CONFIG_MTK_GPU_LEGACY
extern int get_immediate_gpu_wrap(void);
#endif

static ssize_t gpu_temp_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
#ifdef CONFIG_MTK_GPU_LEGACY
	int temperature = get_immediate_gpu_wrap();
#else
	int temperature = gpufreq_get_cur_temperature();
#endif

	return scnprintf(buf, PAGE_SIZE, "%d\n", temperature);
}

GED_ERROR ged_ski_init(void)
{
	int ret = GED_OK;
	int i;

	gpu_kobj = kobject_create_and_add("gpu", kernel_kobj);
	if (!gpu_kobj) {
		ret = GED_ERROR_OOM;
		GED_LOGE("ged: failed to create gpu_kobj!\n");
		goto EXIT;
	}

	for (i = 0; i < ARRAY_SIZE(gpu_nodes); i++) {
		memset(&gpu_attrs[i], 0, sizeof(gpu_attrs[i]));
		gpu_attrs[i].attr.name = gpu_nodes[i].name;
		gpu_attrs[i].attr.mode = gpu_nodes[i].mode;
		gpu_attrs[i].show = gpu_nodes[i].show;
		gpu_attrs[i].store = gpu_nodes[i].store;

		if (sysfs_create_file(gpu_kobj, &gpu_attrs[i].attr)) {
			GED_LOGE("ged: failed to create %s!\n", gpu_nodes[i].name);
			ret = GED_ERROR_FAIL;
			goto EXIT;
		}
	}

	return ret;

EXIT:
	ged_ski_exit();
	return ret;
}

void ged_ski_exit(void)
{
	int i;

	if (!gpu_kobj)
		return;

	for (i = 0; i < ARRAY_SIZE(gpu_nodes); i++)
		sysfs_remove_file(gpu_kobj, &gpu_attrs[i].attr);

	kobject_put(gpu_kobj);
	gpu_kobj = NULL;
}
