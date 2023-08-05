// SPDX-License-Identifier: GPL-2.0-only
// Copyright (c) 2023 FIXME
// Generated with linux-mdss-dsi-panel-driver-generator from vendor device tree:
//   Copyright (c) 2013, The Linux Foundation. All rights reserved. (FIXME)

#include <linux/backlight.h>
#include <linux/delay.h>
#include <linux/gpio/consumer.h>
#include <linux/module.h>
#include <linux/of.h>

#include <drm/drm_mipi_dsi.h>
#include <drm/drm_modes.h>
#include <drm/drm_panel.h>

struct xiaomi_f4_41_06_0a {
	struct drm_panel panel;
	struct mipi_dsi_device *dsi;
	struct gpio_desc *reset_gpio;
	bool prepared;
};

static inline
struct xiaomi_f4_41_06_0a *to_xiaomi_f4_41_06_0a(struct drm_panel *panel)
{
	return container_of(panel, struct xiaomi_f4_41_06_0a, panel);
}

static void xiaomi_f4_41_06_0a_reset(struct xiaomi_f4_41_06_0a *ctx)
{
	gpiod_set_value_cansleep(ctx->reset_gpio, 1);
	usleep_range(1000, 2000);
	gpiod_set_value_cansleep(ctx->reset_gpio, 0);
	usleep_range(10000, 11000);
}

static int xiaomi_f4_41_06_0a_on(struct xiaomi_f4_41_06_0a *ctx)
{
	struct mipi_dsi_device *dsi = ctx->dsi;
	struct device *dev = &dsi->dev;
	int ret;

	mipi_dsi_dcs_write_seq(dsi, 0xfe, 0x40);
	mipi_dsi_dcs_write_seq(dsi, 0x70, 0x04);
	mipi_dsi_dcs_write_seq(dsi, 0x4d, 0x32);
	mipi_dsi_dcs_write_seq(dsi, 0xfe, 0x40);
	mipi_dsi_dcs_write_seq(dsi, 0xbe, 0x17);
	mipi_dsi_dcs_write_seq(dsi, 0xbf, 0xbb);
	mipi_dsi_dcs_write_seq(dsi, 0xc0, 0xdd);
	mipi_dsi_dcs_write_seq(dsi, 0xc1, 0xff);
	mipi_dsi_dcs_write_seq(dsi, 0xfe, 0xd0);
	mipi_dsi_dcs_write_seq(dsi, 0x03, 0x24);
	mipi_dsi_dcs_write_seq(dsi, 0x04, 0x03);
	mipi_dsi_dcs_write_seq(dsi, 0xfe, 0x00);
	mipi_dsi_dcs_write_seq(dsi, 0xc2, 0x08);
	mipi_dsi_dcs_write_seq(dsi, 0xfe, 0x00);

	ret = mipi_dsi_dcs_set_tear_on(dsi, MIPI_DSI_DCS_TEAR_MODE_VBLANK);
	if (ret < 0) {
		dev_err(dev, "Failed to set tear on: %d\n", ret);
		return ret;
	}

	ret = mipi_dsi_dcs_set_display_brightness(dsi, 0x0000);
	if (ret < 0) {
		dev_err(dev, "Failed to set display brightness: %d\n", ret);
		return ret;
	}

	ret = mipi_dsi_dcs_exit_sleep_mode(dsi);
	if (ret < 0) {
		dev_err(dev, "Failed to exit sleep mode: %d\n", ret);
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

static int xiaomi_f4_41_06_0a_off(struct xiaomi_f4_41_06_0a *ctx)
{
	struct mipi_dsi_device *dsi = ctx->dsi;
	struct device *dev = &dsi->dev;
	int ret;

	ret = mipi_dsi_dcs_set_display_off(dsi);
	if (ret < 0) {
		dev_err(dev, "Failed to set display off: %d\n", ret);
		return ret;
	}
	msleep(50);

	ret = mipi_dsi_dcs_enter_sleep_mode(dsi);
	if (ret < 0) {
		dev_err(dev, "Failed to enter sleep mode: %d\n", ret);
		return ret;
	}
	msleep(20);

	return 0;
}

static int xiaomi_f4_41_06_0a_prepare(struct drm_panel *panel)
{
	struct xiaomi_f4_41_06_0a *ctx = to_xiaomi_f4_41_06_0a(panel);
	struct device *dev = &ctx->dsi->dev;
	int ret;

	if (ctx->prepared)
		return 0;

	xiaomi_f4_41_06_0a_reset(ctx);

	ret = xiaomi_f4_41_06_0a_on(ctx);
	if (ret < 0) {
		dev_err(dev, "Failed to initialize panel: %d\n", ret);
		gpiod_set_value_cansleep(ctx->reset_gpio, 1);
		return ret;
	}

	ctx->prepared = true;
	return 0;
}

static int xiaomi_f4_41_06_0a_unprepare(struct drm_panel *panel)
{
	struct xiaomi_f4_41_06_0a *ctx = to_xiaomi_f4_41_06_0a(panel);
	struct device *dev = &ctx->dsi->dev;
	int ret;

	if (!ctx->prepared)
		return 0;

	ret = xiaomi_f4_41_06_0a_off(ctx);
	if (ret < 0)
		dev_err(dev, "Failed to un-initialize panel: %d\n", ret);

	gpiod_set_value_cansleep(ctx->reset_gpio, 1);

	ctx->prepared = false;
	return 0;
}

static const struct drm_display_mode xiaomi_f4_41_06_0a_mode = {
	.clock = (1080 + 28 + 4 + 36) * (2340 + 8 + 4 + 4) * 60 / 1000,
	.hdisplay = 1080,
	.hsync_start = 1080 + 28,
	.hsync_end = 1080 + 28 + 4,
	.htotal = 1080 + 28 + 4 + 36,
	.vdisplay = 2340,
	.vsync_start = 2340 + 8,
	.vsync_end = 2340 + 8 + 4,
	.vtotal = 2340 + 8 + 4 + 4,
	.width_mm = 69,
	.height_mm = 149,
};

static int xiaomi_f4_41_06_0a_get_modes(struct drm_panel *panel,
					struct drm_connector *connector)
{
	struct drm_display_mode *mode;

	mode = drm_mode_duplicate(connector->dev, &xiaomi_f4_41_06_0a_mode);
	if (!mode)
		return -ENOMEM;

	drm_mode_set_name(mode);

	mode->type = DRM_MODE_TYPE_DRIVER | DRM_MODE_TYPE_PREFERRED;
	connector->display_info.width_mm = mode->width_mm;
	connector->display_info.height_mm = mode->height_mm;
	drm_mode_probed_add(connector, mode);

	return 1;
}

static const struct drm_panel_funcs xiaomi_f4_41_06_0a_panel_funcs = {
	.prepare = xiaomi_f4_41_06_0a_prepare,
	.unprepare = xiaomi_f4_41_06_0a_unprepare,
	.get_modes = xiaomi_f4_41_06_0a_get_modes,
};

static int xiaomi_f4_41_06_0a_bl_update_status(struct backlight_device *bl)
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
static int xiaomi_f4_41_06_0a_bl_get_brightness(struct backlight_device *bl)
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

static const struct backlight_ops xiaomi_f4_41_06_0a_bl_ops = {
	.update_status = xiaomi_f4_41_06_0a_bl_update_status,
	.get_brightness = xiaomi_f4_41_06_0a_bl_get_brightness,
};

static struct backlight_device *
xiaomi_f4_41_06_0a_create_backlight(struct mipi_dsi_device *dsi)
{
	struct device *dev = &dsi->dev;
	const struct backlight_properties props = {
		.type = BACKLIGHT_RAW,
		.brightness = 2047,
		.max_brightness = 2047,
	};

	return devm_backlight_device_register(dev, dev_name(dev), dev, dsi,
					      &xiaomi_f4_41_06_0a_bl_ops, &props);
}

static int xiaomi_f4_41_06_0a_probe(struct mipi_dsi_device *dsi)
{
	struct device *dev = &dsi->dev;
	struct xiaomi_f4_41_06_0a *ctx;
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
	dsi->mode_flags = MIPI_DSI_MODE_VIDEO_BURST |
			  MIPI_DSI_CLOCK_NON_CONTINUOUS | MIPI_DSI_MODE_LPM;

	ctx->panel.prepare_prev_first = true;

	drm_panel_init(&ctx->panel, dev, &xiaomi_f4_41_06_0a_panel_funcs,
		       DRM_MODE_CONNECTOR_DSI);
	ctx->panel.prepare_prev_first = true;

	ctx->panel.backlight = xiaomi_f4_41_06_0a_create_backlight(dsi);
	if (IS_ERR(ctx->panel.backlight))
		return dev_err_probe(dev, PTR_ERR(ctx->panel.backlight),
				     "Failed to create backlight\n");

	drm_panel_add(&ctx->panel);

	ret = mipi_dsi_attach(dsi);
	if (ret < 0) {
		dev_err(dev, "Failed to attach to DSI host: %d\n", ret);
		drm_panel_remove(&ctx->panel);
		return ret;
	}

	return 0;
}

static void xiaomi_f4_41_06_0a_remove(struct mipi_dsi_device *dsi)
{
	struct xiaomi_f4_41_06_0a *ctx = mipi_dsi_get_drvdata(dsi);
	int ret;

	ret = mipi_dsi_detach(dsi);
	if (ret < 0)
		dev_err(&dsi->dev, "Failed to detach from DSI host: %d\n", ret);

	drm_panel_remove(&ctx->panel);
}

static const struct of_device_id xiaomi_f4_41_06_0a_of_match[] = {
	{ .compatible = "xiaomi,f4-41-06-0a" }, // FIXME
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, xiaomi_f4_41_06_0a_of_match);

static struct mipi_dsi_driver xiaomi_f4_41_06_0a_driver = {
	.probe = xiaomi_f4_41_06_0a_probe,
	.remove = xiaomi_f4_41_06_0a_remove,
	.driver = {
		.name = "panel-xiaomi-f4-41-06-0a",
		.of_match_table = xiaomi_f4_41_06_0a_of_match,
	},
};
module_mipi_dsi_driver(xiaomi_f4_41_06_0a_driver);

MODULE_AUTHOR("linux-mdss-dsi-panel-driver-generator <fix@me>"); // FIXME
MODULE_DESCRIPTION("DRM driver for xiaomi f4 41 06 0a fhd cmd dsi panel");
MODULE_LICENSE("GPL");
