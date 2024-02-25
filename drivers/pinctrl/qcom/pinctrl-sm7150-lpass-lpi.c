// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2024, Danila Tikhonov <danila@jiaxyga.com>
 */

#include <linux/gpio/driver.h>
#include <linux/module.h>
#include <linux/platform_device.h>

#include "pinctrl-lpass-lpi.h"

enum lpass_lpi_functions {
	LPI_MUX_audio_ref,
	LPI_MUX_cdc_pdm_rx,
	LPI_MUX_dmic1_clk,
	LPI_MUX_dmic1_data,
	LPI_MUX_dmic2_clk,
	LPI_MUX_dmic2_data,
	LPI_MUX_qdss_cti,
	LPI_MUX_gp_pdm,
	LPI_MUX_prim_mclk_a,
	LPI_MUX_prim_mclk_b,
	LPI_MUX_qca_sb_clk,
	LPI_MUX_qca_sb_data,
	LPI_MUX_qua_mi2s_data,
	LPI_MUX_qua_mi2s_sclk,
	LPI_MUX_qua_mi2s_ws,
	LPI_MUX_slimbus_clk,
	LPI_MUX_slimbus_data,
	LPI_MUX_swr_rx_clk,
	LPI_MUX_swr_rx_data,
	LPI_MUX_swr_tx_clk,
	LPI_MUX_swr_tx_data,
	LPI_MUX_gpio,
	LPI_MUX__,
};

static const struct pinctrl_pin_desc sm7150_lpi_pins[] = {
	PINCTRL_PIN(0, "gpio0"),
	PINCTRL_PIN(1, "gpio1"),
	PINCTRL_PIN(2, "gpio2"),
	PINCTRL_PIN(3, "gpio3"),
	PINCTRL_PIN(4, "gpio4"),
	PINCTRL_PIN(5, "gpio5"),
	PINCTRL_PIN(6, "gpio6"),
	PINCTRL_PIN(7, "gpio7"),
	PINCTRL_PIN(8, "gpio8"),
	PINCTRL_PIN(9, "gpio9"),
	PINCTRL_PIN(10, "gpio10"),
	PINCTRL_PIN(11, "gpio11"),
	PINCTRL_PIN(12, "gpio12"),
	PINCTRL_PIN(13, "gpio13"),
};

static const char * const gpio_groups[] = {
	"gpio0",  "gpio1",  "gpio2",  "gpio3",  "gpio4",  "gpio5",  "gpio6",
	"gpio7",  "gpio8",  "gpio9",  "gpio10", "gpio11", "gpio12", "gpio13",
};

static const char * const audio_ref_groups[] = { "gpio1" };
static const char * const cdc_pdm_rx_groups[] = { "gpio6" };
static const char * const dmic1_clk_groups[] = { "gpio8" };
static const char * const dmic1_data_groups[] = { "gpio9" };
static const char * const dmic2_clk_groups[] = { "gpio10" };
static const char * const dmic2_data_groups[] = { "gpio11" };
static const char * const qdss_cti_groups[] = { "gpio9", "gpio10" };
static const char * const gp_pdm_groups[] = { "gpio10" };
static const char * const prim_mclk_a_groups[] = { "gpio4" };
static const char * const prim_mclk_b_groups[] = { "gpio11" };
static const char * const qca_sb_clk_groups[] = { "gpio13" };
static const char * const qca_sb_data_groups[] = { "gpio12" };
static const char * const qua_mi2s_data_groups[] = { "gpio7", "gpio8", "gpio9", "gpio10" };
static const char * const qua_mi2s_sclk_groups[] = { "gpio5" };
static const char * const qua_mi2s_ws_groups[] = { "gpio6" };
static const char * const slimbus_clk_groups[] = { "gpio0" };
static const char * const slimbus_data_groups[] = { "gpio2", "gpio3", "gpio4" };
static const char * const swr_rx_clk_groups[] = { "gpio3" };
static const char * const swr_rx_data_groups[] = { "gpio4", "gpio5" };
static const char * const swr_tx_clk_groups[] = { "gpio0" };
static const char * const swr_tx_data_groups[] = { "gpio1", "gpio2", "gpio5" };

static const struct lpi_pingroup sm7150_groups[] = {
	LPI_PINGROUP(0, 0, slimbus_clk, swr_tx_clk, _, _),
	LPI_PINGROUP(1, 2, swr_tx_data, audio_ref, _, _),
	LPI_PINGROUP(2, 4, slimbus_data, swr_tx_data, _, _),
	LPI_PINGROUP(3, 8, slimbus_data, swr_rx_clk, _, _),
	LPI_PINGROUP(4, 10, slimbus_data, swr_rx_data, prim_mclk_a, _),
	LPI_PINGROUP(5, 6, qua_mi2s_sclk, _, swr_rx_data, swr_tx_data),
	LPI_PINGROUP(6, LPI_NO_SLEW, qua_mi2s_ws, cdc_pdm_rx, _, _),
	LPI_PINGROUP(7, LPI_NO_SLEW, qua_mi2s_data, _, _, _),
	LPI_PINGROUP(8, LPI_NO_SLEW, qua_mi2s_data, dmic1_clk, _, _),
	LPI_PINGROUP(9, LPI_NO_SLEW, qua_mi2s_data, dmic1_data, qdss_cti, _),
	LPI_PINGROUP(10, LPI_NO_SLEW, qua_mi2s_data, dmic2_clk, gp_pdm, qdss_cti),
	LPI_PINGROUP(11, LPI_NO_SLEW, prim_mclk_b, dmic2_data, _, _),
	LPI_PINGROUP(12, LPI_NO_SLEW, qca_sb_data, _, _, _),
	LPI_PINGROUP(13, LPI_NO_SLEW, qca_sb_clk, _, _, _),
};

static const struct lpi_function sm7150_functions[] = {
	LPI_FUNCTION(audio_ref),
	LPI_FUNCTION(cdc_pdm_rx),
	LPI_FUNCTION(dmic1_clk),
	LPI_FUNCTION(dmic1_data),
	LPI_FUNCTION(dmic2_clk),
	LPI_FUNCTION(dmic2_data),
	LPI_FUNCTION(qdss_cti),
	LPI_FUNCTION(gp_pdm),
	LPI_FUNCTION(prim_mclk_a),
	LPI_FUNCTION(prim_mclk_b),
	LPI_FUNCTION(qca_sb_clk),
	LPI_FUNCTION(qca_sb_data),
	LPI_FUNCTION(qua_mi2s_data),
	LPI_FUNCTION(qua_mi2s_sclk),
	LPI_FUNCTION(qua_mi2s_ws),
	LPI_FUNCTION(slimbus_clk),
	LPI_FUNCTION(slimbus_data),
	LPI_FUNCTION(swr_rx_clk),
	LPI_FUNCTION(swr_rx_data),
	LPI_FUNCTION(swr_tx_clk),
	LPI_FUNCTION(swr_tx_data),
	LPI_FUNCTION(gpio),
};

static const struct lpi_pinctrl_variant_data sm7150_lpi_data = {
	.pins = sm7150_lpi_pins,
	.npins = ARRAY_SIZE(sm7150_lpi_pins),
	.groups = sm7150_groups,
	.ngroups = ARRAY_SIZE(sm7150_groups),
	.functions = sm7150_functions,
	.nfunctions = ARRAY_SIZE(sm7150_functions),
	.flags = LPI_FLAG_SLEW_RATE_SAME_REG,
};

static const struct of_device_id lpi_pinctrl_of_match[] = {
	{
	       .compatible = "qcom,sm7150-lpass-lpi-pinctrl",
	       .data = &sm7150_lpi_data,
	},
	{ }
};
MODULE_DEVICE_TABLE(of, lpi_pinctrl_of_match);

static struct platform_driver lpi_pinctrl_driver = {
	.driver = {
		   .name = "qcom-sm7150-lpass-lpi-pinctrl",
		   .of_match_table = lpi_pinctrl_of_match,
	},
	.probe = lpi_pinctrl_probe,
	.remove = lpi_pinctrl_remove,
};

module_platform_driver(lpi_pinctrl_driver);
MODULE_DESCRIPTION("Qualcomm SM7150 LPI GPIO pin control driver");
MODULE_LICENSE("GPL");
