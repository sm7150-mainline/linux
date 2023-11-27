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

struct k9a_36_02_0a_mp_dsc {
	struct drm_panel panel;
	struct mipi_dsi_device *dsi;
	struct drm_dsc_config dsc;
	struct gpio_desc *reset_gpio;
	bool prepared;
};

static inline
struct k9a_36_02_0a_mp_dsc *to_k9a_36_02_0a_mp_dsc(struct drm_panel *panel)
{
	return container_of(panel, struct k9a_36_02_0a_mp_dsc, panel);
}

static void k9a_36_02_0a_mp_dsc_reset(struct k9a_36_02_0a_mp_dsc *ctx)
{
	gpiod_set_value_cansleep(ctx->reset_gpio, 0);
	usleep_range(11000, 12000);
	gpiod_set_value_cansleep(ctx->reset_gpio, 1);
	usleep_range(1000, 2000);
	gpiod_set_value_cansleep(ctx->reset_gpio, 0);
	usleep_range(11000, 12000);
}

static int k9a_36_02_0a_mp_dsc_on(struct k9a_36_02_0a_mp_dsc *ctx)
{
	struct mipi_dsi_device *dsi = ctx->dsi;
	struct device *dev = &dsi->dev;
	int ret;

	mipi_dsi_dcs_write_seq(dsi, 0xf0, 0x55, 0xaa, 0x52, 0x08, 0x00);
	mipi_dsi_dcs_write_seq(dsi, 0xb2, 0x58);
	mipi_dsi_dcs_write_seq(dsi, 0x6f, 0x02);
	mipi_dsi_dcs_write_seq(dsi, 0xb2, 0x0c, 0x0c);
	mipi_dsi_dcs_write_seq(dsi, 0xbe, 0x0e, 0x0b, 0x14, 0x13);
	mipi_dsi_dcs_write_seq(dsi, 0x6f, 0x05);
	mipi_dsi_dcs_write_seq(dsi, 0xbe, 0x8a);
	mipi_dsi_dcs_write_seq(dsi, 0xc0, 0x66);
	mipi_dsi_dcs_write_seq(dsi, 0x6f, 0x08);
	mipi_dsi_dcs_write_seq(dsi, 0xb5, 0x32);
	mipi_dsi_dcs_write_seq(dsi, 0xf0, 0x55, 0xaa, 0x52, 0x08, 0x07);
	mipi_dsi_dcs_write_seq(dsi, 0xc0, 0x00, 0x02);
	mipi_dsi_dcs_write_seq(dsi, 0xf0, 0x55, 0xaa, 0x52, 0x08, 0x01);
	mipi_dsi_dcs_write_seq(dsi, 0xd1, 0x07, 0x00, 0x04);
	mipi_dsi_dcs_write_seq(dsi, 0x3b, 0x00, 0x10, 0x00, 0x30);
	mipi_dsi_dcs_write_seq(dsi, 0x90, 0x01);
	mipi_dsi_dcs_write_seq(dsi, 0x91,
			       0xab, 0x28, 0x00, 0x0c, 0xc2, 0x00, 0x03, 0x1c,
			       0x01, 0x7e, 0x00, 0x0f, 0x08, 0xbb, 0x04, 0x3d,
			       0x10, 0xf0);
	mipi_dsi_dcs_write_seq(dsi, 0x03, 0x01);
	mipi_dsi_dcs_write_seq(dsi, 0x51, 0x00, 0x00, 0x00, 0x00);
	mipi_dsi_dcs_write_seq(dsi, MIPI_DCS_WRITE_CONTROL_DISPLAY, 0x20);

	ret = mipi_dsi_dcs_set_tear_on(dsi, MIPI_DSI_DCS_TEAR_MODE_VBLANK);
	if (ret < 0) {
		dev_err(dev, "Failed to set tear on: %d\n", ret);
		return ret;
	}

	ret = mipi_dsi_dcs_set_column_address(dsi, 0x0000, 0x0437);
	if (ret < 0) {
		dev_err(dev, "Failed to set column address: %d\n", ret);
		return ret;
	}

	ret = mipi_dsi_dcs_set_page_address(dsi, 0x0000, 0x095f);
	if (ret < 0) {
		dev_err(dev, "Failed to set page address: %d\n", ret);
		return ret;
	}

	mipi_dsi_dcs_write_seq(dsi, 0x2f, 0x02);
	mipi_dsi_dcs_write_seq(dsi, 0xff, 0xaa, 0x55, 0xa5, 0x81);
	mipi_dsi_dcs_write_seq(dsi, 0x6f, 0x0f);
	mipi_dsi_dcs_write_seq(dsi, 0xfd, 0x01, 0x5a);
	mipi_dsi_dcs_write_seq(dsi, 0x6f, 0x04);
	mipi_dsi_dcs_write_seq(dsi, 0xfd, 0x5f);
	mipi_dsi_dcs_write_seq(dsi, 0x6f, 0x1a);
	mipi_dsi_dcs_write_seq(dsi, 0xfd, 0x5f);
	mipi_dsi_dcs_write_seq(dsi, MIPI_DCS_WRITE_MEMORY_START);
	mipi_dsi_dcs_write_seq(dsi, 0xf0, 0x55, 0xaa, 0x52, 0x08, 0x00);
	mipi_dsi_dcs_write_seq(dsi, 0xca, 0x12, 0x00, 0x92, 0x02);
	mipi_dsi_dcs_write_seq(dsi, 0x6f, 0x02);
	mipi_dsi_dcs_write_seq(dsi, 0xec, 0x80, 0x10);
	mipi_dsi_dcs_write_seq(dsi, 0xf0, 0x55, 0xaa, 0x52, 0x08, 0x01);
	mipi_dsi_dcs_write_seq(dsi, 0xcd, 0x05, 0x31);
	mipi_dsi_dcs_write_seq(dsi, 0x6f, 0x10);
	mipi_dsi_dcs_write_seq(dsi, 0xd8, 0x0c);
	mipi_dsi_dcs_write_seq(dsi, 0xf0, 0x55, 0xaa, 0x52, 0x08, 0x05);
	mipi_dsi_dcs_write_seq(dsi, 0xb3, 0x86, 0x80);
	mipi_dsi_dcs_write_seq(dsi, 0xb5, 0x85, 0x81);
	mipi_dsi_dcs_write_seq(dsi, 0xb7, 0x85, 0x00, 0x00, 0x81);
	mipi_dsi_dcs_write_seq(dsi, 0xb8, 0x05, 0x00, 0x00, 0x81);
	mipi_dsi_dcs_write_seq(dsi, 0xec, 0x0d, 0x11);
	mipi_dsi_dcs_write_seq(dsi, 0x6f, 0x02);
	mipi_dsi_dcs_write_seq(dsi, 0xec,
			       0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			       0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			       0x00, 0x00);
	mipi_dsi_dcs_write_seq(dsi, 0xf0, 0x55, 0xaa, 0x52, 0x08, 0x01);
	mipi_dsi_dcs_write_seq(dsi, 0x6f, 0x01);
	mipi_dsi_dcs_write_seq(dsi, 0xd2, 0x01, 0x28, 0x33);
	mipi_dsi_dcs_write_seq(dsi, 0x6f, 0x06);
	mipi_dsi_dcs_write_seq(dsi, 0xd2, 0x00);
	mipi_dsi_dcs_write_seq(dsi, 0x6f, 0x0f);
	mipi_dsi_dcs_write_seq(dsi, 0xd2, 0x00);
	mipi_dsi_dcs_write_seq(dsi, 0x6f, 0x09);
	mipi_dsi_dcs_write_seq(dsi, 0xd2, 0x00);

	ret = mipi_dsi_dcs_exit_sleep_mode(dsi);
	if (ret < 0) {
		dev_err(dev, "Failed to exit sleep mode: %d\n", ret);
		return ret;
	}
	msleep(50);

	ret = mipi_dsi_dcs_set_display_on(dsi);
	if (ret < 0) {
		dev_err(dev, "Failed to set display on: %d\n", ret);
		return ret;
	}
	usleep_range(16000, 17000);

	return 0;
}

static int k9a_36_02_0a_mp_dsc_off(struct k9a_36_02_0a_mp_dsc *ctx)
{
	struct mipi_dsi_device *dsi = ctx->dsi;
	struct device *dev = &dsi->dev;
	int ret;

	ret = mipi_dsi_dcs_set_display_off(dsi);
	if (ret < 0) {
		dev_err(dev, "Failed to set display off: %d\n", ret);
		return ret;
	}
	msleep(20);

	ret = mipi_dsi_dcs_enter_sleep_mode(dsi);
	if (ret < 0) {
		dev_err(dev, "Failed to enter sleep mode: %d\n", ret);
		return ret;
	}
	msleep(80);

	return 0;
}

static int k9a_36_02_0a_mp_dsc_prepare(struct drm_panel *panel)
{
	struct k9a_36_02_0a_mp_dsc *ctx = to_k9a_36_02_0a_mp_dsc(panel);
	struct device *dev = &ctx->dsi->dev;
	struct drm_dsc_picture_parameter_set pps;
	int ret;

	if (ctx->prepared)
		return 0;

	k9a_36_02_0a_mp_dsc_reset(ctx);

	ret = k9a_36_02_0a_mp_dsc_on(ctx);
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

static int k9a_36_02_0a_mp_dsc_unprepare(struct drm_panel *panel)
{
	struct k9a_36_02_0a_mp_dsc *ctx = to_k9a_36_02_0a_mp_dsc(panel);
	struct device *dev = &ctx->dsi->dev;
	int ret;

	if (!ctx->prepared)
		return 0;

	ret = k9a_36_02_0a_mp_dsc_off(ctx);
	if (ret < 0)
		dev_err(dev, "Failed to un-initialize panel: %d\n", ret);

	gpiod_set_value_cansleep(ctx->reset_gpio, 1);

	ctx->prepared = false;
	return 0;
}

static const struct drm_display_mode k9a_36_02_0a_mp_dsc_mode = {
	.clock = (1080 + 16 + 8 + 8) * (2400 + 1212 + 4 + 8) * 60 / 1000,
	.hdisplay = 1080,
	.hsync_start = 1080 + 16,
	.hsync_end = 1080 + 16 + 8,
	.htotal = 1080 + 16 + 8 + 8,
	.vdisplay = 2400,
	.vsync_start = 2400 + 1212,
	.vsync_end = 2400 + 1212 + 4,
	.vtotal = 2400 + 1212 + 4 + 8,
	.width_mm = 683,
	.height_mm = 1517,
};

static int k9a_36_02_0a_mp_dsc_get_modes(struct drm_panel *panel,
					 struct drm_connector *connector)
{
	struct drm_display_mode *mode;

	mode = drm_mode_duplicate(connector->dev, &k9a_36_02_0a_mp_dsc_mode);
	if (!mode)
		return -ENOMEM;

	drm_mode_set_name(mode);

	mode->type = DRM_MODE_TYPE_DRIVER | DRM_MODE_TYPE_PREFERRED;
	connector->display_info.width_mm = mode->width_mm;
	connector->display_info.height_mm = mode->height_mm;
	drm_mode_probed_add(connector, mode);

	return 1;
}

static const struct drm_panel_funcs k9a_36_02_0a_mp_dsc_panel_funcs = {
	.prepare = k9a_36_02_0a_mp_dsc_prepare,
	.unprepare = k9a_36_02_0a_mp_dsc_unprepare,
	.get_modes = k9a_36_02_0a_mp_dsc_get_modes,
};

static int k9a_36_02_0a_mp_dsc_bl_update_status(struct backlight_device *bl)
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

static const struct backlight_ops k9a_36_02_0a_mp_dsc_bl_ops = {
	.update_status = k9a_36_02_0a_mp_dsc_bl_update_status,
};

static struct backlight_device *
k9a_36_02_0a_mp_dsc_create_backlight(struct mipi_dsi_device *dsi)
{
	struct device *dev = &dsi->dev;
	const struct backlight_properties props = {
		.type = BACKLIGHT_RAW,
		.brightness = 2047,
		.max_brightness = 2047,
	};

	return devm_backlight_device_register(dev, dev_name(dev), dev, dsi,
					      &k9a_36_02_0a_mp_dsc_bl_ops, &props);
}

static int k9a_36_02_0a_mp_dsc_probe(struct mipi_dsi_device *dsi)
{
	struct device *dev = &dsi->dev;
	struct k9a_36_02_0a_mp_dsc *ctx;
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
	dsi->format = MIPI_DSI_FMT_RGB101010;
	dsi->mode_flags = MIPI_DSI_MODE_VIDEO_BURST |
			  MIPI_DSI_CLOCK_NON_CONTINUOUS | MIPI_DSI_MODE_LPM;

	drm_panel_init(&ctx->panel, dev, &k9a_36_02_0a_mp_dsc_panel_funcs,
		       DRM_MODE_CONNECTOR_DSI);
	ctx->panel.prepare_prev_first = true;

	ctx->panel.backlight = k9a_36_02_0a_mp_dsc_create_backlight(dsi);
	if (IS_ERR(ctx->panel.backlight))
		return dev_err_probe(dev, PTR_ERR(ctx->panel.backlight),
				     "Failed to create backlight\n");

	drm_panel_add(&ctx->panel);

	/* This panel only supports DSC; unconditionally enable it */
	dsi->dsc = &ctx->dsc;

	ctx->dsc.dsc_version_major = 1;
	ctx->dsc.dsc_version_minor = 1;

	/* TODO: Pass slice_per_pkt = 1 */
	ctx->dsc.slice_height = 12;
	ctx->dsc.slice_width = 1080;
	/*
	 * TODO: hdisplay should be read from the selected mode once
	 * it is passed back to drm_panel (in prepare?)
	 */
	WARN_ON(1080 % ctx->dsc.slice_width);
	ctx->dsc.slice_count = 1080 / ctx->dsc.slice_width;
	ctx->dsc.bits_per_component = 10;
	ctx->dsc.bits_per_pixel = 10 << 4; /* 4 fractional bits */
	ctx->dsc.block_pred_enable = true;

	ret = mipi_dsi_attach(dsi);
	if (ret < 0) {
		dev_err(dev, "Failed to attach to DSI host: %d\n", ret);
		drm_panel_remove(&ctx->panel);
		return ret;
	}

	return 0;
}

static void k9a_36_02_0a_mp_dsc_remove(struct mipi_dsi_device *dsi)
{
	struct k9a_36_02_0a_mp_dsc *ctx = mipi_dsi_get_drvdata(dsi);
	int ret;

	ret = mipi_dsi_detach(dsi);
	if (ret < 0)
		dev_err(&dsi->dev, "Failed to detach from DSI host: %d\n", ret);

	drm_panel_remove(&ctx->panel);
}

static const struct of_device_id k9a_36_02_0a_mp_dsc_of_match[] = {
	{ .compatible = "mdss,k9a-36-02-0a-mp-dsc" }, // FIXME
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, k9a_36_02_0a_mp_dsc_of_match);

static struct mipi_dsi_driver k9a_36_02_0a_mp_dsc_driver = {
	.probe = k9a_36_02_0a_mp_dsc_probe,
	.remove = k9a_36_02_0a_mp_dsc_remove,
	.driver = {
		.name = "panel-k9a-36-02-0a-mp-dsc",
		.of_match_table = k9a_36_02_0a_mp_dsc_of_match,
	},
};
module_mipi_dsi_driver(k9a_36_02_0a_mp_dsc_driver);

MODULE_AUTHOR("linux-mdss-dsi-panel-driver-generator <fix@me>"); // FIXME
MODULE_DESCRIPTION("DRM driver for xiaomi 36 02 0a mp cmd mode dsc dsi panel");
MODULE_LICENSE("GPL");
