// SPDX-License-Identifier: GPL-2.0-only
// Copyright (c) 2022 FIXME
// Generated with linux-mdss-dsi-panel-driver-generator from vendor device tree:
//   Copyright (c) 2013, The Linux Foundation. All rights reserved. (FIXME)

#include <linux/delay.h>
#include <linux/gpio/consumer.h>
#include <linux/module.h>
#include <linux/of.h>

#include <video/mipi_display.h>

#include <drm/drm_mipi_dsi.h>
#include <drm/drm_modes.h>
#include <drm/drm_panel.h>
#include <drm/display/drm_dsc.h>
#include <drm/display/drm_dsc_helper.h>

struct nt36672c {
	struct drm_panel panel;
	struct mipi_dsi_device *dsi;
	struct gpio_desc *reset_gpio;
	bool prepared;
};

static inline
struct nt36672c *to_nt36672c(struct drm_panel *panel)
{
	return container_of(panel, struct nt36672c, panel);
}

#define dsi_dcs_write_seq(dsi, seq...) do {				\
		static const u8 d[] = { seq };				\
		int ret;						\
		ret = mipi_dsi_dcs_write_buffer(dsi, d, ARRAY_SIZE(d));	\
		if (ret < 0)						\
			return ret;					\
	} while (0)

static void nt36672c_reset(struct nt36672c *ctx)
{
	gpiod_set_value_cansleep(ctx->reset_gpio, 0);
	usleep_range(5000, 6000);
	gpiod_set_value_cansleep(ctx->reset_gpio, 1);
	usleep_range(1000, 2000);
	gpiod_set_value_cansleep(ctx->reset_gpio, 0);
	usleep_range(10000, 11000);
}

static int nt36672c_on(struct nt36672c *ctx)
{
	struct mipi_dsi_device *dsi = ctx->dsi;
	struct device *dev = &dsi->dev;
	int ret;

	dsi_dcs_write_seq(dsi, 0xff, 0x10);
	dsi_dcs_write_seq(dsi, 0xfb, 0x01);
	dsi_dcs_write_seq(dsi, 0xb0, 0x00);
	dsi_dcs_write_seq(dsi, 0xff, 0xe0);
	dsi_dcs_write_seq(dsi, 0xfb, 0x01);
	dsi_dcs_write_seq(dsi, 0x35, 0x82);
	dsi_dcs_write_seq(dsi, 0xff, 0xf0);
	dsi_dcs_write_seq(dsi, 0xfb, 0x01);
	dsi_dcs_write_seq(dsi, 0x5a, 0x00);
	dsi_dcs_write_seq(dsi, 0x9c, 0x00);
	dsi_dcs_write_seq(dsi, 0xbe, 0x08);
	dsi_dcs_write_seq(dsi, 0xff, 0xc0);
	dsi_dcs_write_seq(dsi, 0xfb, 0x01);
	dsi_dcs_write_seq(dsi, 0x9c, 0x11);
	dsi_dcs_write_seq(dsi, 0x9d, 0x11);
	dsi_dcs_write_seq(dsi, 0xff, 0x10);
	dsi_dcs_write_seq(dsi, 0xfb, 0x01);

	ret = mipi_dsi_dcs_set_display_brightness(dsi, 0x00c1);
	if (ret < 0) {
		dev_err(dev, "Failed to set display brightness: %d\n", ret);
		return ret;
	}

	dsi_dcs_write_seq(dsi, MIPI_DCS_WRITE_CONTROL_DISPLAY, 0x2c);
	dsi_dcs_write_seq(dsi, MIPI_DCS_WRITE_POWER_SAVE, 0x00);
	dsi_dcs_write_seq(dsi, 0xff, 0x23);
	dsi_dcs_write_seq(dsi, 0xfb, 0x01);
	dsi_dcs_write_seq(dsi, 0x10, 0x82);
	dsi_dcs_write_seq(dsi, 0x11, 0x01);
	dsi_dcs_write_seq(dsi, 0x12, 0x95);
	dsi_dcs_write_seq(dsi, 0x15, 0x68);
	dsi_dcs_write_seq(dsi, 0x16, 0x0b);
	dsi_dcs_write_seq(dsi, MIPI_DCS_SET_PARTIAL_ROWS, 0xff);
	dsi_dcs_write_seq(dsi, MIPI_DCS_SET_PARTIAL_COLUMNS, 0xfd);
	dsi_dcs_write_seq(dsi, 0x32, 0xfb);
	dsi_dcs_write_seq(dsi, 0x33, 0xfa);
	dsi_dcs_write_seq(dsi, 0x34, 0xf9);
	dsi_dcs_write_seq(dsi, 0x35, 0xf7);
	dsi_dcs_write_seq(dsi, MIPI_DCS_SET_ADDRESS_MODE, 0xf5);
	dsi_dcs_write_seq(dsi, 0x37, 0xf4);
	dsi_dcs_write_seq(dsi, 0x38, 0xf1);
	dsi_dcs_write_seq(dsi, 0x39, 0xef);

	ret = mipi_dsi_dcs_set_pixel_format(dsi, 0xed);
	if (ret < 0) {
		dev_err(dev, "Failed to set pixel format: %d\n", ret);
		return ret;
	}

	dsi_dcs_write_seq(dsi, 0x3b, 0xeb);
	dsi_dcs_write_seq(dsi, MIPI_DCS_SET_3D_CONTROL, 0xea);
	dsi_dcs_write_seq(dsi, 0x3f, 0xe8);
	dsi_dcs_write_seq(dsi, MIPI_DCS_SET_VSYNC_TIMING, 0xe6);
	dsi_dcs_write_seq(dsi, 0x41, 0xe5);
	dsi_dcs_write_seq(dsi, MIPI_DCS_GET_SCANLINE, 0xff);
	dsi_dcs_write_seq(dsi, 0x46, 0xf3);
	dsi_dcs_write_seq(dsi, 0x47, 0xe8);
	dsi_dcs_write_seq(dsi, 0x48, 0xdd);
	dsi_dcs_write_seq(dsi, 0x49, 0xd3);
	dsi_dcs_write_seq(dsi, 0x4a, 0xc9);
	dsi_dcs_write_seq(dsi, 0x4b, 0xbe);
	dsi_dcs_write_seq(dsi, 0x4c, 0xb3);
	dsi_dcs_write_seq(dsi, 0x4d, 0xa9);
	dsi_dcs_write_seq(dsi, 0x4e, 0x9f);
	dsi_dcs_write_seq(dsi, 0x4f, 0x95);
	dsi_dcs_write_seq(dsi, 0x50, 0x8b);

	ret = mipi_dsi_dcs_set_display_brightness(dsi, 0x0081);
	if (ret < 0) {
		dev_err(dev, "Failed to set display brightness: %d\n", ret);
		return ret;
	}

	dsi_dcs_write_seq(dsi, 0x52, 0x77);
	dsi_dcs_write_seq(dsi, MIPI_DCS_WRITE_CONTROL_DISPLAY, 0x6d);
	dsi_dcs_write_seq(dsi, 0x54, 0x65);
	dsi_dcs_write_seq(dsi, 0x58, 0xff);
	dsi_dcs_write_seq(dsi, 0x59, 0xf8);
	dsi_dcs_write_seq(dsi, 0x5a, 0xf3);
	dsi_dcs_write_seq(dsi, 0x5b, 0xee);
	dsi_dcs_write_seq(dsi, 0x5c, 0xe9);
	dsi_dcs_write_seq(dsi, 0x5d, 0xe4);
	dsi_dcs_write_seq(dsi, MIPI_DCS_SET_CABC_MIN_BRIGHTNESS, 0xdf);
	dsi_dcs_write_seq(dsi, 0x5f, 0xda);
	dsi_dcs_write_seq(dsi, 0x60, 0xd4);
	dsi_dcs_write_seq(dsi, 0x61, 0xcf);
	dsi_dcs_write_seq(dsi, 0x62, 0xca);
	dsi_dcs_write_seq(dsi, 0x63, 0xc5);
	dsi_dcs_write_seq(dsi, 0x64, 0xc0);
	dsi_dcs_write_seq(dsi, 0x65, 0xbb);
	dsi_dcs_write_seq(dsi, 0x66, 0xb6);
	dsi_dcs_write_seq(dsi, 0x67, 0xb1);
	dsi_dcs_write_seq(dsi, 0xa0, 0x11);
	dsi_dcs_write_seq(dsi, 0xff, 0x27);
	dsi_dcs_write_seq(dsi, 0xfb, 0x01);
	dsi_dcs_write_seq(dsi, MIPI_DCS_SET_VSYNC_TIMING, 0x25);
	dsi_dcs_write_seq(dsi, 0xff, 0x10);
	dsi_dcs_write_seq(dsi, 0xfb, 0x01);

	ret = mipi_dsi_dcs_exit_sleep_mode(dsi);
	if (ret < 0) {
		dev_err(dev, "Failed to exit sleep mode: %d\n", ret);
		return ret;
	}
	msleep(70);

	ret = mipi_dsi_dcs_set_display_on(dsi);
	if (ret < 0) {
		dev_err(dev, "Failed to set display on: %d\n", ret);
		return ret;
	}
	msleep(40);

	dsi_dcs_write_seq(dsi, 0xff, 0x27);
	dsi_dcs_write_seq(dsi, 0xfb, 0x01);
	dsi_dcs_write_seq(dsi, 0x3f, 0x01);
	dsi_dcs_write_seq(dsi, 0x43, 0x08);

	return 0;
}

static int nt36672c_off(struct nt36672c *ctx)
{
	struct mipi_dsi_device *dsi = ctx->dsi;
	struct device *dev = &dsi->dev;
	int ret;

	dsi_dcs_write_seq(dsi, 0xff, 0x10);
	dsi_dcs_write_seq(dsi, 0xfb, 0x01);

	ret = mipi_dsi_dcs_set_display_off(dsi);
	if (ret < 0) {
		dev_err(dev, "Failed to set display off: %d\n", ret);
		return ret;
	}
	usleep_range(16000, 17000);

	ret = mipi_dsi_dcs_enter_sleep_mode(dsi);
	if (ret < 0) {
		dev_err(dev, "Failed to enter sleep mode: %d\n", ret);
		return ret;
	}
	msleep(60);

	return 0;
}

static int nt36672c_prepare(struct drm_panel *panel)
{
	struct nt36672c *ctx = to_nt36672c(panel);
	struct device *dev = &ctx->dsi->dev;
	struct drm_dsc_picture_parameter_set pps;
	int ret;

	if (ctx->prepared)
		return 0;

	nt36672c_reset(ctx);

	ret = nt36672c_on(ctx);
	if (ret < 0) {
		dev_err(dev, "Failed to initialize panel: %d\n", ret);
		gpiod_set_value_cansleep(ctx->reset_gpio, 1);
		return ret;
	}

	if (ctx->dsi->dsc) {
		drm_dsc_pps_payload_pack(&pps, ctx->dsi->dsc);
		print_hex_dump(KERN_DEBUG, "DSC params:", DUMP_PREFIX_NONE,
				16, 1, &pps, sizeof(pps), false);

		ret = mipi_dsi_picture_parameter_set(ctx->dsi, &pps);
		if (ret < 0) {
			dev_err(panel->dev, "failed to set pps: %d\n", ret);
			return ret;
		}
		msleep(28);
	}

	ctx->prepared = true;
	return 0;
}

static int nt36672c_unprepare(struct drm_panel *panel)
{
	struct nt36672c *ctx = to_nt36672c(panel);
	struct device *dev = &ctx->dsi->dev;
	int ret;

	if (!ctx->prepared)
		return 0;

	ret = nt36672c_off(ctx);
	if (ret < 0)
		dev_err(dev, "Failed to un-initialize panel: %d\n", ret);

	gpiod_set_value_cansleep(ctx->reset_gpio, 1);

	ctx->prepared = false;
	return 0;
}

static const struct drm_display_mode nt36672c_mode = {
	.clock = (1080 + 73 + 12 + 40) * (2400 + 32 + 2 + 30) * 120 / 1000,
	.hdisplay = 1080,
	.hsync_start = 1080 + 73,
	.hsync_end = 1080 + 73 + 12,
	.htotal = 1080 + 73 + 12 + 40,
	.vdisplay = 2400,
	.vsync_start = 2400 + 32,
	.vsync_end = 2400 + 32 + 2,
	.vtotal = 2400 + 32 + 2 + 30,
	.width_mm = 70,
	.height_mm = 154,
};

static int nt36672c_get_modes(struct drm_panel *panel,
					  struct drm_connector *connector)
{
	struct drm_display_mode *mode;

	mode = drm_mode_duplicate(connector->dev, &nt36672c_mode);
	if (!mode)
		return -ENOMEM;

	drm_mode_set_name(mode);

	mode->type = DRM_MODE_TYPE_DRIVER | DRM_MODE_TYPE_PREFERRED;
	connector->display_info.width_mm = mode->width_mm;
	connector->display_info.height_mm = mode->height_mm;
	drm_mode_probed_add(connector, mode);

	return 1;
}

static const struct drm_panel_funcs nt36672c_panel_funcs = {
	.prepare = nt36672c_prepare,
	.unprepare = nt36672c_unprepare,
	.get_modes = nt36672c_get_modes,
};

static int nt36672c_probe(struct mipi_dsi_device *dsi)
{
	struct device *dev = &dsi->dev;
	struct nt36672c *ctx;
	struct drm_dsc_config *dsc;
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
	dsi->mode_flags = MIPI_DSI_MODE_VIDEO | MIPI_DSI_CLOCK_NON_CONTINUOUS |
			  MIPI_DSI_MODE_LPM;

	drm_panel_init(&ctx->panel, dev, &nt36672c_panel_funcs,
		       DRM_MODE_CONNECTOR_DSI);

	ret = drm_panel_of_backlight(&ctx->panel);
	if (ret)
		return dev_err_probe(dev, ret, "Failed to get backlight\n");

	drm_panel_add(&ctx->panel);

	/* The panel is DSC panel only, set the dsc params */
	dsi->dsc = dsc = devm_kzalloc(&dsi->dev, sizeof(*dsc), GFP_KERNEL);
	if (!dsc)
		return -ENOMEM;

	dsc->dsc_version_major = 0x1;
	dsc->dsc_version_minor = 0x1;

	dsc->slice_height = 20;
	dsc->slice_width = 540;
	dsc->slice_count = 2;
	dsc->bits_per_component = 8;
	dsc->bits_per_pixel = 8 << 4; /* 4 fractional bits */
	dsc->block_pred_enable = true;

	ret = mipi_dsi_attach(dsi);
	if (ret < 0) {
		dev_err(dev, "Failed to attach to DSI host: %d\n", ret);
		drm_panel_remove(&ctx->panel);
		return ret;
	}

	return 0;
}

static void nt36672c_remove(struct mipi_dsi_device *dsi)
{
	struct nt36672c *ctx = mipi_dsi_get_drvdata(dsi);
	int ret;

	ret = mipi_dsi_detach(dsi);
	if (ret < 0)
		dev_err(&dsi->dev, "Failed to detach from DSI host: %d\n", ret);

	drm_panel_remove(&ctx->panel);
}

static const struct of_device_id nt36672c_of_match[] = {
	{ .compatible = "huaxing,nt36672c" }, // FIXME
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, nt36672c_of_match);

static struct mipi_dsi_driver nt36672c_driver = {
	.probe = nt36672c_probe,
	.remove = nt36672c_remove,
	.driver = {
		.name = "panel-huaxing-nt36672c",
		.of_match_table = nt36672c_of_match,
	},
};
module_mipi_dsi_driver(nt36672c_driver);

MODULE_AUTHOR("linux-mdss-dsi-panel-driver-generator <fix@me>"); // FIXME
MODULE_DESCRIPTION("DRM driver for nt36672c huaxing fhd video mode dsi panel");
MODULE_LICENSE("GPL v2");
