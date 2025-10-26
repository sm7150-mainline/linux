// SPDX-License-Identifier: GPL-2.0-only
// Copyright (c) 2025 Jens Reidel <adrian@mainlining.org>
// Generated with linux-mdss-dsi-panel-driver-generator from vendor device tree:
//   Copyright (c) 2013, The Linux Foundation. All rights reserved.

#include <linux/backlight.h>
#include <linux/delay.h>
#include <linux/gpio/consumer.h>
#include <linux/mod_devicetable.h>
#include <linux/module.h>
#include <linux/regulator/consumer.h>

#include <drm/drm_mipi_dsi.h>
#include <drm/drm_modes.h>
#include <drm/drm_panel.h>
#include <drm/drm_probe_helper.h>

struct g1639fp106 {
	struct drm_panel panel;
	struct mipi_dsi_device *dsi;
	struct gpio_desc *reset_gpio;
	struct regulator_bulk_data *supplies;
};

static const struct regulator_bulk_data g1639fp106_supplies[] = {
	{ .supply = "vdd3p3" },
	{ .supply = "vddio" },
	{ .supply = "vsn" },
	{ .supply = "vsp" },
};

static inline struct g1639fp106 *to_g1639fp106(struct drm_panel *panel)
{
	return container_of(panel, struct g1639fp106, panel);
}

static void g1639fp106_reset(struct g1639fp106 *ctx)
{
	gpiod_set_value_cansleep(ctx->reset_gpio, 1);
	usleep_range(1000, 2000);
	gpiod_set_value_cansleep(ctx->reset_gpio, 0);
	usleep_range(10000, 11000);
}

static int g1639fp106_on(struct g1639fp106 *ctx)
{
	struct mipi_dsi_multi_context dsi_ctx = { .dsi = ctx->dsi };

	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0xb0, 0x00);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0xbb,
				     0x59, 0xff, 0xff, 0xff, 0xef, 0xcf, 0xab,
				     0x87, 0x63, 0x3f, 0x4a, 0x48, 0x46, 0x44,
				     0x42, 0x40, 0x3e, 0x3c, 0x3a, 0x64, 0x00,
				     0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
				     0xff, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00,
				     0x00, 0x00, 0x00, 0x00, 0x02, 0x02, 0x42,
				     0x00);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0xd4, 0x00, 0x8d);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0xfa, 0x0f);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0xb0, 0x80);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0xe6, 0x00);
	mipi_dsi_dcs_set_tear_on_multi(&dsi_ctx, MIPI_DSI_DCS_TEAR_MODE_VBLANK);
	mipi_dsi_dcs_set_column_address_multi(&dsi_ctx, 0x0000, 0x0437);
	mipi_dsi_dcs_set_page_address_multi(&dsi_ctx, 0x0000, 0x0923);
	mipi_dsi_dcs_set_display_brightness_multi(&dsi_ctx, 0x0001);
	mipi_dsi_dcs_exit_sleep_mode_multi(&dsi_ctx);
	mipi_dsi_msleep(&dsi_ctx, 110);
	mipi_dsi_dcs_set_display_on_multi(&dsi_ctx);
	mipi_dsi_usleep_range(&dsi_ctx, 16000, 17000);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0xb0, 0x00);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0xf0, 0xcf, 0xc3, 0x00);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, 0xf0, 0xcf, 0xc4, 0x00);

	return dsi_ctx.accum_err;
}

static int g1639fp106_off(struct g1639fp106 *ctx)
{
	struct mipi_dsi_multi_context dsi_ctx = { .dsi = ctx->dsi };

	mipi_dsi_dcs_set_display_off_multi(&dsi_ctx);
	mipi_dsi_dcs_enter_sleep_mode_multi(&dsi_ctx);
	mipi_dsi_msleep(&dsi_ctx, 120);

	return dsi_ctx.accum_err;
}

static int g1639fp106_prepare(struct drm_panel *panel)
{
	struct g1639fp106 *ctx = to_g1639fp106(panel);
	struct device *dev = &ctx->dsi->dev;
	int ret;

	ret = regulator_bulk_enable(ARRAY_SIZE(g1639fp106_supplies), ctx->supplies);
	if (ret < 0) {
		dev_err(dev, "Failed to enable regulators: %d\n", ret);
		return ret;
	}

	g1639fp106_reset(ctx);

	ret = g1639fp106_on(ctx);
	if (ret < 0) {
		dev_err(dev, "Failed to initialize panel: %d\n", ret);
		gpiod_set_value_cansleep(ctx->reset_gpio, 1);
		regulator_bulk_disable(ARRAY_SIZE(g1639fp106_supplies), ctx->supplies);
		return ret;
	}

	return 0;
}

static int g1639fp106_unprepare(struct drm_panel *panel)
{
	struct g1639fp106 *ctx = to_g1639fp106(panel);
	struct device *dev = &ctx->dsi->dev;
	int ret;

	ret = g1639fp106_off(ctx);
	if (ret < 0)
		dev_err(dev, "Failed to un-initialize panel: %d\n", ret);

	gpiod_set_value_cansleep(ctx->reset_gpio, 1);
	regulator_bulk_disable(ARRAY_SIZE(g1639fp106_supplies), ctx->supplies);

	return 0;
}

static const struct drm_display_mode g1639fp106_mode = {
	.clock = (1080 + 64 + 20 + 64) * (2340 + 64 + 20 + 64) * 60 / 1000,
	.hdisplay = 1080,
	.hsync_start = 1080 + 64,
	.hsync_end = 1080 + 64 + 20,
	.htotal = 1080 + 64 + 20 + 64,
	.vdisplay = 2340,
	.vsync_start = 2340 + 64,
	.vsync_end = 2340 + 64 + 20,
	.vtotal = 2340 + 64 + 20 + 64,
	.width_mm = 68,
	.height_mm = 147,
	.type = DRM_MODE_TYPE_DRIVER,
};

static int g1639fp106_get_modes(struct drm_panel *panel,
			       struct drm_connector *connector)
{
	return drm_connector_helper_get_modes_fixed(connector, &g1639fp106_mode);
}

static const struct drm_panel_funcs g1639fp106_panel_funcs = {
	.prepare = g1639fp106_prepare,
	.unprepare = g1639fp106_unprepare,
	.get_modes = g1639fp106_get_modes,
};

static int g1639fp106_bl_update_status(struct backlight_device *bl)
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

static int g1639fp106_bl_get_brightness(struct backlight_device *bl)
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

static const struct backlight_ops g1639fp106_bl_ops = {
	.update_status = g1639fp106_bl_update_status,
	.get_brightness = g1639fp106_bl_get_brightness,
};

static struct backlight_device *
g1639fp106_create_backlight(struct mipi_dsi_device *dsi)
{
	struct device *dev = &dsi->dev;
	const struct backlight_properties props = {
		.type = BACKLIGHT_RAW,
		.brightness = 2047,
		.max_brightness = 4095,
	};

	return devm_backlight_device_register(dev, dev_name(dev), dev, dsi,
					      &g1639fp106_bl_ops, &props);
}

static int g1639fp106_probe(struct mipi_dsi_device *dsi)
{
	struct device *dev = &dsi->dev;
	struct g1639fp106 *ctx;
	int ret;

	ctx = devm_drm_panel_alloc(dev, struct g1639fp106, panel,
				   &g1639fp106_panel_funcs,
				   DRM_MODE_CONNECTOR_DSI);
	if (IS_ERR(ctx))
		return PTR_ERR(ctx);

	ret = devm_regulator_bulk_get_const(dev,
					    ARRAY_SIZE(g1639fp106_supplies),
					    g1639fp106_supplies,
					    &ctx->supplies);
	if (ret < 0)
		return ret;

	ctx->reset_gpio = devm_gpiod_get(dev, "reset", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->reset_gpio))
		return dev_err_probe(dev, PTR_ERR(ctx->reset_gpio),
				     "Failed to get reset-gpios\n");

	ctx->dsi = dsi;
	mipi_dsi_set_drvdata(dsi, ctx);

	dsi->lanes = 4;
	dsi->format = MIPI_DSI_FMT_RGB888;
	dsi->mode_flags = MIPI_DSI_MODE_VIDEO_BURST |
			  MIPI_DSI_CLOCK_NON_CONTINUOUS | MIPI_DSI_MODE_LPM;

	ctx->panel.prepare_prev_first = true;

	ctx->panel.backlight = g1639fp106_create_backlight(dsi);
	if (IS_ERR(ctx->panel.backlight))
		return dev_err_probe(dev, PTR_ERR(ctx->panel.backlight),
				     "Failed to create backlight\n");

	drm_panel_add(&ctx->panel);

	ret = mipi_dsi_attach(dsi);
	if (ret < 0) {
		drm_panel_remove(&ctx->panel);
		return dev_err_probe(dev, ret, "Failed to attach to DSI host\n");
	}

	return 0;
}

static void g1639fp106_remove(struct mipi_dsi_device *dsi)
{
	struct g1639fp106 *ctx = mipi_dsi_get_drvdata(dsi);
	int ret;

	ret = mipi_dsi_detach(dsi);
	if (ret < 0)
		dev_err(&dsi->dev, "Failed to detach from DSI host: %d\n", ret);

	drm_panel_remove(&ctx->panel);
}

static const struct of_device_id g1639fp106_of_match[] = {
	{ .compatible = "visionox,g1639fp106" },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, g1639fp106_of_match);

static struct mipi_dsi_driver g1639fp106_driver = {
	.probe = g1639fp106_probe,
	.remove = g1639fp106_remove,
	.driver = {
		.name = "panel-visionox-g1639fp106",
		.of_match_table = g1639fp106_of_match,
	},
};
module_mipi_dsi_driver(g1639fp106_driver);

MODULE_AUTHOR("Jens Reidel <adrian@mainlining.org>");
MODULE_DESCRIPTION("DRM driver for Visionox G1639FP106 panel");
MODULE_LICENSE("GPL");
