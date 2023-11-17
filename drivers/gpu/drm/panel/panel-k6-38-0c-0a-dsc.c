// SPDX-License-Identifier: GPL-2.0-only
// Copyright (c) 2023 FIXME
// Generated with linux-mdss-dsi-panel-driver-generator from vendor device tree:
//   Copyright (c) 2013, The Linux Foundation. All rights reserved. (FIXME)

#include <linux/backlight.h>
#include <linux/delay.h>
#include <linux/gpio/consumer.h>
#include <linux/module.h>
#include <linux/of.h>

#include <video/mipi_display.h>

#include <drm/display/drm_dsc.h>
#include <drm/display/drm_dsc_helper.h>
#include <drm/drm_mipi_dsi.h>
#include <drm/drm_modes.h>
#include <drm/drm_panel.h>

struct k6_38_0c_0a_dsc {
	struct drm_panel panel;
	struct mipi_dsi_device *dsi;
	struct drm_dsc_config dsc;
	struct gpio_desc *reset_gpio;
	bool prepared;
};

static inline
struct k6_38_0c_0a_dsc *to_k6_38_0c_0a_dsc(struct drm_panel *panel)
{
	return container_of(panel, struct k6_38_0c_0a_dsc, panel);
}

static void k6_38_0c_0a_dsc_reset(struct k6_38_0c_0a_dsc *ctx)
{
	gpiod_set_value_cansleep(ctx->reset_gpio, 1);
	usleep_range(1000, 2000);
	gpiod_set_value_cansleep(ctx->reset_gpio, 0);
	usleep_range(10000, 11000);
}

static int k6_38_0c_0a_dsc_on(struct k6_38_0c_0a_dsc *ctx)
{
	struct mipi_dsi_device *dsi = ctx->dsi;
	struct device *dev = &dsi->dev;
	int ret;

	dsi->mode_flags |= MIPI_DSI_MODE_LPM;

	ret = mipi_dsi_dcs_exit_sleep_mode(dsi);
	if (ret < 0) {
		dev_err(dev, "Failed to exit sleep mode: %d\n", ret);
		return ret;
	}
	msleep(20);

	mipi_dsi_dcs_write_seq(dsi, 0xf0, 0x5a, 0x5a);
	mipi_dsi_dcs_write_seq(dsi, 0xb2, 0x01, 0x31);
	mipi_dsi_dcs_write_seq(dsi, 0xdf, 0x09, 0x30, 0x95, 0x46, 0xe9);
	mipi_dsi_dcs_write_seq(dsi, 0xf0, 0xa5, 0xa5);
	mipi_dsi_dcs_write_seq(dsi, 0x9d, 0x01);
	mipi_dsi_dcs_write_seq(dsi, 0x9e,
			       0x11, 0x00, 0x00, 0x89, 0x30, 0x80, 0x09, 0x60,
			       0x04, 0x38, 0x00, 0x28, 0x02, 0x1c, 0x02, 0x1c,
			       0x02, 0x00, 0x02, 0x0e, 0x00, 0x20, 0x03, 0xdd,
			       0x00, 0x07, 0x00, 0x0c, 0x02, 0x77, 0x02, 0x8b,
			       0x18, 0x00, 0x10, 0xf0, 0x03, 0x0c, 0x20, 0x00,
			       0x06, 0x0b, 0x0b, 0x33, 0x0e, 0x1c, 0x2a, 0x38,
			       0x46, 0x54, 0x62, 0x69, 0x70, 0x77, 0x79, 0x7b,
			       0x7d, 0x7e, 0x01, 0x02, 0x01, 0x00, 0x09, 0x40,
			       0x09, 0xbe, 0x19, 0xfc, 0x19, 0xfa, 0x19, 0xf8,
			       0x1a, 0x38, 0x1a, 0x78, 0x1a, 0xb6, 0x2a, 0xf6,
			       0x2b, 0x34, 0x2b, 0x74, 0x3b, 0x74, 0x6b, 0xf4,
			       0x00);
	mipi_dsi_dcs_write_seq(dsi, 0xf0, 0x5a, 0x5a);
	mipi_dsi_dcs_write_seq(dsi, 0x60, 0x21);
	mipi_dsi_dcs_write_seq(dsi, 0xf7, 0x0b);
	mipi_dsi_dcs_write_seq(dsi, 0xf0, 0xa5, 0xa5);
	mipi_dsi_dcs_write_seq(dsi, 0xf0, 0x5a, 0x5a);
	mipi_dsi_dcs_write_seq(dsi, 0xb0, 0x00, 0x15, 0xf6);
	mipi_dsi_dcs_write_seq(dsi, 0xf6, 0xf0);
	mipi_dsi_dcs_write_seq(dsi, 0xb0, 0x00, 0x28, 0xf6);
	mipi_dsi_dcs_write_seq(dsi, 0xf6, 0xf0);
	mipi_dsi_dcs_write_seq(dsi, 0xb0, 0x00, 0x3b, 0xf6);
	mipi_dsi_dcs_write_seq(dsi, 0xf6, 0xf0);
	mipi_dsi_dcs_write_seq(dsi, 0xb0, 0x00, 0x0a, 0xf4);
	mipi_dsi_dcs_write_seq(dsi, 0xf4, 0x98);
	mipi_dsi_dcs_write_seq(dsi, 0xb0, 0x00, 0x11, 0xf4);
	mipi_dsi_dcs_write_seq(dsi, 0xf4, 0xee);
	mipi_dsi_dcs_write_seq(dsi, 0xb0, 0x00, 0x18, 0xb2);
	mipi_dsi_dcs_write_seq(dsi, 0xb2, 0x1c);
	mipi_dsi_dcs_write_seq(dsi, 0xfc, 0x5a, 0x5a);
	mipi_dsi_dcs_write_seq(dsi, 0xb0, 0x00, 0x11, 0xfe);
	mipi_dsi_dcs_write_seq(dsi, 0xfe, 0x00);
	mipi_dsi_dcs_write_seq(dsi, 0xfc, 0xa5, 0xa5);
	mipi_dsi_dcs_write_seq(dsi, 0xb0, 0x00, 0x0d, 0xb2);
	mipi_dsi_dcs_write_seq(dsi, 0xb2, 0x20);
	mipi_dsi_dcs_write_seq(dsi, 0xb0, 0x00, 0x0c, 0xb2);
	mipi_dsi_dcs_write_seq(dsi, 0xb2, 0x30);
	mipi_dsi_dcs_write_seq(dsi, 0xf7, 0x0b);
	mipi_dsi_dcs_write_seq(dsi, 0xf0, 0xa5, 0xa5);
	mipi_dsi_dcs_write_seq(dsi, 0xf0, 0x5a, 0x5a);
	mipi_dsi_dcs_write_seq(dsi, 0xfc, 0x5a, 0x5a);
	mipi_dsi_dcs_write_seq(dsi, 0xe1,
			       0x83, 0x00, 0x00, 0x81, 0x00, 0xf9, 0xf8);
	mipi_dsi_dcs_write_seq(dsi, 0xed, 0x00, 0x01);
	mipi_dsi_dcs_write_seq(dsi, 0xb0, 0x00, 0x06, 0xf4);
	mipi_dsi_dcs_write_seq(dsi, 0xf4, 0x1f);
	mipi_dsi_dcs_write_seq(dsi, 0xf0, 0xa5, 0xa5);
	mipi_dsi_dcs_write_seq(dsi, 0xfc, 0xa5, 0xa5);
	mipi_dsi_dcs_write_seq(dsi, MIPI_DCS_WRITE_CONTROL_DISPLAY, 0x28);

	ret = mipi_dsi_dcs_set_display_brightness(dsi, 0x0000);
	if (ret < 0) {
		dev_err(dev, "Failed to set display brightness: %d\n", ret);
		return ret;
	}
	msleep(100);

	ret = mipi_dsi_dcs_set_display_on(dsi);
	if (ret < 0) {
		dev_err(dev, "Failed to set display on: %d\n", ret);
		return ret;
	}

	return 0;
}

static int k6_38_0c_0a_dsc_off(struct k6_38_0c_0a_dsc *ctx)
{
	struct mipi_dsi_device *dsi = ctx->dsi;
	struct device *dev = &dsi->dev;
	int ret;

	dsi->mode_flags &= ~MIPI_DSI_MODE_LPM;

	ret = mipi_dsi_dcs_set_display_off(dsi);
	if (ret < 0) {
		dev_err(dev, "Failed to set display off: %d\n", ret);
		return ret;
	}
	usleep_range(10000, 11000);

	ret = mipi_dsi_dcs_enter_sleep_mode(dsi);
	if (ret < 0) {
		dev_err(dev, "Failed to enter sleep mode: %d\n", ret);
		return ret;
	}
	msleep(150);

	return 0;
}

static int k6_38_0c_0a_dsc_prepare(struct drm_panel *panel)
{
	struct k6_38_0c_0a_dsc *ctx = to_k6_38_0c_0a_dsc(panel);
	struct device *dev = &ctx->dsi->dev;
	struct drm_dsc_picture_parameter_set pps;
	int ret;

	if (ctx->prepared)
		return 0;

	k6_38_0c_0a_dsc_reset(ctx);

	ret = k6_38_0c_0a_dsc_on(ctx);
	if (ret < 0) {
		dev_err(dev, "Failed to initialize panel: %d\n", ret);
		gpiod_set_value_cansleep(ctx->reset_gpio, 1);
		return ret;
	}

	drm_dsc_pps_payload_pack(&pps, &ctx->dsc);

	ret = mipi_dsi_picture_parameter_set(ctx->dsi, &pps);
	if (ret < 0) {
		dev_err(panel->dev, "failed to transmit PPS: %d\n", ret);
		return ret;
	}

	ret = mipi_dsi_compression_mode(ctx->dsi, true);
	if (ret < 0) {
		dev_err(dev, "failed to enable compression mode: %d\n", ret);
		return ret;
	}

	msleep(28); /* TODO: Is this panel-dependent? */

	ctx->prepared = true;
	return 0;
}

static int k6_38_0c_0a_dsc_unprepare(struct drm_panel *panel)
{
	struct k6_38_0c_0a_dsc *ctx = to_k6_38_0c_0a_dsc(panel);
	struct device *dev = &ctx->dsi->dev;
	int ret;

	if (!ctx->prepared)
		return 0;

	ret = k6_38_0c_0a_dsc_off(ctx);
	if (ret < 0)
		dev_err(dev, "Failed to un-initialize panel: %d\n", ret);

	gpiod_set_value_cansleep(ctx->reset_gpio, 1);

	ctx->prepared = false;
	return 0;
}

static const struct drm_display_mode k6_38_0c_0a_dsc_mode = {
	.clock = (1080 + 120 + 28 + 120) * (2400 + 20 + 2 + 10) * 120 / 1000,
	.hdisplay = 1080,
	.hsync_start = 1080 + 120,
	.hsync_end = 1080 + 120 + 28,
	.htotal = 1080 + 120 + 28 + 120,
	.vdisplay = 2400,
	.vsync_start = 2400 + 20,
	.vsync_end = 2400 + 20 + 2,
	.vtotal = 2400 + 20 + 2 + 10,
	.width_mm = 69,
	.height_mm = 154,
};

static int k6_38_0c_0a_dsc_get_modes(struct drm_panel *panel,
				     struct drm_connector *connector)
{
	struct drm_display_mode *mode;

	mode = drm_mode_duplicate(connector->dev, &k6_38_0c_0a_dsc_mode);
	if (!mode)
		return -ENOMEM;

	drm_mode_set_name(mode);

	mode->type = DRM_MODE_TYPE_DRIVER | DRM_MODE_TYPE_PREFERRED;
	connector->display_info.width_mm = mode->width_mm;
	connector->display_info.height_mm = mode->height_mm;
	drm_mode_probed_add(connector, mode);

	return 1;
}

static const struct drm_panel_funcs k6_38_0c_0a_dsc_panel_funcs = {
	.prepare = k6_38_0c_0a_dsc_prepare,
	.unprepare = k6_38_0c_0a_dsc_unprepare,
	.get_modes = k6_38_0c_0a_dsc_get_modes,
};

static int k6_38_0c_0a_dsc_bl_update_status(struct backlight_device *bl)
{
	struct mipi_dsi_device *dsi = bl_get_data(bl);
	u16 brightness = backlight_get_brightness(bl);
	int ret;

	dsi->mode_flags &= ~MIPI_DSI_MODE_LPM;

	ret = mipi_dsi_dcs_set_display_brightness_large(dsi, brightness);
	if (ret < 0)
		return ret;

	dsi->mode_flags |= MIPI_DSI_MODE_LPM;

	return 0;
}

// TODO: Check if /sys/class/backlight/.../actual_brightness actually returns
// correct values. If not, remove this function.
static int k6_38_0c_0a_dsc_bl_get_brightness(struct backlight_device *bl)
{
	struct mipi_dsi_device *dsi = bl_get_data(bl);
	u16 brightness;
	int ret;

	dsi->mode_flags &= ~MIPI_DSI_MODE_LPM;

	ret = mipi_dsi_dcs_get_display_brightness_large(dsi, &brightness);
	if (ret < 0)
		return ret;

	dsi->mode_flags |= MIPI_DSI_MODE_LPM;

	return brightness;
}

static const struct backlight_ops k6_38_0c_0a_dsc_bl_ops = {
	.update_status = k6_38_0c_0a_dsc_bl_update_status,
	.get_brightness = k6_38_0c_0a_dsc_bl_get_brightness,
};

static struct backlight_device *
k6_38_0c_0a_dsc_create_backlight(struct mipi_dsi_device *dsi)
{
	struct device *dev = &dsi->dev;
	const struct backlight_properties props = {
		.type = BACKLIGHT_RAW,
		.brightness = 2047,
		.max_brightness = 2047,
	};

	return devm_backlight_device_register(dev, dev_name(dev), dev, dsi,
					      &k6_38_0c_0a_dsc_bl_ops, &props);
}

static int k6_38_0c_0a_dsc_probe(struct mipi_dsi_device *dsi)
{
	struct device *dev = &dsi->dev;
	struct k6_38_0c_0a_dsc *ctx;
	int ret;

	ctx = devm_kzalloc(dev, sizeof(*ctx), GFP_KERNEL);
	if (!ctx)
		return -ENOMEM;

	ctx->reset_gpio = devm_gpiod_get(dev, "reset", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->reset_gpio))
		return dev_err_probe(dev, PTR_ERR(ctx->reset_gpio),
				     "Failed to get reset-gpios\n");

	ctx->dsi = dsi;
	mipi_dsi_set_drvdata(dsi, ctx);

	dsi->lanes = 4;
	dsi->format = MIPI_DSI_FMT_RGB888;
	dsi->mode_flags = MIPI_DSI_MODE_VIDEO | MIPI_DSI_MODE_VIDEO_BURST |
			  MIPI_DSI_CLOCK_NON_CONTINUOUS;

	drm_panel_init(&ctx->panel, dev, &k6_38_0c_0a_dsc_panel_funcs,
		       DRM_MODE_CONNECTOR_DSI);
	ctx->panel.prepare_prev_first = true;

	ctx->panel.backlight = k6_38_0c_0a_dsc_create_backlight(dsi);
	if (IS_ERR(ctx->panel.backlight))
		return dev_err_probe(dev, PTR_ERR(ctx->panel.backlight),
				     "Failed to create backlight\n");

	drm_panel_add(&ctx->panel);

	/* This panel only supports DSC; unconditionally enable it */
	dsi->dsc = &ctx->dsc;

	ctx->dsc.dsc_version_major = 1;
	ctx->dsc.dsc_version_minor = 1;

	/* TODO: Pass slice_per_pkt = 2 */
	ctx->dsc.slice_height = 20;
	ctx->dsc.slice_width = 540;
	/*
	 * TODO: hdisplay should be read from the selected mode once
	 * it is passed back to drm_panel (in prepare?)
	 */
	WARN_ON(1080 % ctx->dsc.slice_width);
	ctx->dsc.slice_count = 1080 / ctx->dsc.slice_width;
	ctx->dsc.bits_per_component = 8;
	ctx->dsc.bits_per_pixel = 8 << 4; /* 4 fractional bits */
	ctx->dsc.block_pred_enable = true;

	ret = mipi_dsi_attach(dsi);
	if (ret < 0) {
		dev_err(dev, "Failed to attach to DSI host: %d\n", ret);
		drm_panel_remove(&ctx->panel);
		return ret;
	}

	return 0;
}

static void k6_38_0c_0a_dsc_remove(struct mipi_dsi_device *dsi)
{
	struct k6_38_0c_0a_dsc *ctx = mipi_dsi_get_drvdata(dsi);
	int ret;

	ret = mipi_dsi_detach(dsi);
	if (ret < 0)
		dev_err(&dsi->dev, "Failed to detach from DSI host: %d\n", ret);

	drm_panel_remove(&ctx->panel);
}

static const struct of_device_id k6_38_0c_0a_dsc_of_match[] = {
	{ .compatible = "mdss,k6-38-0c-0a-dsc" }, // FIXME
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, k6_38_0c_0a_dsc_of_match);

static struct mipi_dsi_driver k6_38_0c_0a_dsc_driver = {
	.probe = k6_38_0c_0a_dsc_probe,
	.remove = k6_38_0c_0a_dsc_remove,
	.driver = {
		.name = "panel-k6-38-0c-0a-dsc",
		.of_match_table = k6_38_0c_0a_dsc_of_match,
	},
};
module_mipi_dsi_driver(k6_38_0c_0a_dsc_driver);

MODULE_AUTHOR("linux-mdss-dsi-panel-driver-generator <fix@me>"); // FIXME
MODULE_DESCRIPTION("DRM driver for xiaomi k6 38 0c 0a fhd dsc video dsi panel");
MODULE_LICENSE("GPL");
