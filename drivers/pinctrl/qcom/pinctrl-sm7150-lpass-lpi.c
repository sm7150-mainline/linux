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
	PINCTRL_PIN(14, "gpio14"),
	PINCTRL_PIN(15, "gpio15"),
	PINCTRL_PIN(16, "gpio16"),
	PINCTRL_PIN(17, "gpio17"),
	PINCTRL_PIN(18, "gpio18"),
	PINCTRL_PIN(19, "gpio19"),
	PINCTRL_PIN(20, "gpio20"),
	PINCTRL_PIN(21, "gpio21"),
	PINCTRL_PIN(22, "gpio22"),
	PINCTRL_PIN(23, "gpio23"),
	PINCTRL_PIN(24, "gpio24"),
	PINCTRL_PIN(25, "gpio25"),
	PINCTRL_PIN(26, "gpio26"),
	PINCTRL_PIN(27, "gpio27"),
	PINCTRL_PIN(28, "gpio28"),
	PINCTRL_PIN(29, "gpio29"),
	PINCTRL_PIN(30, "gpio30"),
	PINCTRL_PIN(31, "gpio31"),
};

static const char * const gpio_groups[] = {
	"gpio18", "gpio19", "gpio20", "gpio21", "gpio22", "gpio23", "gpio24",
	"gpio25", "gpio26", "gpio27", "gpio28", "gpio29", "gpio30", "gpio31",
};

static const char * const audio_ref_groups[] = { "gpio19" };
static const char * const cdc_pdm_rx_groups[] = { "gpio24" };
static const char * const dmic1_clk_groups[] = { "gpi26" };
static const char * const dmic1_data_groups[] = { "gpio27" };
static const char * const dmic2_clk_groups[] = { "gpio28" };
static const char * const dmic2_data_groups[] = { "gpio29" };
static const char * const qdss_cti_groups[] = { "gpio27", "gpio28" };
static const char * const gp_pdm_groups[] = { "gpio28" };
static const char * const prim_mclk_a_groups[] = { "gpio22" };
static const char * const prim_mclk_b_groups[] = { "gpio29" };
static const char * const qca_sb_clk_groups[] = { "gpio31" };
static const char * const qca_sb_data_groups[] = { "gpio30" };
static const char * const qua_mi2s_data_groups[] = { "gpio25", "gpio26", "gpio27", "gpio28" };
static const char * const qua_mi2s_sclk_groups[] = { "gpio23" };
static const char * const qua_mi2s_ws_groups[] = { "gpio24" };
static const char * const slimbus_clk_groups[] = { "gpio18" };
static const char * const slimbus_data_groups[] = { "gpio20", "gpio21", "gpio22" };
static const char * const swr_rx_clk_groups[] = { "gpio21" };
static const char * const swr_rx_data_groups[] = { "gpio22", "gpio23" };
static const char * const swr_tx_clk_groups[] = { "gpio18" };
static const char * const swr_tx_data_groups[] = { "gpio19", "gpio20", "gpio23" };

static const struct lpi_pingroup sm7150_groups[] = {
	LPI_PINGROUP(0, LPI_NO_SLEW, _, _, _, _), // slew rate 3
	LPI_PINGROUP(1, LPI_NO_SLEW, _, _, _, _), // slew rate 0
	LPI_PINGROUP(2, LPI_NO_SLEW, _, _, _, _), // slew rate 3
	LPI_PINGROUP(3, LPI_NO_SLEW, _, _, _, _),  // slew rate 0
	LPI_PINGROUP(4, LPI_NO_SLEW, _, _, _, _), // slew rate 3
	LPI_PINGROUP(5, LPI_NO_SLEW, _, _, _, _), // slew rate 0
	LPI_PINGROUP(6, LPI_NO_SLEW, _, _, _, _), // slew rate 3
	LPI_PINGROUP(7, LPI_NO_SLEW, _, _, _, _),
	LPI_PINGROUP(8, LPI_NO_SLEW, _, _, _, _),
	LPI_PINGROUP(9, LPI_NO_SLEW, _, _, _, _),
	LPI_PINGROUP(10, LPI_NO_SLEW, _, _, _, _),
	LPI_PINGROUP(11, LPI_NO_SLEW, _, _, _, _),
	LPI_PINGROUP(12, LPI_NO_SLEW, _, _, _, _),
	LPI_PINGROUP(13, LPI_NO_SLEW, _, _, _, _),
	LPI_PINGROUP(14, LPI_NO_SLEW, _, _, _, _),
	LPI_PINGROUP(15, LPI_NO_SLEW, _, _, _, _),
	LPI_PINGROUP(16, LPI_NO_SLEW, _, _, _, _),
	LPI_PINGROUP(17, LPI_NO_SLEW, _, _, _, _),
	LPI_PINGROUP(18, LPI_NO_SLEW, slimbus_clk, swr_tx_clk, _, _),
	LPI_PINGROUP(19, LPI_NO_SLEW, swr_tx_data, audio_ref, _, _),
	LPI_PINGROUP(20, LPI_NO_SLEW, slimbus_data, swr_tx_data, _, _),
	LPI_PINGROUP(21, LPI_NO_SLEW, slimbus_data, swr_rx_clk, _, _),
	LPI_PINGROUP(22, LPI_NO_SLEW, slimbus_data, swr_rx_data, prim_mclk_a, _),
	LPI_PINGROUP(23, LPI_NO_SLEW, qua_mi2s_sclk, _, swr_rx_data, swr_tx_data),
	LPI_PINGROUP(24, LPI_NO_SLEW, qua_mi2s_ws, cdc_pdm_rx, _, _),
	LPI_PINGROUP(25, LPI_NO_SLEW, qua_mi2s_data, _, _, _),
	LPI_PINGROUP(26, LPI_NO_SLEW, qua_mi2s_data, dmic1_clk, _, _),
	LPI_PINGROUP(27, LPI_NO_SLEW, qua_mi2s_data, dmic1_data, qdss_cti, _),
	LPI_PINGROUP(28, LPI_NO_SLEW, qua_mi2s_data, dmic2_clk, gp_pdm, qdss_cti),
	LPI_PINGROUP(29, LPI_NO_SLEW, prim_mclk_b, dmic2_data, _, _),
	LPI_PINGROUP(30, LPI_NO_SLEW, qca_sb_data, _, _, _),
	LPI_PINGROUP(31, LPI_NO_SLEW, qca_sb_clk, _, _, _),
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
	//.flags = LPI_FLAG_SLEW_RATE_SAME_REG,
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
