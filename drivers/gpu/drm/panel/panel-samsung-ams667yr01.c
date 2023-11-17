// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2025, Jens Reidel <adrian@mainlining.org>
 */

#include <linux/backlight.h>
#include <linux/delay.h>
#include <linux/gpio/consumer.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/regulator/consumer.h>

#include <video/mipi_display.h>

#include <drm/display/drm_dsc.h>
#include <drm/display/drm_dsc_helper.h>
#include <drm/drm_mipi_dsi.h>
#include <drm/drm_modes.h>
#include <drm/drm_panel.h>
#include <drm/drm_probe_helper.h>

struct ams667yr01 {
	struct drm_panel panel;
	struct mipi_dsi_device *dsi;
	struct drm_dsc_config dsc;
	struct gpio_desc *reset_gpio;
	struct regulator_bulk_data *supplies;
};

static const struct regulator_bulk_data ams667yr01_supplies[] = {
	{ .supply = "vdd3p3" },
	{ .supply = "vddio" },
	{ .supply = "vsn" },
	{ .supply = "vsp" },
};

static inline struct ams667yr01 *to_ams667yr01(struct drm_panel *panel)
{
	return container_of(panel, struct ams667yr01, panel);
}

static void ams667yr01_reset(struct ams667yr01 *ctx)
{
	gpiod_set_value_cansleep(ctx->reset_gpio, 1);
	usleep_range(1000, 2000);
	gpiod_set_value_cansleep(ctx->reset_gpio, 0);
	usleep_range(10000, 11000);
}

static void ams667yr01_on(struct mipi_dsi_multi_context *dsi_ctx)
{
	dsi_ctx->dsi->mode_flags |= MIPI_DSI_MODE_LPM;

	mipi_dsi_dcs_exit_sleep_mode_multi(dsi_ctx);
	mipi_dsi_msleep(dsi_ctx, 20);

	mipi_dsi_dcs_write_seq_multi(dsi_ctx, 0xf0, 0x5a, 0x5a);
	mipi_dsi_dcs_write_seq_multi(dsi_ctx, 0xb2, 0x01, 0x31);
	mipi_dsi_dcs_write_seq_multi(dsi_ctx, 0xdf, 0x09, 0x30, 0x95, 0x46,
				     0xe9);
	mipi_dsi_dcs_write_seq_multi(dsi_ctx, 0xf0, 0xa5, 0xa5);
	mipi_dsi_dcs_write_seq_multi(dsi_ctx, 0x9d, 0x01);
	mipi_dsi_dcs_write_seq_multi(
		dsi_ctx, 0x9e, 0x11, 0x00, 0x00, 0x89, 0x30, 0x80, 0x09, 0x60,
		0x04, 0x38, 0x00, 0x28, 0x02, 0x1c, 0x02, 0x1c, 0x02, 0x00,
		0x02, 0x0e, 0x00, 0x20, 0x03, 0xdd, 0x00, 0x07, 0x00, 0x0c,
		0x02, 0x77, 0x02, 0x8b, 0x18, 0x00, 0x10, 0xf0, 0x03, 0x0c,
		0x20, 0x00, 0x06, 0x0b, 0x0b, 0x33, 0x0e, 0x1c, 0x2a, 0x38,
		0x46, 0x54, 0x62, 0x69, 0x70, 0x77, 0x79, 0x7b, 0x7d, 0x7e,
		0x01, 0x02, 0x01, 0x00, 0x09, 0x40, 0x09, 0xbe, 0x19, 0xfc,
		0x19, 0xfa, 0x19, 0xf8, 0x1a, 0x38, 0x1a, 0x78, 0x1a, 0xb6,
		0x2a, 0xf6, 0x2b, 0x34, 0x2b, 0x74, 0x3b, 0x74, 0x6b, 0xf4,
		0x00);
	mipi_dsi_dcs_write_seq_multi(dsi_ctx, 0xf0, 0x5a, 0x5a);
	mipi_dsi_dcs_write_seq_multi(dsi_ctx, 0x60, 0x21);
	mipi_dsi_dcs_write_seq_multi(dsi_ctx, 0xf7, 0x0b);
	mipi_dsi_dcs_write_seq_multi(dsi_ctx, 0xf0, 0xa5, 0xa5);
	mipi_dsi_dcs_write_seq_multi(dsi_ctx, 0xf0, 0x5a, 0x5a);
	mipi_dsi_dcs_write_seq_multi(dsi_ctx, 0xb0, 0x00, 0x15, 0xf6);
	mipi_dsi_dcs_write_seq_multi(dsi_ctx, 0xf6, 0xf0);
	mipi_dsi_dcs_write_seq_multi(dsi_ctx, 0xb0, 0x00, 0x28, 0xf6);
	mipi_dsi_dcs_write_seq_multi(dsi_ctx, 0xf6, 0xf0);
	mipi_dsi_dcs_write_seq_multi(dsi_ctx, 0xb0, 0x00, 0x3b, 0xf6);
	mipi_dsi_dcs_write_seq_multi(dsi_ctx, 0xf6, 0xf0);
	mipi_dsi_dcs_write_seq_multi(dsi_ctx, 0xb0, 0x00, 0x0a, 0xf4);
	mipi_dsi_dcs_write_seq_multi(dsi_ctx, 0xf4, 0x98);
	mipi_dsi_dcs_write_seq_multi(dsi_ctx, 0xb0, 0x00, 0x11, 0xf4);
	mipi_dsi_dcs_write_seq_multi(dsi_ctx, 0xf4, 0xee);
	mipi_dsi_dcs_write_seq_multi(dsi_ctx, 0xb0, 0x00, 0x18, 0xb2);
	mipi_dsi_dcs_write_seq_multi(dsi_ctx, 0xb2, 0x1c);
	mipi_dsi_dcs_write_seq_multi(dsi_ctx, 0xfc, 0x5a, 0x5a);
	mipi_dsi_dcs_write_seq_multi(dsi_ctx, 0xb0, 0x00, 0x11, 0xfe);
	mipi_dsi_dcs_write_seq_multi(dsi_ctx, 0xfe, 0x00);
	mipi_dsi_dcs_write_seq_multi(dsi_ctx, 0xfc, 0xa5, 0xa5);
	mipi_dsi_dcs_write_seq_multi(dsi_ctx, 0xb0, 0x00, 0x0d, 0xb2);
	mipi_dsi_dcs_write_seq_multi(dsi_ctx, 0xb2, 0x20);
	mipi_dsi_dcs_write_seq_multi(dsi_ctx, 0xb0, 0x00, 0x0c, 0xb2);
	mipi_dsi_dcs_write_seq_multi(dsi_ctx, 0xb2, 0x30);
	mipi_dsi_dcs_write_seq_multi(dsi_ctx, 0xf7, 0x0b);
	mipi_dsi_dcs_write_seq_multi(dsi_ctx, 0xf0, 0xa5, 0xa5);
	mipi_dsi_dcs_write_seq_multi(dsi_ctx, 0xf0, 0x5a, 0x5a);
	mipi_dsi_dcs_write_seq_multi(dsi_ctx, 0xfc, 0x5a, 0x5a);
	mipi_dsi_dcs_write_seq_multi(dsi_ctx, 0xe1, 0x83, 0x00, 0x00, 0x81,
				     0x00, 0xf9, 0xf8);
	mipi_dsi_dcs_write_seq_multi(dsi_ctx, 0xed, 0x00, 0x01);
	mipi_dsi_dcs_write_seq_multi(dsi_ctx, 0xb0, 0x00, 0x06, 0xf4);
	mipi_dsi_dcs_write_seq_multi(dsi_ctx, 0xf4, 0x1f);
	mipi_dsi_dcs_write_seq_multi(dsi_ctx, 0xf0, 0xa5, 0xa5);
	mipi_dsi_dcs_write_seq_multi(dsi_ctx, 0xfc, 0xa5, 0xa5);
	mipi_dsi_dcs_write_seq_multi(dsi_ctx, MIPI_DCS_WRITE_CONTROL_DISPLAY,
				     0x28);

	mipi_dsi_dcs_set_display_brightness_multi(dsi_ctx, 0x0000);
	mipi_dsi_msleep(dsi_ctx, 100);

	mipi_dsi_dcs_set_display_on_multi(dsi_ctx);
}

static int ams667yr01_disable(struct drm_panel *panel)
{
	struct ams667yr01 *ctx = to_ams667yr01(panel);
	struct mipi_dsi_device *dsi = ctx->dsi;
	struct mipi_dsi_multi_context dsi_ctx = { .dsi = dsi };

	dsi->mode_flags &= ~MIPI_DSI_MODE_LPM;

	mipi_dsi_dcs_set_display_off_multi(&dsi_ctx);
	usleep_range(10000, 11000);
	mipi_dsi_dcs_enter_sleep_mode_multi(&dsi_ctx);
	mipi_dsi_msleep(&dsi_ctx, 150);

	return dsi_ctx.accum_err;
}

static int ams667yr01_prepare(struct drm_panel *panel)
{
	struct ams667yr01 *ctx = to_ams667yr01(panel);
	struct drm_dsc_picture_parameter_set pps;
	struct mipi_dsi_multi_context dsi_ctx = { .dsi = ctx->dsi };

	dsi_ctx.accum_err = regulator_bulk_enable(
		ARRAY_SIZE(ams667yr01_supplies), ctx->supplies);
	if (dsi_ctx.accum_err)
		return dsi_ctx.accum_err;

	ams667yr01_reset(ctx);

	ams667yr01_on(&dsi_ctx);

	drm_dsc_pps_payload_pack(&pps, &ctx->dsc);

	mipi_dsi_picture_parameter_set_multi(&dsi_ctx, &pps);
	mipi_dsi_compression_mode_multi(&dsi_ctx, true);
	mipi_dsi_msleep(&dsi_ctx, 28);

	if (dsi_ctx.accum_err) {
		gpiod_set_value_cansleep(ctx->reset_gpio, 1);
		regulator_bulk_disable(ARRAY_SIZE(ams667yr01_supplies),
				       ctx->supplies);
	}

	return dsi_ctx.accum_err;
}

static int ams667yr01_unprepare(struct drm_panel *panel)
{
	struct ams667yr01 *ctx = to_ams667yr01(panel);

	gpiod_set_value_cansleep(ctx->reset_gpio, 1);
	regulator_bulk_disable(ARRAY_SIZE(ams667yr01_supplies), ctx->supplies);

	return 0;
}

static const struct drm_display_mode ams667yr01_modes[] = {
	{
		/* 120Hz mode */
		.clock = (1080 + 120 + 28 + 120) * (2400 + 20 + 2 + 10) * 120 /
			 1000,
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
	},
	{
		/* 90Hz mode */
		.clock = (1080 + 120 + 28 + 120) * (2400 + 20 + 2 + 10) * 90 /
			 1000,
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
	},
	{
		/* 60Hz mode */
		.clock = (1080 + 120 + 28 + 120) * (2400 + 20 + 2 + 10) * 60 /
			 1000,
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
	},
};

static int ams667yr01_get_modes(struct drm_panel *panel,
				struct drm_connector *connector)
{
	int count = 0;

	for (int i = 0; i < ARRAY_SIZE(ams667yr01_modes); i++) {
		count += drm_connector_helper_get_modes_fixed(
			connector, &ams667yr01_modes[i]);
	}

	return count;
}

static const struct drm_panel_funcs ams667yr01_panel_funcs = {
	.prepare = ams667yr01_prepare,
	.unprepare = ams667yr01_unprepare,
	.disable = ams667yr01_disable,
	.get_modes = ams667yr01_get_modes,
};

static int ams667yr01_bl_update_status(struct backlight_device *bl)
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

static int ams667yr01_bl_get_brightness(struct backlight_device *bl)
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

static const struct backlight_ops ams667yr01_bl_ops = {
	.update_status = ams667yr01_bl_update_status,
	.get_brightness = ams667yr01_bl_get_brightness,
};

static struct backlight_device *
ams667yr01_create_backlight(struct mipi_dsi_device *dsi)
{
	struct device *dev = &dsi->dev;
	const struct backlight_properties props = {
		.type = BACKLIGHT_RAW,
		.brightness = 1024,
		.max_brightness = 2047,
	};

	return devm_backlight_device_register(dev, dev_name(dev), dev, dsi,
					      &ams667yr01_bl_ops, &props);
}

static int ams667yr01_probe(struct mipi_dsi_device *dsi)
{
	struct device *dev = &dsi->dev;
	struct ams667yr01 *ctx;
	int ret;

	ctx = devm_kzalloc(dev, sizeof(*ctx), GFP_KERNEL);
	if (!ctx)
		return -ENOMEM;

	ret = devm_regulator_bulk_get_const(&dsi->dev,
					    ARRAY_SIZE(ams667yr01_supplies),
					    ams667yr01_supplies,
					    &ctx->supplies);
	if (ret < 0)
		return dev_err_probe(dev, ret, "Failed to get regulators\n");

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

	drm_panel_init(&ctx->panel, dev, &ams667yr01_panel_funcs,
		       DRM_MODE_CONNECTOR_DSI);
	ctx->panel.prepare_prev_first = true;

	ctx->panel.backlight = ams667yr01_create_backlight(dsi);
	if (IS_ERR(ctx->panel.backlight))
		return dev_err_probe(dev, PTR_ERR(ctx->panel.backlight),
				     "Failed to create backlight\n");

	drm_panel_add(&ctx->panel);

	/* This panel only supports DSC; unconditionally enable it */
	dsi->dsc = &ctx->dsc;

	/* TODO: Pass slice_per_pkt = 2 */
	ctx->dsc.dsc_version_major = 1;
	ctx->dsc.dsc_version_minor = 1;
	ctx->dsc.slice_height = 20;
	ctx->dsc.slice_width = 540;

	ctx->dsc.slice_count = 1080 / ctx->dsc.slice_width;
	ctx->dsc.bits_per_component = 8;
	ctx->dsc.bits_per_pixel = 8 << 4; /* 4 fractional bits */
	ctx->dsc.block_pred_enable = true;

	ret = devm_mipi_dsi_attach(dev, dsi);
	if (ret < 0) {
		drm_panel_remove(&ctx->panel);
		return dev_err_probe(dev, ret,
				     "Failed to attach to DSI host\n");
	}

	return 0;
}

static void ams667yr01_remove(struct mipi_dsi_device *dsi)
{
	struct ams667yr01 *ctx = mipi_dsi_get_drvdata(dsi);

	drm_panel_remove(&ctx->panel);
}

static const struct of_device_id ams667yr01_of_match[] = {
	{ .compatible = "samsung,ams667yr01" },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, ams667yr01_of_match);

static struct mipi_dsi_driver ams667yr01_driver = {
	.probe = ams667yr01_probe,
	.remove = ams667yr01_remove,
	.driver = {
		.name = "panel-samsung-ams667yr01",
		.of_match_table = ams667yr01_of_match,
	},
};
module_mipi_dsi_driver(ams667yr01_driver);

MODULE_AUTHOR("Jens Reidel <adrian@mainlining.org>");
MODULE_DESCRIPTION("DRM driver for SAMSUNG AMS667YR01 video mode dsi panel");
MODULE_LICENSE("GPL");
