// SPDX-License-Identifier: GPL-2.0-only
/*
 * Realme EVEN family (Realme C25, C25s, Narzo 50A / RMX3430 / MT6768)
 * Unified HD+ 720x1600 DSI Panel Driver
 *
 * Supports second-source panels:
 * 1. Truly ILI9882N (realme,even-truly-ili9882n / ilt9882n_truly_even_hdp_dsi_vdo_lcm)
 * 2. HLT ILI7807S   (realme,even-hlt-ili7807s   / ilt7807s_hlt_even_hdp_dsi_vdo_lcm)
 * 3. BOE NT36525B   (realme,even-boe-nt36525b   / nt36525b_hlt_even_boe_hdp_dsi_vdo_lcm)
 */

#include <linux/backlight.h>
#include <linux/delay.h>
#include <linux/gpio/consumer.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/regulator/consumer.h>

#include <video/mipi_display.h>

#include <drm/drm_connector.h>
#include <drm/drm_crtc.h>
#include <drm/drm_device.h>
#include <drm/drm_mipi_dsi.h>
#include <drm/drm_modes.h>
#include <drm/drm_panel.h>

#define CONFIG_MTK_PANEL_EXT
#include "mtk_panel_ext.h"
#include "mtk_drm_graphics_base.h"
#include <soc/oplus/device_info.h>

extern unsigned int oplus_display_brightness;
extern unsigned int oplus_max_normal_brightness;

/* Linux 6.6 multi-context compatibility macros */
struct mipi_dsi_multi_context {
	struct mipi_dsi_device *dsi;
	int accum_err;
};

#define mipi_dsi_dcs_write_seq_multi(ctx, seq...) \
	do { \
		static const u8 _d[] = { seq }; \
		if ((ctx)->accum_err) \
			break; \
		if (_d[0] < 0xb0) \
			(ctx)->accum_err = mipi_dsi_dcs_write_buffer((ctx)->dsi, _d, ARRAY_SIZE(_d)); \
		else \
			(ctx)->accum_err = mipi_dsi_generic_write((ctx)->dsi, _d, ARRAY_SIZE(_d)); \
	} while (0)

#define mipi_dsi_msleep(ctx, delay) \
	do { \
		if (!(ctx)->accum_err) \
			msleep(delay); \
	} while (0)

#define mipi_dsi_dcs_exit_sleep_mode_multi(ctx) \
	do { \
		if (!(ctx)->accum_err) \
			(ctx)->accum_err = mipi_dsi_dcs_exit_sleep_mode((ctx)->dsi); \
	} while (0)

#define mipi_dsi_dcs_set_display_on_multi(ctx) \
	do { \
		if (!(ctx)->accum_err) \
			(ctx)->accum_err = mipi_dsi_dcs_set_display_on((ctx)->dsi); \
	} while (0)

struct even_panel_desc;

struct lcm {
	struct device *dev;
	struct drm_panel panel;
	struct backlight_device *backlight;
	struct gpio_desc *reset_gpio;
	struct gpio_desc *bias_pos, *bias_neg;
	struct regulator *vddio;
	struct regulator *vsp;
	struct regulator *vsn;
	const struct even_panel_desc *desc;
	bool prepared;
	bool enabled;
};

struct even_panel_desc {
	const char *name;
	const char *vendor;
	const struct drm_display_mode *modes;
	unsigned int num_modes;
	unsigned int width_mm;
	unsigned int height_mm;
	unsigned int bpc;
	unsigned int lanes;
	enum mipi_dsi_pixel_format format;
	unsigned long mode_flags;
	int (*init)(struct mipi_dsi_multi_context *ctx);
	struct mtk_panel_params *ext_params;
};

static inline struct lcm *panel_to_lcm(struct drm_panel *panel)
{
	return container_of(panel, struct lcm, panel);
}

/* ==========================================================================
 * 1. Truly ILI9882N Initialization Sequence (HD+ 720x1600 @ 60Hz)
 * ========================================================================== */
static int truly_ili9882n_init(struct mipi_dsi_multi_context *ctx)
{
	mipi_dsi_dcs_write_seq_multi(ctx, 0xff, 0x98, 0x82, 0x01);
	mipi_dsi_dcs_write_seq_multi(ctx, 0x00, 0x47);
	mipi_dsi_dcs_write_seq_multi(ctx, 0x01, 0x32);
	mipi_dsi_dcs_write_seq_multi(ctx, 0x02, 0x00);
	mipi_dsi_dcs_write_seq_multi(ctx, 0x03, 0x00);
	mipi_dsi_dcs_write_seq_multi(ctx, 0x04, 0x04);
	mipi_dsi_dcs_write_seq_multi(ctx, 0x05, 0x32);
	mipi_dsi_dcs_write_seq_multi(ctx, 0x06, 0x00);
	mipi_dsi_dcs_write_seq_multi(ctx, 0x07, 0x00);
	mipi_dsi_dcs_write_seq_multi(ctx, 0x08, 0x85);
	mipi_dsi_dcs_write_seq_multi(ctx, 0x09, 0x04);
	mipi_dsi_dcs_write_seq_multi(ctx, 0x0a, 0x72);
	mipi_dsi_dcs_write_seq_multi(ctx, 0x0b, 0x00);
	mipi_dsi_dcs_write_seq_multi(ctx, 0x0c, 0x00);
	mipi_dsi_dcs_write_seq_multi(ctx, 0x0d, 0x00);
	mipi_dsi_dcs_write_seq_multi(ctx, 0x0e, 0x00);
	mipi_dsi_dcs_write_seq_multi(ctx, 0x0f, 0x00);
	mipi_dsi_dcs_write_seq_multi(ctx, 0x28, 0x48);
	mipi_dsi_dcs_write_seq_multi(ctx, 0x29, 0x88);
	mipi_dsi_dcs_write_seq_multi(ctx, 0x2a, 0x48);
	mipi_dsi_dcs_write_seq_multi(ctx, 0x2b, 0x88);

	mipi_dsi_dcs_write_seq_multi(ctx, 0x31, 0x0c);
	mipi_dsi_dcs_write_seq_multi(ctx, 0x32, 0x02);
	mipi_dsi_dcs_write_seq_multi(ctx, 0x33, 0x02);
	mipi_dsi_dcs_write_seq_multi(ctx, 0x34, 0x23);
	mipi_dsi_dcs_write_seq_multi(ctx, 0x35, 0x02);
	mipi_dsi_dcs_write_seq_multi(ctx, 0x36, 0x08);
	mipi_dsi_dcs_write_seq_multi(ctx, 0x37, 0x0a);
	mipi_dsi_dcs_write_seq_multi(ctx, 0x38, 0x06);
	mipi_dsi_dcs_write_seq_multi(ctx, 0x39, 0x06);
	mipi_dsi_dcs_write_seq_multi(ctx, 0x3a, 0x10);
	mipi_dsi_dcs_write_seq_multi(ctx, 0x3b, 0x10);
	mipi_dsi_dcs_write_seq_multi(ctx, 0x3c, 0x12);
	mipi_dsi_dcs_write_seq_multi(ctx, 0x3d, 0x12);
	mipi_dsi_dcs_write_seq_multi(ctx, 0x3e, 0x14);
	mipi_dsi_dcs_write_seq_multi(ctx, 0x3f, 0x14);
	mipi_dsi_dcs_write_seq_multi(ctx, 0x40, 0x16);
	mipi_dsi_dcs_write_seq_multi(ctx, 0x41, 0x16);
	mipi_dsi_dcs_write_seq_multi(ctx, 0x42, 0x07);
	mipi_dsi_dcs_write_seq_multi(ctx, 0x43, 0x07);
	mipi_dsi_dcs_write_seq_multi(ctx, 0x44, 0x07);
	mipi_dsi_dcs_write_seq_multi(ctx, 0x45, 0x07);
	mipi_dsi_dcs_write_seq_multi(ctx, 0x46, 0x07);

	mipi_dsi_dcs_write_seq_multi(ctx, 0x47, 0x0d);
	mipi_dsi_dcs_write_seq_multi(ctx, 0x48, 0x02);
	mipi_dsi_dcs_write_seq_multi(ctx, 0x49, 0x02);
	mipi_dsi_dcs_write_seq_multi(ctx, 0x4a, 0x23);
	mipi_dsi_dcs_write_seq_multi(ctx, 0x4b, 0x02);
	mipi_dsi_dcs_write_seq_multi(ctx, 0x4c, 0x09);
	mipi_dsi_dcs_write_seq_multi(ctx, 0x4d, 0x0b);
	mipi_dsi_dcs_write_seq_multi(ctx, 0x4e, 0x06);
	mipi_dsi_dcs_write_seq_multi(ctx, 0x4f, 0x06);
	mipi_dsi_dcs_write_seq_multi(ctx, 0x50, 0x11);
	mipi_dsi_dcs_write_seq_multi(ctx, 0x51, 0x11);
	mipi_dsi_dcs_write_seq_multi(ctx, 0x52, 0x13);
	mipi_dsi_dcs_write_seq_multi(ctx, 0x53, 0x13);
	mipi_dsi_dcs_write_seq_multi(ctx, 0x54, 0x15);
	mipi_dsi_dcs_write_seq_multi(ctx, 0x55, 0x15);
	mipi_dsi_dcs_write_seq_multi(ctx, 0x56, 0x17);
	mipi_dsi_dcs_write_seq_multi(ctx, 0x57, 0x17);
	mipi_dsi_dcs_write_seq_multi(ctx, 0x58, 0x07);
	mipi_dsi_dcs_write_seq_multi(ctx, 0x59, 0x07);
	mipi_dsi_dcs_write_seq_multi(ctx, 0x5a, 0x07);
	mipi_dsi_dcs_write_seq_multi(ctx, 0x5b, 0x07);
	mipi_dsi_dcs_write_seq_multi(ctx, 0x5c, 0x07);

	mipi_dsi_dcs_write_seq_multi(ctx, 0x61, 0x0c);
	mipi_dsi_dcs_write_seq_multi(ctx, 0x62, 0x02);
	mipi_dsi_dcs_write_seq_multi(ctx, 0x63, 0x02);
	mipi_dsi_dcs_write_seq_multi(ctx, 0x64, 0x23);
	mipi_dsi_dcs_write_seq_multi(ctx, 0x65, 0x02);
	mipi_dsi_dcs_write_seq_multi(ctx, 0x66, 0x08);
	mipi_dsi_dcs_write_seq_multi(ctx, 0x67, 0x0a);
	mipi_dsi_dcs_write_seq_multi(ctx, 0x68, 0x06);
	mipi_dsi_dcs_write_seq_multi(ctx, 0x69, 0x06);
	mipi_dsi_dcs_write_seq_multi(ctx, 0x6a, 0x10);
	mipi_dsi_dcs_write_seq_multi(ctx, 0x6b, 0x10);
	mipi_dsi_dcs_write_seq_multi(ctx, 0x6c, 0x12);
	mipi_dsi_dcs_write_seq_multi(ctx, 0x6d, 0x12);
	mipi_dsi_dcs_write_seq_multi(ctx, 0x6e, 0x14);
	mipi_dsi_dcs_write_seq_multi(ctx, 0x6f, 0x14);
	mipi_dsi_dcs_write_seq_multi(ctx, 0x70, 0x16);
	mipi_dsi_dcs_write_seq_multi(ctx, 0x71, 0x16);
	mipi_dsi_dcs_write_seq_multi(ctx, 0x72, 0x07);
	mipi_dsi_dcs_write_seq_multi(ctx, 0x73, 0x07);
	mipi_dsi_dcs_write_seq_multi(ctx, 0x74, 0x07);
	mipi_dsi_dcs_write_seq_multi(ctx, 0x75, 0x07);
	mipi_dsi_dcs_write_seq_multi(ctx, 0x76, 0x07);

	mipi_dsi_dcs_write_seq_multi(ctx, 0x77, 0x0d);
	mipi_dsi_dcs_write_seq_multi(ctx, 0x78, 0x02);
	mipi_dsi_dcs_write_seq_multi(ctx, 0x79, 0x02);
	mipi_dsi_dcs_write_seq_multi(ctx, 0x7a, 0x23);
	mipi_dsi_dcs_write_seq_multi(ctx, 0x7b, 0x02);
	mipi_dsi_dcs_write_seq_multi(ctx, 0x7c, 0x09);
	mipi_dsi_dcs_write_seq_multi(ctx, 0x7d, 0x0b);
	mipi_dsi_dcs_write_seq_multi(ctx, 0x7e, 0x06);
	mipi_dsi_dcs_write_seq_multi(ctx, 0x7f, 0x06);
	mipi_dsi_dcs_write_seq_multi(ctx, 0x80, 0x11);
	mipi_dsi_dcs_write_seq_multi(ctx, 0x81, 0x11);
	mipi_dsi_dcs_write_seq_multi(ctx, 0x82, 0x13);
	mipi_dsi_dcs_write_seq_multi(ctx, 0x83, 0x13);
	mipi_dsi_dcs_write_seq_multi(ctx, 0x84, 0x15);
	mipi_dsi_dcs_write_seq_multi(ctx, 0x85, 0x15);
	mipi_dsi_dcs_write_seq_multi(ctx, 0x86, 0x17);
	mipi_dsi_dcs_write_seq_multi(ctx, 0x87, 0x17);
	mipi_dsi_dcs_write_seq_multi(ctx, 0x88, 0x07);
	mipi_dsi_dcs_write_seq_multi(ctx, 0x89, 0x07);
	mipi_dsi_dcs_write_seq_multi(ctx, 0x8a, 0x07);
	mipi_dsi_dcs_write_seq_multi(ctx, 0x8b, 0x07);
	mipi_dsi_dcs_write_seq_multi(ctx, 0x8c, 0x07);
	mipi_dsi_dcs_write_seq_multi(ctx, 0xb0, 0x33);
	mipi_dsi_dcs_write_seq_multi(ctx, 0xb1, 0x33);
	mipi_dsi_dcs_write_seq_multi(ctx, 0xb2, 0x00);
	mipi_dsi_dcs_write_seq_multi(ctx, 0xd0, 0x01);
	mipi_dsi_dcs_write_seq_multi(ctx, 0xd1, 0x00);
	mipi_dsi_dcs_write_seq_multi(ctx, 0xe2, 0x00);
	mipi_dsi_dcs_write_seq_multi(ctx, 0xe6, 0x22);
	mipi_dsi_dcs_write_seq_multi(ctx, 0xe7, 0x54);

	mipi_dsi_dcs_write_seq_multi(ctx, 0xff, 0x98, 0x82, 0x02);
	mipi_dsi_dcs_write_seq_multi(ctx, 0xf1, 0x1c);
	mipi_dsi_dcs_write_seq_multi(ctx, 0x4b, 0x5a);
	mipi_dsi_dcs_write_seq_multi(ctx, 0x50, 0xca);
	mipi_dsi_dcs_write_seq_multi(ctx, 0x51, 0x00);
	mipi_dsi_dcs_write_seq_multi(ctx, 0x06, 0x8f);
	mipi_dsi_dcs_write_seq_multi(ctx, 0x0b, 0xa0);
	mipi_dsi_dcs_write_seq_multi(ctx, 0x0c, 0x00);
	mipi_dsi_dcs_write_seq_multi(ctx, 0x0d, 0x14);
	mipi_dsi_dcs_write_seq_multi(ctx, 0x0e, 0xe6);
	mipi_dsi_dcs_write_seq_multi(ctx, 0x4e, 0x11);

	mipi_dsi_dcs_write_seq_multi(ctx, 0xff, 0x98, 0x82, 0x05);
	mipi_dsi_dcs_write_seq_multi(ctx, 0x03, 0x01);
	mipi_dsi_dcs_write_seq_multi(ctx, 0x04, 0x2c);
	mipi_dsi_dcs_write_seq_multi(ctx, 0x58, 0x61);
	mipi_dsi_dcs_write_seq_multi(ctx, 0x63, 0x8d);
	mipi_dsi_dcs_write_seq_multi(ctx, 0x64, 0x8d);
	mipi_dsi_dcs_write_seq_multi(ctx, 0x68, 0xa1);
	mipi_dsi_dcs_write_seq_multi(ctx, 0x69, 0xa7);
	mipi_dsi_dcs_write_seq_multi(ctx, 0x6a, 0x79);
	mipi_dsi_dcs_write_seq_multi(ctx, 0x6b, 0x6b);
	mipi_dsi_dcs_write_seq_multi(ctx, 0x85, 0x37);
	mipi_dsi_dcs_write_seq_multi(ctx, 0x46, 0x00);

	mipi_dsi_dcs_write_seq_multi(ctx, 0xff, 0x98, 0x82, 0x06);
	mipi_dsi_dcs_write_seq_multi(ctx, 0xd9, 0x10);
	mipi_dsi_dcs_write_seq_multi(ctx, 0xc0, 0x40);
	mipi_dsi_dcs_write_seq_multi(ctx, 0xc1, 0x16);
	mipi_dsi_dcs_write_seq_multi(ctx, 0x06, 0xa4);
	mipi_dsi_dcs_write_seq_multi(ctx, 0xd6, 0x55);
	mipi_dsi_dcs_write_seq_multi(ctx, 0xdd, 0x61);
	mipi_dsi_dcs_write_seq_multi(ctx, 0xdc, 0x68);
	mipi_dsi_dcs_write_seq_multi(ctx, 0x80, 0x00);
	mipi_dsi_dcs_write_seq_multi(ctx, 0x08, 0x00);

	mipi_dsi_dcs_write_seq_multi(ctx, 0xff, 0x98, 0x82, 0x08);
	mipi_dsi_dcs_write_seq_multi(ctx, 0xe0, 0x00, 0x24, 0x76, 0xab, 0xeb, 0x55, 0x20, 0x49,
				     0x79, 0xa0, 0xa9, 0xde, 0x11, 0x3d, 0x68, 0xea, 0x94, 0xc6,
				     0xe6, 0x0d, 0xff, 0x2c, 0x55, 0x85, 0xb5, 0x03, 0xec);
	mipi_dsi_dcs_write_seq_multi(ctx, 0xe1, 0x00, 0x24, 0x76, 0xab, 0xeb, 0x55, 0x20, 0x49,
				     0x79, 0xa0, 0xa9, 0xde, 0x11, 0x3d, 0x68, 0xea, 0x94, 0xc6,
				     0xe6, 0x0d, 0xff, 0x2c, 0x55, 0x85, 0xb5, 0x03, 0xec);

	mipi_dsi_dcs_write_seq_multi(ctx, 0xff, 0x98, 0x82, 0x0b);
	mipi_dsi_dcs_write_seq_multi(ctx, 0x9a, 0x44);
	mipi_dsi_dcs_write_seq_multi(ctx, 0x9b, 0x81);
	mipi_dsi_dcs_write_seq_multi(ctx, 0x9c, 0x03);
	mipi_dsi_dcs_write_seq_multi(ctx, 0x9d, 0x03);
	mipi_dsi_dcs_write_seq_multi(ctx, 0x9e, 0x70);
	mipi_dsi_dcs_write_seq_multi(ctx, 0x9f, 0x70);
	mipi_dsi_dcs_write_seq_multi(ctx, 0xab, 0xe0);

	mipi_dsi_dcs_write_seq_multi(ctx, 0xff, 0x98, 0x82, 0x0e);
	mipi_dsi_dcs_write_seq_multi(ctx, 0x11, 0x10);
	mipi_dsi_dcs_write_seq_multi(ctx, 0x13, 0x10);
	mipi_dsi_dcs_write_seq_multi(ctx, 0x00, 0xa0);

	mipi_dsi_dcs_write_seq_multi(ctx, 0xff, 0x98, 0x82, 0x03);
	mipi_dsi_dcs_write_seq_multi(ctx, 0x81, 0x14);
	mipi_dsi_dcs_write_seq_multi(ctx, 0x82, 0x15);
	mipi_dsi_dcs_write_seq_multi(ctx, 0x83, 0x30);
	mipi_dsi_dcs_write_seq_multi(ctx, 0x84, 0x00);
	mipi_dsi_dcs_write_seq_multi(ctx, 0x88, 0xcc);
	mipi_dsi_dcs_write_seq_multi(ctx, 0x89, 0xe5);
	mipi_dsi_dcs_write_seq_multi(ctx, 0x8a, 0xed);
	mipi_dsi_dcs_write_seq_multi(ctx, 0x8b, 0xef);
	mipi_dsi_dcs_write_seq_multi(ctx, 0xb3, 0xe5);
	mipi_dsi_dcs_write_seq_multi(ctx, 0xac, 0xfa);
	mipi_dsi_dcs_write_seq_multi(ctx, 0xad, 0xe0);
	mipi_dsi_dcs_write_seq_multi(ctx, 0x8c, 0x9b);
	mipi_dsi_dcs_write_seq_multi(ctx, 0x8d, 0xa9);
	mipi_dsi_dcs_write_seq_multi(ctx, 0x8e, 0xad);
	mipi_dsi_dcs_write_seq_multi(ctx, 0x8f, 0xaf);
	mipi_dsi_dcs_write_seq_multi(ctx, 0x90, 0xb2);
	mipi_dsi_dcs_write_seq_multi(ctx, 0x91, 0xbc);
	mipi_dsi_dcs_write_seq_multi(ctx, 0x92, 0xbd);
	mipi_dsi_dcs_write_seq_multi(ctx, 0x93, 0xba);
	mipi_dsi_dcs_write_seq_multi(ctx, 0x94, 0xca);
	mipi_dsi_dcs_write_seq_multi(ctx, 0x95, 0xd5);
	mipi_dsi_dcs_write_seq_multi(ctx, 0xb4, 0xd9);
	mipi_dsi_dcs_write_seq_multi(ctx, 0xae, 0xd3);
	mipi_dsi_dcs_write_seq_multi(ctx, 0x96, 0x70);
	mipi_dsi_dcs_write_seq_multi(ctx, 0x97, 0x89);
	mipi_dsi_dcs_write_seq_multi(ctx, 0x98, 0x99);
	mipi_dsi_dcs_write_seq_multi(ctx, 0x99, 0x9f);
	mipi_dsi_dcs_write_seq_multi(ctx, 0x9a, 0xa3);
	mipi_dsi_dcs_write_seq_multi(ctx, 0x9b, 0xa6);
	mipi_dsi_dcs_write_seq_multi(ctx, 0x9c, 0xa9);
	mipi_dsi_dcs_write_seq_multi(ctx, 0x9d, 0xad);
	mipi_dsi_dcs_write_seq_multi(ctx, 0x9e, 0xc2);
	mipi_dsi_dcs_write_seq_multi(ctx, 0x9f, 0xcc);
	mipi_dsi_dcs_write_seq_multi(ctx, 0xb5, 0xd0);
	mipi_dsi_dcs_write_seq_multi(ctx, 0xa0, 0xc0);
	mipi_dsi_dcs_write_seq_multi(ctx, 0xa1, 0x7e);
	mipi_dsi_dcs_write_seq_multi(ctx, 0xa2, 0x7e);
	mipi_dsi_dcs_write_seq_multi(ctx, 0xa7, 0xe5);
	mipi_dsi_dcs_write_seq_multi(ctx, 0xa8, 0xe5);
	mipi_dsi_dcs_write_seq_multi(ctx, 0xb6, 0x00);
	mipi_dsi_dcs_write_seq_multi(ctx, 0xb7, 0xd0);
	mipi_dsi_dcs_write_seq_multi(ctx, 0xb8, 0xd0);
	mipi_dsi_dcs_write_seq_multi(ctx, 0xb1, 0x66);
	mipi_dsi_dcs_write_seq_multi(ctx, 0xb2, 0x66);
	mipi_dsi_dcs_write_seq_multi(ctx, 0x86, 0x6c);
	mipi_dsi_dcs_write_seq_multi(ctx, 0x87, 0x6c);

	mipi_dsi_dcs_write_seq_multi(ctx, 0xff, 0x98, 0x82, 0x03);
	mipi_dsi_dcs_write_seq_multi(ctx, 0xac, 0xfa);
	mipi_dsi_dcs_write_seq_multi(ctx, 0xff, 0x98, 0x82, 0x00);
	mipi_dsi_dcs_write_seq_multi(ctx, 0x55, 0x01);

	mipi_dsi_dcs_write_seq_multi(ctx, 0xff, 0x98, 0x82, 0x00);
	mipi_dsi_dcs_write_seq_multi(ctx, 0x51, 0x00, 0x00);
	mipi_dsi_dcs_write_seq_multi(ctx, 0x53, 0x24);
	mipi_dsi_dcs_write_seq_multi(ctx, 0x5e, 0x00, 0x50);
	mipi_dsi_dcs_write_seq_multi(ctx, 0x35, 0x00); /* TE */

	mipi_dsi_dcs_exit_sleep_mode_multi(ctx);
	mipi_dsi_msleep(ctx, 120);

	mipi_dsi_dcs_set_display_on_multi(ctx);
	mipi_dsi_msleep(ctx, 20);

	return ctx->accum_err;
}

/* ==========================================================================
 * 2. HLT ILI7807S Initialization Sequence (HD+ 720x1600 @ 60Hz)
 * ========================================================================== */
static int hlt_ili7807s_init(struct mipi_dsi_multi_context *ctx)
{
	mipi_dsi_dcs_write_seq_multi(ctx, 0xff, 0x78, 0x07, 0x03);
	mipi_dsi_dcs_write_seq_multi(ctx, 0x83, 0x20);
	mipi_dsi_dcs_write_seq_multi(ctx, 0x84, 0x01);
	mipi_dsi_dcs_write_seq_multi(ctx, 0x86, 0x3f);
	mipi_dsi_dcs_write_seq_multi(ctx, 0x87, 0x3f);
	mipi_dsi_dcs_write_seq_multi(ctx, 0xaf, 0x18);
	mipi_dsi_dcs_write_seq_multi(ctx, 0x88, 0xe3);
	mipi_dsi_dcs_write_seq_multi(ctx, 0x89, 0xea);
	mipi_dsi_dcs_write_seq_multi(ctx, 0x8a, 0xeb);
	mipi_dsi_dcs_write_seq_multi(ctx, 0x8b, 0xf6);
	mipi_dsi_dcs_write_seq_multi(ctx, 0xb3, 0xf9);
	mipi_dsi_dcs_write_seq_multi(ctx, 0xac, 0xff);
	mipi_dsi_dcs_write_seq_multi(ctx, 0xad, 0xff);
	mipi_dsi_dcs_write_seq_multi(ctx, 0x8c, 0xc0);
	mipi_dsi_dcs_write_seq_multi(ctx, 0x8d, 0xc2);
	mipi_dsi_dcs_write_seq_multi(ctx, 0x8e, 0xc6);
	mipi_dsi_dcs_write_seq_multi(ctx, 0x8f, 0xc8);
	mipi_dsi_dcs_write_seq_multi(ctx, 0x90, 0xd1);
	mipi_dsi_dcs_write_seq_multi(ctx, 0x91, 0xd6);
	mipi_dsi_dcs_write_seq_multi(ctx, 0x92, 0xd8);
	mipi_dsi_dcs_write_seq_multi(ctx, 0x93, 0xd1);
	mipi_dsi_dcs_write_seq_multi(ctx, 0x94, 0xe8);
	mipi_dsi_dcs_write_seq_multi(ctx, 0x95, 0xf6);
	mipi_dsi_dcs_write_seq_multi(ctx, 0xb4, 0xe8);

	mipi_dsi_dcs_write_seq_multi(ctx, 0xff, 0x78, 0x07, 0x00);
	mipi_dsi_dcs_write_seq_multi(ctx, 0x51, 0x00, 0x00);
	mipi_dsi_dcs_write_seq_multi(ctx, 0x53, 0x2c);
	mipi_dsi_dcs_write_seq_multi(ctx, 0x55, 0x01);

	mipi_dsi_dcs_exit_sleep_mode_multi(ctx);
	mipi_dsi_msleep(ctx, 120);

	mipi_dsi_dcs_write_seq_multi(ctx, 0xff, 0x78, 0x07, 0x0e);
	mipi_dsi_dcs_write_seq_multi(ctx, 0xb0, 0x01);

	mipi_dsi_dcs_write_seq_multi(ctx, 0xff, 0x78, 0x07, 0x06);
	mipi_dsi_dcs_write_seq_multi(ctx, 0xdd, 0x17);
	mipi_dsi_dcs_write_seq_multi(ctx, 0x08, 0x20);

	mipi_dsi_dcs_write_seq_multi(ctx, 0xff, 0x78, 0x07, 0x05);
	mipi_dsi_dcs_write_seq_multi(ctx, 0x7b, 0x88);
	mipi_dsi_dcs_write_seq_multi(ctx, 0x7c, 0x79);

	mipi_dsi_dcs_write_seq_multi(ctx, 0xff, 0x78, 0x07, 0x08);
	mipi_dsi_dcs_write_seq_multi(ctx, 0xe0, 0x00, 0x00, 0x1b, 0x45, 0x00, 0x84, 0xb1, 0xd5,
				     0x15, 0x0d, 0x38, 0x7a, 0x25, 0xaa, 0xf3, 0x2f, 0x2a,
				     0x68, 0xa8, 0xd1, 0x3f, 0x04, 0x28, 0x52, 0x3f, 0x6f,
				     0x91, 0xbd, 0x0f, 0xd8, 0xd8);
	mipi_dsi_dcs_write_seq_multi(ctx, 0xe1, 0x00, 0x00, 0x1b, 0x45, 0x00, 0x84, 0xb1, 0xd5,
				     0x15, 0x0d, 0x38, 0x7a, 0x25, 0xaa, 0xf3, 0x2f, 0x2a,
				     0x68, 0xa8, 0xd1, 0x3f, 0x04, 0x28, 0x52, 0x3f, 0x6f,
				     0x91, 0xbd, 0x0f, 0xd8, 0xd8);

	mipi_dsi_dcs_write_seq_multi(ctx, 0xff, 0x78, 0x07, 0x00);
	mipi_dsi_dcs_set_display_on_multi(ctx);
	mipi_dsi_msleep(ctx, 20);
	mipi_dsi_dcs_write_seq_multi(ctx, 0x35, 0x00); /* TE */

	return ctx->accum_err;
}

/* ==========================================================================
 * 3. BOE / HLT NT36525B Initialization Sequence (HD+ 720x1600 @ 60Hz)
 * ========================================================================== */
static int boe_nt36525b_init(struct mipi_dsi_multi_context *ctx)
{
	/* CMD2_Page0 */
	mipi_dsi_dcs_write_seq_multi(ctx, 0xff, 0x20);
	mipi_dsi_dcs_write_seq_multi(ctx, 0xfb, 0x01);

	/* R(+) */
	mipi_dsi_dcs_write_seq_multi(ctx, 0xb0, 0x00, 0x04, 0x00, 0x16, 0x00, 0x2e, 0x00, 0x43,
				     0x00, 0x55, 0x00, 0x67, 0x00, 0x76, 0x00, 0x86);
	mipi_dsi_dcs_write_seq_multi(ctx, 0xb1, 0x00, 0x94, 0x00, 0xc7, 0x00, 0xf0, 0x01, 0x33,
				     0x01, 0x65, 0x01, 0xb7, 0x01, 0xf7, 0x01, 0xfa);
	mipi_dsi_dcs_write_seq_multi(ctx, 0xb2, 0x02, 0x39, 0x02, 0x7e, 0x02, 0xae, 0x02, 0xe5,
				     0x03, 0x0e, 0x03, 0x3b, 0x03, 0x4c, 0x03, 0x5b);
	mipi_dsi_dcs_write_seq_multi(ctx, 0xb3, 0x03, 0x6e, 0x03, 0x85, 0x03, 0x91, 0x03, 0xc2,
				     0x03, 0xda, 0x03, 0xda);

	/* G(+) */
	mipi_dsi_dcs_write_seq_multi(ctx, 0xb4, 0x00, 0x04, 0x00, 0x16, 0x00, 0x2e, 0x00, 0x43,
				     0x00, 0x55, 0x00, 0x67, 0x00, 0x76, 0x00, 0x86);
	mipi_dsi_dcs_write_seq_multi(ctx, 0xb5, 0x00, 0x94, 0x00, 0xc7, 0x00, 0xf0, 0x01, 0x33,
				     0x01, 0x65, 0x01, 0xb7, 0x01, 0xf7, 0x01, 0xfa);
	mipi_dsi_dcs_write_seq_multi(ctx, 0xb6, 0x02, 0x39, 0x02, 0x7e, 0x02, 0xae, 0x02, 0xe5,
				     0x03, 0x0e, 0x03, 0x3b, 0x03, 0x4c, 0x03, 0x5b);
	mipi_dsi_dcs_write_seq_multi(ctx, 0xb7, 0x03, 0x6e, 0x03, 0x85, 0x03, 0x91, 0x03, 0xc2,
				     0x03, 0xda, 0x03, 0xda);

	/* B(+) */
	mipi_dsi_dcs_write_seq_multi(ctx, 0xb8, 0x00, 0x04, 0x00, 0x16, 0x00, 0x2e, 0x00, 0x43,
				     0x00, 0x55, 0x00, 0x67, 0x00, 0x76, 0x00, 0x86);
	mipi_dsi_dcs_write_seq_multi(ctx, 0xb9, 0x00, 0x94, 0x00, 0xc7, 0x00, 0xf0, 0x01, 0x33,
				     0x01, 0x65, 0x01, 0xb7, 0x01, 0xf7, 0x01, 0xfa);
	mipi_dsi_dcs_write_seq_multi(ctx, 0xba, 0x02, 0x39, 0x02, 0x7e, 0x02, 0xae, 0x02, 0xe5,
				     0x03, 0x0e, 0x03, 0x3b, 0x03, 0x4c, 0x03, 0x5b);
	mipi_dsi_dcs_write_seq_multi(ctx, 0xbb, 0x03, 0x6e, 0x03, 0x85, 0x03, 0x91, 0x03, 0xc2,
				     0x03, 0xda, 0x03, 0xda);

	/* CMD2_Page1 */
	mipi_dsi_dcs_write_seq_multi(ctx, 0xff, 0x21);
	mipi_dsi_dcs_write_seq_multi(ctx, 0xfb, 0x01);

	/* R(-) */
	mipi_dsi_dcs_write_seq_multi(ctx, 0xb0, 0x00, 0x04, 0x00, 0x0e, 0x00, 0x26, 0x00, 0x3b,
				     0x00, 0x4d, 0x00, 0x5f, 0x00, 0x6e, 0x00, 0x7e);
	mipi_dsi_dcs_write_seq_multi(ctx, 0xb1, 0x00, 0x8c, 0x00, 0xbf, 0x00, 0xe8, 0x01, 0x2b,
				     0x01, 0x5d, 0x01, 0xaf, 0x01, 0xef, 0x01, 0xf2);
	mipi_dsi_dcs_write_seq_multi(ctx, 0xb2, 0x02, 0x31, 0x02, 0x76, 0x02, 0xa6, 0x02, 0xdd,
				     0x03, 0x06, 0x03, 0x33, 0x03, 0x44, 0x03, 0x53);
	mipi_dsi_dcs_write_seq_multi(ctx, 0xb3, 0x03, 0x66, 0x03, 0x7d, 0x03, 0x89, 0x03, 0xba,
				     0x03, 0xd2, 0x03, 0xda);

	/* G(-) */
	mipi_dsi_dcs_write_seq_multi(ctx, 0xb4, 0x00, 0x04, 0x00, 0x0e, 0x00, 0x26, 0x00, 0x3b,
				     0x00, 0x4d, 0x00, 0x5f, 0x00, 0x6e, 0x00, 0x7e);
	mipi_dsi_dcs_write_seq_multi(ctx, 0xb5, 0x00, 0x8c, 0x00, 0xbf, 0x00, 0xe8, 0x01, 0x2b,
				     0x01, 0x5d, 0x01, 0xaf, 0x01, 0xef, 0x01, 0xf2);
	mipi_dsi_dcs_write_seq_multi(ctx, 0xb6, 0x02, 0x31, 0x02, 0x76, 0x02, 0xa6, 0x02, 0xdd,
				     0x03, 0x06, 0x03, 0x33, 0x03, 0x44, 0x03, 0x53);
	mipi_dsi_dcs_write_seq_multi(ctx, 0xb7, 0x03, 0x66, 0x03, 0x7d, 0x03, 0x89, 0x03, 0xba,
				     0x03, 0xd2, 0x03, 0xda);

	/* B(-) */
	mipi_dsi_dcs_write_seq_multi(ctx, 0xb8, 0x00, 0x04, 0x00, 0x0e, 0x00, 0x26, 0x00, 0x3b,
				     0x00, 0x4d, 0x00, 0x5f, 0x00, 0x6e, 0x00, 0x7e);
	mipi_dsi_dcs_write_seq_multi(ctx, 0xb9, 0x00, 0x8c, 0x00, 0xbf, 0x00, 0xe8, 0x01, 0x2b,
				     0x01, 0x5d, 0x01, 0xaf, 0x01, 0xef, 0x01, 0xf2);
	mipi_dsi_dcs_write_seq_multi(ctx, 0xba, 0x02, 0x31, 0x02, 0x76, 0x02, 0xa6, 0x02, 0xdd,
				     0x03, 0x06, 0x03, 0x33, 0x03, 0x44, 0x03, 0x53);
	mipi_dsi_dcs_write_seq_multi(ctx, 0xbb, 0x03, 0x66, 0x03, 0x7d, 0x03, 0x89, 0x03, 0xba,
				     0x03, 0xd2, 0x03, 0xda);

	/* CABC Setting update */
	mipi_dsi_dcs_write_seq_multi(ctx, 0xff, 0x23);
	mipi_dsi_msleep(ctx, 1);
	mipi_dsi_dcs_write_seq_multi(ctx, 0xfb, 0x01);
	mipi_dsi_dcs_write_seq_multi(ctx, 0x00, 0x68);
	mipi_dsi_dcs_write_seq_multi(ctx, 0x07, 0x00);
	mipi_dsi_dcs_write_seq_multi(ctx, 0x08, 0x01);
	mipi_dsi_dcs_write_seq_multi(ctx, 0x10, 0x20);
	mipi_dsi_dcs_write_seq_multi(ctx, 0x11, 0x00);
	mipi_dsi_dcs_write_seq_multi(ctx, 0x12, 0xb4);
	mipi_dsi_dcs_write_seq_multi(ctx, 0x15, 0xe9);
	mipi_dsi_dcs_write_seq_multi(ctx, 0x16, 0x0b);
	mipi_dsi_dcs_write_seq_multi(ctx, 0x29, 0x20);
	mipi_dsi_dcs_write_seq_multi(ctx, 0x2a, 0x20);
	mipi_dsi_dcs_write_seq_multi(ctx, 0x2b, 0x30);
	mipi_dsi_dcs_write_seq_multi(ctx, 0x30, 0xfa);
	mipi_dsi_dcs_write_seq_multi(ctx, 0x31, 0xf7);
	mipi_dsi_dcs_write_seq_multi(ctx, 0x32, 0xf0);
	mipi_dsi_dcs_write_seq_multi(ctx, 0x33, 0xed);
	mipi_dsi_dcs_write_seq_multi(ctx, 0x34, 0xec);
	mipi_dsi_dcs_write_seq_multi(ctx, 0x35, 0xeb);
	mipi_dsi_dcs_write_seq_multi(ctx, 0x36, 0xea);
	mipi_dsi_dcs_write_seq_multi(ctx, 0x37, 0xe9);
	mipi_dsi_dcs_write_seq_multi(ctx, 0x38, 0xe3);
	mipi_dsi_dcs_write_seq_multi(ctx, 0x39, 0xe0);
	mipi_dsi_dcs_write_seq_multi(ctx, 0x3a, 0xdb);
	mipi_dsi_dcs_write_seq_multi(ctx, 0x3b, 0xd7);
	mipi_dsi_dcs_write_seq_multi(ctx, 0x3d, 0xd5);
	mipi_dsi_dcs_write_seq_multi(ctx, 0x3f, 0xd2);
	mipi_dsi_dcs_write_seq_multi(ctx, 0x40, 0xcf);
	mipi_dsi_dcs_write_seq_multi(ctx, 0x41, 0xcc);
	mipi_dsi_dcs_write_seq_multi(ctx, 0x45, 0xf2);
	mipi_dsi_dcs_write_seq_multi(ctx, 0x46, 0xed);
	mipi_dsi_dcs_write_seq_multi(ctx, 0x47, 0xe0);
	mipi_dsi_dcs_write_seq_multi(ctx, 0x48, 0xdd);
	mipi_dsi_dcs_write_seq_multi(ctx, 0x49, 0xd5);
	mipi_dsi_dcs_write_seq_multi(ctx, 0x4a, 0xd0);
	mipi_dsi_dcs_write_seq_multi(ctx, 0x4b, 0xc8);
	mipi_dsi_dcs_write_seq_multi(ctx, 0x4c, 0xc5);
	mipi_dsi_dcs_write_seq_multi(ctx, 0x4d, 0xc0);
	mipi_dsi_dcs_write_seq_multi(ctx, 0x4e, 0xbd);
	mipi_dsi_dcs_write_seq_multi(ctx, 0x4f, 0xbb);
	mipi_dsi_dcs_write_seq_multi(ctx, 0x50, 0xba);
	mipi_dsi_dcs_write_seq_multi(ctx, 0x51, 0xb5);
	mipi_dsi_dcs_write_seq_multi(ctx, 0x52, 0xb0);
	mipi_dsi_dcs_write_seq_multi(ctx, 0x53, 0xad);
	mipi_dsi_dcs_write_seq_multi(ctx, 0x54, 0xa5);
	mipi_dsi_dcs_write_seq_multi(ctx, 0x58, 0xd4);
	mipi_dsi_dcs_write_seq_multi(ctx, 0x59, 0xd0);
	mipi_dsi_dcs_write_seq_multi(ctx, 0x5a, 0xc6);
	mipi_dsi_dcs_write_seq_multi(ctx, 0x5b, 0xbf);
	mipi_dsi_dcs_write_seq_multi(ctx, 0x5c, 0xba);
	mipi_dsi_dcs_write_seq_multi(ctx, 0x5d, 0xb1);
	mipi_dsi_dcs_write_seq_multi(ctx, 0x5e, 0xb0);
	mipi_dsi_dcs_write_seq_multi(ctx, 0x5f, 0xaf);
	mipi_dsi_dcs_write_seq_multi(ctx, 0x60, 0xa2);
	mipi_dsi_dcs_write_seq_multi(ctx, 0x61, 0xa0);
	mipi_dsi_dcs_write_seq_multi(ctx, 0x62, 0x99);
	mipi_dsi_dcs_write_seq_multi(ctx, 0x63, 0x93);
	mipi_dsi_dcs_write_seq_multi(ctx, 0x64, 0x8f);
	mipi_dsi_dcs_write_seq_multi(ctx, 0x65, 0x85);
	mipi_dsi_dcs_write_seq_multi(ctx, 0x66, 0x80);
	mipi_dsi_dcs_write_seq_multi(ctx, 0x67, 0x70);
	mipi_dsi_dcs_write_seq_multi(ctx, 0x04, 0x06);
	mipi_dsi_dcs_write_seq_multi(ctx, 0x05, 0x36);
	mipi_dsi_dcs_write_seq_multi(ctx, 0x06, 0x02);

	mipi_dsi_dcs_write_seq_multi(ctx, 0xff, 0x20);
	mipi_dsi_msleep(ctx, 1);
	mipi_dsi_dcs_write_seq_multi(ctx, 0xfb, 0x01);
	mipi_dsi_dcs_write_seq_multi(ctx, 0x07, 0x48);
	mipi_dsi_dcs_write_seq_multi(ctx, 0x08, 0xdc);

	mipi_dsi_dcs_write_seq_multi(ctx, 0xff, 0x10);
	mipi_dsi_dcs_write_seq_multi(ctx, 0xfb, 0x01);
	mipi_dsi_dcs_write_seq_multi(ctx, 0xba, 0x02);
	mipi_dsi_dcs_write_seq_multi(ctx, 0x68, 0x02, 0x01);
	mipi_dsi_dcs_write_seq_multi(ctx, 0x53, 0x24);
	mipi_dsi_dcs_write_seq_multi(ctx, 0x55, 0x01);
	mipi_dsi_dcs_write_seq_multi(ctx, 0x35, 0x00); /* TE */

	mipi_dsi_dcs_exit_sleep_mode_multi(ctx);
	mipi_dsi_msleep(ctx, 120);

	mipi_dsi_dcs_set_display_on_multi(ctx);
	mipi_dsi_msleep(ctx, 20);

	return ctx->accum_err;
}

/* ==========================================================================
 * Display Modes and Timing Definitions (720x1600 @ 60Hz)
 * ========================================================================== */
static const struct drm_display_mode truly_ili9882n_modes[] = {
	{
		.clock = 87066,
		.hdisplay = 720,
		.hsync_start = 747,
		.hsync_end = 755,
		.htotal = 781,
		.vdisplay = 1600,
		.vsync_start = 1840,
		.vsync_end = 1842,
		.vtotal = 1858,
		.width_mm = 68,
		.height_mm = 151,
		.type = DRM_MODE_TYPE_DRIVER | DRM_MODE_TYPE_PREFERRED,
	},
};

static const struct drm_display_mode hlt_ili7807s_modes[] = {
	{
		.clock = 86724,
		.hdisplay = 720,
		.hsync_start = 800,
		.hsync_end = 812,
		.htotal = 876,
		.vdisplay = 1600,
		.vsync_start = 1632,
		.vsync_end = 1634,
		.vtotal = 1650,
		.width_mm = 68,
		.height_mm = 151,
		.type = DRM_MODE_TYPE_DRIVER | DRM_MODE_TYPE_PREFERRED,
	},
};

static const struct drm_display_mode even_boe_modes[] = {
	{
		.clock = 85805,
		.hdisplay = 720,
		.hsync_start = 728,
		.hsync_end = 732,
		.htotal = 774,
		.vdisplay = 1600,
		.vsync_start = 1610,
		.vsync_end = 1612,
		.vtotal = 1866,
		.width_mm = 68,
		.height_mm = 151,
		.type = DRM_MODE_TYPE_DRIVER | DRM_MODE_TYPE_PREFERRED,
	},
};

/* ==========================================================================
 * MediaTek DRM v2 Extension Parameters
 * ========================================================================== */
static struct mtk_panel_params truly_ili9882n_ext_params = {
	.pll_clk = 360,
	.data_rate = 735,
	.physical_width_um = 68000,
	.physical_height_um = 151000,
	.cust_esd_check = 0,
	.esd_check_enable = 0,
};

static struct mtk_panel_params hlt_ili7807s_ext_params = {
	.pll_clk = 360,
	.data_rate = 720,
	.physical_width_um = 68000,
	.physical_height_um = 151000,
	.cust_esd_check = 0,
	.esd_check_enable = 0,
};

static struct mtk_panel_params even_boe_ext_params = {
	.vendor = "nt36525b",
	.manufacture = "hlt_boe",
	.pll_clk = 368,
	.data_rate = 735,
	.physical_width_um = 68000,
	.physical_height_um = 151000,
	.cust_esd_check = 0,
	.esd_check_enable = 0,
};

/* Panel Descriptors */
static const struct even_panel_desc truly_ili9882n_desc = {
	.name = "ili9882n",
	.vendor = "truly_ilitek",
	.modes = truly_ili9882n_modes,
	.num_modes = ARRAY_SIZE(truly_ili9882n_modes),
	.width_mm = 68,
	.height_mm = 151,
	.bpc = 8,
	.lanes = 3,
	.format = MIPI_DSI_FMT_RGB888,
	.mode_flags = MIPI_DSI_MODE_VIDEO | MIPI_DSI_MODE_VIDEO_SYNC_PULSE | MIPI_DSI_MODE_LPM,
	.init = truly_ili9882n_init,
	.ext_params = &truly_ili9882n_ext_params,
};

static const struct even_panel_desc hlt_ili7807s_desc = {
	.name = "ili7807s",
	.vendor = "hlt_ilitek",
	.modes = hlt_ili7807s_modes,
	.num_modes = ARRAY_SIZE(hlt_ili7807s_modes),
	.width_mm = 68,
	.height_mm = 151,
	.bpc = 8,
	.lanes = 3,
	.format = MIPI_DSI_FMT_RGB888,
	.mode_flags = MIPI_DSI_MODE_VIDEO | MIPI_DSI_MODE_VIDEO_SYNC_PULSE | MIPI_DSI_MODE_LPM,
	.init = hlt_ili7807s_init,
	.ext_params = &hlt_ili7807s_ext_params,
};

static const struct even_panel_desc even_boe_desc = {
	.name = "nt36525b",
	.vendor = "hlt_boe",
	.modes = even_boe_modes,
	.num_modes = ARRAY_SIZE(even_boe_modes),
	.width_mm = 68,
	.height_mm = 151,
	.bpc = 8,
	.lanes = 3,
	.format = MIPI_DSI_FMT_RGB888,
	.mode_flags = MIPI_DSI_MODE_VIDEO | MIPI_DSI_MODE_VIDEO_SYNC_PULSE | MIPI_DSI_MODE_LPM,
	.init = boe_nt36525b_init,
	.ext_params = &even_boe_ext_params,
};

/* ==========================================================================
 * MediaTek DRM Extension Callbacks
 * ========================================================================== */
static int panel_ext_reset(struct drm_panel *panel, int on)
{
	struct lcm *ctx = panel_to_lcm(panel);

	if (ctx->reset_gpio)
		gpiod_set_value_cansleep(ctx->reset_gpio, on ? 1 : 0);
	return 0;
}

static int lcm_panel_poweron(struct drm_panel *panel)
{
	struct lcm *ctx = panel_to_lcm(panel);
	int ret;

	if (ctx->vddio) {
		ret = regulator_enable(ctx->vddio);
		if (ret)
			dev_err(ctx->dev, "failed to enable vddio: %d\n", ret);
		msleep(5);
	}

	if (ctx->vsp) {
		regulator_set_voltage(ctx->vsp, 6000000, 6000000);
		ret = regulator_enable(ctx->vsp);
		if (ret)
			dev_err(ctx->dev, "failed to enable vsp regulator: %d\n", ret);
		msleep(5);
	} else if (ctx->bias_pos) {
		gpiod_set_value_cansleep(ctx->bias_pos, 1);
		msleep(5);
	}

	if (ctx->vsn) {
		regulator_set_voltage(ctx->vsn, 6000000, 6000000);
		ret = regulator_enable(ctx->vsn);
		if (ret)
			dev_err(ctx->dev, "failed to enable vsn regulator: %d\n", ret);
		msleep(10);
	} else if (ctx->bias_neg) {
		gpiod_set_value_cansleep(ctx->bias_neg, 1);
		msleep(10);
	}

	return 0;
}

static int lcm_panel_poweroff(struct drm_panel *panel)
{
	struct lcm *ctx = panel_to_lcm(panel);

	if (ctx->vsn) {
		regulator_disable(ctx->vsn);
		msleep(5);
	} else if (ctx->bias_neg) {
		gpiod_set_value_cansleep(ctx->bias_neg, 0);
		msleep(5);
	}

	if (ctx->vsp) {
		regulator_disable(ctx->vsp);
		msleep(2);
	} else if (ctx->bias_pos) {
		gpiod_set_value_cansleep(ctx->bias_pos, 0);
		msleep(2);
	}

	if (ctx->vddio)
		regulator_disable(ctx->vddio);

	return 0;
}

static void lcm_panel_reset(struct lcm *ctx)
{
	if (!ctx->reset_gpio)
		return;

	/* GPIO_ACTIVE_HIGH: 0 = reset asserted (low), 1 = out of reset (high) */
	gpiod_set_value_cansleep(ctx->reset_gpio, 0);
	msleep(12);
	gpiod_set_value_cansleep(ctx->reset_gpio, 1);
	msleep(12);
	gpiod_set_value_cansleep(ctx->reset_gpio, 0);
	msleep(12);
	gpiod_set_value_cansleep(ctx->reset_gpio, 1);
	msleep(12);
}

static int lcm_setbacklight_cmdq(void *dsi, dcs_write_gce cb,
				 void *handle, unsigned int level)
{
	char bl_cmd[] = { 0x51, 0x07, 0xff };

	if (!dsi || !cb)
		return -EINVAL;

	if (level > 4095)
		level = 4095;

	/* Novatek NT36525B uses 11-bit MSB-first 0x51 brightness (level >> 1) */
	bl_cmd[1] = (level >> 9) & 0x0f;
	bl_cmd[2] = (level >> 1) & 0xff;
	cb(dsi, handle, bl_cmd, ARRAY_SIZE(bl_cmd));

	oplus_display_brightness = level;
	return 0;
}

static struct mtk_panel_funcs ext_funcs = {
	.set_backlight_cmdq = lcm_setbacklight_cmdq,
	.reset = panel_ext_reset,
};

static int lcm_bl_update_status(struct backlight_device *bl)
{
	struct mipi_dsi_device *dsi = bl_get_data(bl);
	u16 brightness = backlight_get_brightness(bl);
	u8 bl_cmd[3];
	int ret;

	if (brightness > 4095)
		brightness = 4095;

	bl_cmd[0] = 0x51;
	bl_cmd[1] = (brightness >> 9) & 0x0f;
	bl_cmd[2] = (brightness >> 1) & 0xff;

	dsi->mode_flags &= ~MIPI_DSI_MODE_LPM;
	ret = mipi_dsi_dcs_write_buffer(dsi, bl_cmd, sizeof(bl_cmd));
	dsi->mode_flags |= MIPI_DSI_MODE_LPM;

	oplus_display_brightness = brightness;
	return ret < 0 ? ret : 0;
}

static int lcm_bl_get_brightness(struct backlight_device *bl)
{
	struct mipi_dsi_device *dsi = bl_get_data(bl);
	u16 brightness = 0;
	int ret;

	dsi->mode_flags &= ~MIPI_DSI_MODE_LPM;
	ret = mipi_dsi_dcs_get_display_brightness_large(dsi, &brightness);
	dsi->mode_flags |= MIPI_DSI_MODE_LPM;

	if (ret < 0)
		return oplus_display_brightness;

	return brightness;
}

static const struct backlight_ops lcm_bl_ops = {
	.update_status = lcm_bl_update_status,
	.get_brightness = lcm_bl_get_brightness,
};

static struct backlight_device *lcm_create_backlight(struct mipi_dsi_device *dsi)
{
	struct device *dev = &dsi->dev;
	const struct backlight_properties props = {
		.type = BACKLIGHT_RAW,
		.brightness = 2048,
		.max_brightness = 4095,
		.scale = BACKLIGHT_SCALE_NON_LINEAR,
	};

	return devm_backlight_device_register(dev, dev_name(dev), dev, dsi,
					      &lcm_bl_ops, &props);
}

/* ==========================================================================
 * Standard DRM Panel Operations
 * ========================================================================== */
static int lcm_prepare(struct drm_panel *panel)
{
	struct lcm *ctx = panel_to_lcm(panel);
	struct mipi_dsi_multi_context dsi_ctx = { .dsi = to_mipi_dsi_device(ctx->dev) };
	int ret;

	if (ctx->prepared)
		return 0;

	lcm_panel_poweron(panel);

	lcm_panel_reset(ctx);

	if (ctx->desc && ctx->desc->init) {
		ret = ctx->desc->init(&dsi_ctx);
		if (ret < 0) {
			dev_err(ctx->dev, "failed to initialize panel: %d\n", ret);
			lcm_panel_poweroff(panel);
			return ret;
		}
	}

	ctx->prepared = true;
	return 0;
}

static int lcm_unprepare(struct drm_panel *panel)
{
	struct lcm *ctx = panel_to_lcm(panel);
	struct mipi_dsi_device *dsi = to_mipi_dsi_device(ctx->dev);

	if (!ctx->prepared)
		return 0;

	mipi_dsi_dcs_set_display_off(dsi);
	msleep(20);
	mipi_dsi_dcs_enter_sleep_mode(dsi);
	msleep(120);

	if (ctx->reset_gpio)
		gpiod_set_value_cansleep(ctx->reset_gpio, 0);

	lcm_panel_poweroff(panel);
	ctx->prepared = false;
	return 0;
}

static int lcm_enable(struct drm_panel *panel)
{
	struct lcm *ctx = panel_to_lcm(panel);
	struct mipi_dsi_device *dsi = to_mipi_dsi_device(ctx->dev);
	u8 bl_cmd[3] = { 0x51, 0x04, 0x00 };

	if (ctx->enabled)
		return 0;

	/* Send MSB-first DCS brightness command to unblank backlight */
	dsi->mode_flags &= ~MIPI_DSI_MODE_LPM;
	mipi_dsi_dcs_write_buffer(dsi, bl_cmd, sizeof(bl_cmd));
	dsi->mode_flags |= MIPI_DSI_MODE_LPM;

	if (ctx->backlight) {
		ctx->backlight->props.brightness = 2048;
		ctx->backlight->props.power = FB_BLANK_UNBLANK;
		backlight_update_status(ctx->backlight);
	}

	ctx->enabled = true;
	return 0;
}

static int lcm_disable(struct drm_panel *panel)
{
	struct lcm *ctx = panel_to_lcm(panel);
	struct mipi_dsi_device *dsi = to_mipi_dsi_device(ctx->dev);
	u8 bl_cmd[3] = { 0x51, 0x00, 0x00 };

	if (!ctx->enabled)
		return 0;

	if (ctx->backlight) {
		ctx->backlight->props.power = FB_BLANK_POWERDOWN;
		backlight_update_status(ctx->backlight);
	}

	dsi->mode_flags &= ~MIPI_DSI_MODE_LPM;
	mipi_dsi_dcs_write_buffer(dsi, bl_cmd, sizeof(bl_cmd));
	dsi->mode_flags |= MIPI_DSI_MODE_LPM;

	ctx->enabled = false;
	return 0;
}

static int lcm_get_modes(struct drm_panel *panel, struct drm_connector *connector)
{
	struct lcm *ctx = panel_to_lcm(panel);
	const struct even_panel_desc *desc = ctx->desc;
	unsigned int i;

	if (!desc)
		return 0;

	for (i = 0; i < desc->num_modes; i++) {
		const struct drm_display_mode *m = &desc->modes[i];
		struct drm_display_mode *mode = drm_mode_duplicate(connector->dev, m);

		if (!mode)
			return -ENOMEM;

		mode->type = DRM_MODE_TYPE_DRIVER;
		if (i == 0)
			mode->type |= DRM_MODE_TYPE_PREFERRED;

		drm_mode_set_name(mode);
		drm_mode_probed_add(connector, mode);
	}

	connector->display_info.width_mm = desc->width_mm;
	connector->display_info.height_mm = desc->height_mm;
	connector->display_info.bpc = desc->bpc;
	return desc->num_modes;
}

static const struct drm_panel_funcs lcm_drm_funcs = {
	.disable = lcm_disable,
	.unprepare = lcm_unprepare,
	.prepare = lcm_prepare,
	.enable = lcm_enable,
	.get_modes = lcm_get_modes,
};

/* ==========================================================================
 * Probe & Driver Registration
 * ========================================================================== */
static int lcm_probe(struct mipi_dsi_device *dsi)
{
	struct device *dev = &dsi->dev;
	struct lcm *ctx;
	int ret;

	ctx = devm_kzalloc(dev, sizeof(*ctx), GFP_KERNEL);
	if (!ctx)
		return -ENOMEM;

	ctx->dev = dev;
	mipi_dsi_set_drvdata(dsi, ctx);

	ctx->desc = of_device_get_match_data(dev);
	if (!ctx->desc) {
		dev_err(dev, "no matching panel desc found\n");
		return -ENODEV;
	}

	ctx->vddio = devm_regulator_get_optional(dev, "vddio");
	if (IS_ERR(ctx->vddio)) {
		if (PTR_ERR(ctx->vddio) != -ENODEV)
			return dev_err_probe(dev, PTR_ERR(ctx->vddio), "failed to get vddio regulator\n");
		ctx->vddio = NULL;
	}

	ctx->vsp = devm_regulator_get_optional(dev, "vsp");
	if (IS_ERR(ctx->vsp)) {
		if (PTR_ERR(ctx->vsp) != -ENODEV)
			return dev_err_probe(dev, PTR_ERR(ctx->vsp), "failed to get vsp regulator\n");
		ctx->vsp = NULL;
	}

	ctx->vsn = devm_regulator_get_optional(dev, "vsn");
	if (IS_ERR(ctx->vsn)) {
		if (PTR_ERR(ctx->vsn) != -ENODEV)
			return dev_err_probe(dev, PTR_ERR(ctx->vsn), "failed to get vsn regulator\n");
		ctx->vsn = NULL;
	}

	ctx->reset_gpio = devm_gpiod_get_optional(dev, "reset", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->reset_gpio))
		return dev_err_probe(dev, PTR_ERR(ctx->reset_gpio), "cannot get reset-gpio\n");

	ctx->bias_pos = devm_gpiod_get_index_optional(dev, "bias", 0, GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->bias_pos))
		return dev_err_probe(dev, PTR_ERR(ctx->bias_pos), "cannot get bias-pos gpio\n");

	ctx->bias_neg = devm_gpiod_get_index_optional(dev, "bias", 1, GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->bias_neg))
		return dev_err_probe(dev, PTR_ERR(ctx->bias_neg), "cannot get bias-neg gpio\n");

	/* Keep bias regulators enabled to match bootloader seamless display state */
	if (ctx->vsp) {
		regulator_set_voltage(ctx->vsp, 6000000, 6000000);
		if (regulator_enable(ctx->vsp))
			dev_warn(dev, "failed to enable vsp on probe\n");
	}
	if (ctx->vsn) {
		regulator_set_voltage(ctx->vsn, 6000000, 6000000);
		if (regulator_enable(ctx->vsn))
			dev_warn(dev, "failed to enable vsn on probe\n");
	}

	/* Register DCS backlight */
	ctx->backlight = lcm_create_backlight(dsi);
	if (IS_ERR(ctx->backlight)) {
		dev_warn(dev, "failed to register backlight device: %ld\n", PTR_ERR(ctx->backlight));
		ctx->backlight = NULL;
	}

	ctx->prepared = true;
	ctx->enabled = true;
	ctx->panel.dev = dev;
	ctx->panel.funcs = &lcm_drm_funcs;
	ctx->panel.backlight = ctx->backlight;

	drm_panel_init(&ctx->panel, dev, &lcm_drm_funcs, DRM_MODE_CONNECTOR_DSI);
	drm_panel_add(&ctx->panel);

	dsi->lanes = ctx->desc->lanes;
	dsi->format = ctx->desc->format;
	dsi->mode_flags = ctx->desc->mode_flags;

	ret = mipi_dsi_attach(dsi);
	if (ret < 0) {
		dev_err(dev, "mipi_dsi_attach failed: %d\n", ret);
		drm_panel_remove(&ctx->panel);
		return ret;
	}

#if defined(CONFIG_MTK_PANEL_EXT)
	mtk_panel_tch_handle_reg(&ctx->panel);
	ret = mtk_panel_ext_create(dev, ctx->desc->ext_params, &ext_funcs, &ctx->panel);
	if (ret < 0) {
		dev_err(dev, "mtk_panel_ext_create failed: %d\n", ret);
		return ret;
	}
#endif

	register_device_proc("lcd", (char *)ctx->desc->name, (char *)ctx->desc->vendor);
	oplus_max_normal_brightness = 4095;
	pr_info("[DRM/LCM] %s (%s) probed successfully\n", ctx->desc->name, ctx->desc->vendor);
	return 0;
}

static void lcm_remove(struct mipi_dsi_device *dsi)
{
	struct lcm *ctx = mipi_dsi_get_drvdata(dsi);
#if defined(CONFIG_MTK_PANEL_EXT)
	struct mtk_panel_ctx *ext_ctx = find_panel_ctx(&ctx->panel);
#endif

	mipi_dsi_detach(dsi);
	drm_panel_remove(&ctx->panel);
#if defined(CONFIG_MTK_PANEL_EXT)
	if (ext_ctx) {
		mtk_panel_detach(ext_ctx);
		mtk_panel_remove(ext_ctx);
	}
#endif
}

static const struct of_device_id even_panel_of_match[] = {
	{ .compatible = "realme,even-truly-ili9882n",         .data = &truly_ili9882n_desc },
	{ .compatible = "ilt9882n_truly_even_hdp_dsi_vdo_lcm", .data = &truly_ili9882n_desc },
	{ .compatible = "realme,even-hlt-ili7807s",           .data = &hlt_ili7807s_desc },
	{ .compatible = "ilt7807s_hlt_even_hdp_dsi_vdo_lcm",   .data = &hlt_ili7807s_desc },
	{ .compatible = "realme,even-boe-nt36525b",           .data = &even_boe_desc },
	{ .compatible = "nt36525b_hlt_even_boe_hdp_dsi_vdo_lcm", .data = &even_boe_desc },
	{ }
};
MODULE_DEVICE_TABLE(of, even_panel_of_match);

static struct mipi_dsi_driver lcm_driver = {
	.probe = lcm_probe,
	.remove = lcm_remove,
	.driver = {
		.name = "panel-realme-even",
		.owner = THIS_MODULE,
		.of_match_table = even_panel_of_match,
	},
};

module_mipi_dsi_driver(lcm_driver);

MODULE_AUTHOR("Antigravity Port Team");
MODULE_DESCRIPTION("Realme EVEN (C25/C25s/Narzo 50A) HD+ DSI Panel Driver");
MODULE_LICENSE("GPL");
