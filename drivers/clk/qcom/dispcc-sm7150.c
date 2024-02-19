// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2018, The Linux Foundation. All rights reserved.
 * Copyright (c) 2024, Danila Tikhonov <danila@jiaxyga.com>
 * Copyright (c) 2024, David Wronek <david@mainlining.org>
 */

#include <linux/clk-provider.h>
#include <linux/mod_devicetable.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>

#include <dt-bindings/clock/qcom,sm7150-dispcc.h>

#include "clk-alpha-pll.h"
#include "clk-branch.h"
#include "clk-rcg.h"
#include "clk-regmap.h"
#include "clk-regmap-divider.h"
#include "common.h"
#include "gdsc.h"

enum {
	DT_BI_TCXO,
	DT_BI_TCXO_AO,
	DT_GCC_DISP_GPLL0_CLK,
	DT_CHIP_SLEEP_CLK,
	DT_DSI0_PHY_PLL_OUT_BYTECLK,
	DT_DSI0_PHY_PLL_OUT_DSICLK,
	DT_DSI1_PHY_PLL_OUT_BYTECLK,
	DT_DSI1_PHY_PLL_OUT_DSICLK,
	DT_DP_PHY_PLL_LINK_CLK,
	DT_DP_PHY_PLL_VCO_DIV_CLK,
};

enum {
	P_BI_TCXO,
	P_CHIP_SLEEP_CLK,
	P_DISP_CC_PLL0_OUT_EVEN,
	P_DISP_CC_PLL0_OUT_MAIN,
	P_DP_PHY_PLL_LINK_CLK,
	P_DP_PHY_PLL_VCO_DIV_CLK,
	P_DSI0_PHY_PLL_OUT_BYTECLK,
	P_DSI0_PHY_PLL_OUT_DSICLK,
	P_DSI1_PHY_PLL_OUT_BYTECLK,
	P_DSI1_PHY_PLL_OUT_DSICLK,
	P_GCC_DISP_GPLL0_CLK,
};

static const struct pll_vco fabia_vco[] = {
	{ 249600000, 2000000000, 0 },
	{ 125000000, 1000000000, 1 },
};

/* 860MHz configuration */
static const struct alpha_pll_config disp_cc_pll0_config = {
	.l = 0x2c,
	.alpha = 0xcaaa,
	.test_ctl_val = 0x40000000,
};

static struct clk_alpha_pll disp_cc_pll0 = {
	.offset = 0x0,
	.vco_table = fabia_vco,
	.num_vco = ARRAY_SIZE(fabia_vco),
	.regs = clk_alpha_pll_regs[CLK_ALPHA_PLL_TYPE_FABIA],
	.clkr = {
		.hw.init = &(struct clk_init_data){
			.name = "disp_cc_pll0",
			.parent_data = &(const struct clk_parent_data){
				.index = DT_BI_TCXO,
			},
			.num_parents = 1,
			.ops = &clk_alpha_pll_fabia_ops,
		},
	},
};

static const struct parent_map disp_cc_parent_map_0[] = {
	{ P_BI_TCXO, 0 },
	{ P_DSI0_PHY_PLL_OUT_BYTECLK, 1 },
	{ P_DSI1_PHY_PLL_OUT_BYTECLK, 2 },
};

static const struct clk_parent_data disp_cc_parent_data_0[] = {
	{ .index = DT_BI_TCXO },
	{ .index = DT_DSI0_PHY_PLL_OUT_BYTECLK },
	{ .index = DT_DSI1_PHY_PLL_OUT_BYTECLK },
};

static const struct parent_map disp_cc_parent_map_1[] = {
	{ P_BI_TCXO, 0 },
	{ P_DP_PHY_PLL_LINK_CLK, 1 },
	{ P_DP_PHY_PLL_VCO_DIV_CLK, 2 },
};

static const struct clk_parent_data disp_cc_parent_data_1[] = {
	{ .index = DT_BI_TCXO },
	{ .index = DT_DP_PHY_PLL_LINK_CLK },
	{ .index = DT_DP_PHY_PLL_VCO_DIV_CLK },
};

static const struct parent_map disp_cc_parent_map_2[] = {
	{ P_BI_TCXO, 0 },
};

static const struct clk_parent_data disp_cc_parent_data_2[] = {
	{ .index = DT_BI_TCXO },
};

static const struct clk_parent_data disp_cc_parent_data_2_ao[] = {
	{ .index = DT_BI_TCXO_AO },
};

static const struct parent_map disp_cc_parent_map_3[] = {
	{ P_BI_TCXO, 0 },
	{ P_DISP_CC_PLL0_OUT_MAIN, 1 },
	{ P_GCC_DISP_GPLL0_CLK, 4 },
	{ P_DISP_CC_PLL0_OUT_EVEN, 5 },
};

static const struct clk_parent_data disp_cc_parent_data_3[] = {
	{ .index = DT_BI_TCXO },
	{ .hw = &disp_cc_pll0.clkr.hw },
	{ .index = DT_GCC_DISP_GPLL0_CLK },
	{ .hw = &disp_cc_pll0.clkr.hw },
};

static const struct parent_map disp_cc_parent_map_4[] = {
	{ P_BI_TCXO, 0 },
	{ P_DSI0_PHY_PLL_OUT_DSICLK, 1 },
	{ P_DSI1_PHY_PLL_OUT_DSICLK, 2 },
};

static const struct clk_parent_data disp_cc_parent_data_4[] = {
	{ .index = DT_BI_TCXO },
	{ .index = DT_DSI0_PHY_PLL_OUT_DSICLK },
	{ .index = DT_DSI1_PHY_PLL_OUT_DSICLK },
};

static const struct parent_map disp_cc_parent_map_5[] = {
	{ P_BI_TCXO, 0 },
	{ P_GCC_DISP_GPLL0_CLK, 4 },
};

static const struct clk_parent_data disp_cc_parent_data_5[] = {
	{ .index = DT_BI_TCXO },
	{ .index = DT_GCC_DISP_GPLL0_CLK },
};

static const struct parent_map disp_cc_parent_map_6[] = {
	{ P_CHIP_SLEEP_CLK, 0 },
};

static const struct clk_parent_data disp_cc_parent_data_6[] = {
	{ .index = DT_CHIP_SLEEP_CLK },
};

static const struct freq_tbl ftbl_disp_cc_mdss_ahb_clk_src[] = {
	F(19200000, P_BI_TCXO, 1, 0, 0),
	F(37500000, P_GCC_DISP_GPLL0_CLK, 16, 0, 0),
	F(75000000, P_GCC_DISP_GPLL0_CLK, 8, 0, 0),
	{ }
};

static struct clk_rcg2 disp_cc_mdss_ahb_clk_src = {
	.cmd_rcgr = 0x22bc,
	.mnd_width = 0,
	.hid_width = 5,
	.parent_map = disp_cc_parent_map_5,
	.freq_tbl = ftbl_disp_cc_mdss_ahb_clk_src,
	.clkr.hw.init = &(const struct clk_init_data) {
		.name = "disp_cc_mdss_ahb_clk_src",
		.parent_data = disp_cc_parent_data_5,
		.num_parents = ARRAY_SIZE(disp_cc_parent_data_5),
		.flags = CLK_SET_RATE_PARENT,
		.ops = &clk_rcg2_shared_ops,
	},
};

static const struct freq_tbl ftbl_disp_cc_mdss_byte0_clk_src[] = {
	F(19200000, P_BI_TCXO, 1, 0, 0),
	{ }
};

static struct clk_rcg2 disp_cc_mdss_byte0_clk_src = {
	.cmd_rcgr = 0x2110,
	.mnd_width = 0,
	.hid_width = 5,
	.parent_map = disp_cc_parent_map_0,
	.clkr.hw.init = &(const struct clk_init_data) {
		.name = "disp_cc_mdss_byte0_clk_src",
		.parent_data = disp_cc_parent_data_0,
		.num_parents = ARRAY_SIZE(disp_cc_parent_data_0),
		.flags = CLK_SET_RATE_PARENT | CLK_GET_RATE_NOCACHE,
		.ops = &clk_byte2_ops,
	},
};

static struct clk_rcg2 disp_cc_mdss_byte1_clk_src = {
	.cmd_rcgr = 0x212c,
	.mnd_width = 0,
	.hid_width = 5,
	.parent_map = disp_cc_parent_map_0,
	.clkr.hw.init = &(const struct clk_init_data) {
		.name = "disp_cc_mdss_byte1_clk_src",
		.parent_data = disp_cc_parent_data_0,
		.num_parents = ARRAY_SIZE(disp_cc_parent_data_0),
		.flags = CLK_SET_RATE_PARENT | CLK_GET_RATE_NOCACHE,
		.ops = &clk_byte2_ops,
	},
};

static struct clk_rcg2 disp_cc_mdss_dp_aux_clk_src = {
	.cmd_rcgr = 0x21dc,
	.mnd_width = 0,
	.hid_width = 5,
	.parent_map = disp_cc_parent_map_2,
	.freq_tbl = ftbl_disp_cc_mdss_byte0_clk_src,
	.clkr.hw.init = &(const struct clk_init_data) {
		.name = "disp_cc_mdss_dp_aux_clk_src",
		.parent_data = disp_cc_parent_data_2,
		.num_parents = ARRAY_SIZE(disp_cc_parent_data_2),
		.ops = &clk_rcg2_ops,
	},
};

static const struct freq_tbl ftbl_disp_cc_mdss_dp_crypto_clk_src[] = {
	F(108000, P_DP_PHY_PLL_LINK_CLK, 3, 0, 0),
	F(180000, P_DP_PHY_PLL_LINK_CLK, 3, 0, 0),
	F(360000, P_DP_PHY_PLL_LINK_CLK, 1.5, 0, 0),
	F(540000, P_DP_PHY_PLL_LINK_CLK, 1.5, 0, 0),
	{ }
};

static struct clk_rcg2 disp_cc_mdss_dp_crypto_clk_src = {
	.cmd_rcgr = 0x2194,
	.mnd_width = 0,
	.hid_width = 5,
	.parent_map = disp_cc_parent_map_1,
	.freq_tbl = ftbl_disp_cc_mdss_dp_crypto_clk_src,
	.clkr.hw.init = &(const struct clk_init_data) {
		.name = "disp_cc_mdss_dp_crypto_clk_src",
		.parent_data = disp_cc_parent_data_1,
		.num_parents = ARRAY_SIZE(disp_cc_parent_data_1),
		.flags = CLK_GET_RATE_NOCACHE,
		.ops = &clk_rcg2_ops,
	},
};

static const struct freq_tbl ftbl_disp_cc_mdss_dp_link_clk_src[] = {
	F(162000, P_DP_PHY_PLL_LINK_CLK, 1, 0, 0),
	F(270000, P_DP_PHY_PLL_LINK_CLK, 1, 0, 0),
	F(540000, P_DP_PHY_PLL_LINK_CLK, 1, 0, 0),
	F(810000, P_DP_PHY_PLL_LINK_CLK, 1, 0, 0),
	{ }
};

static struct clk_rcg2 disp_cc_mdss_dp_link_clk_src = {
	.cmd_rcgr = 0x2178,
	.mnd_width = 0,
	.hid_width = 5,
	.parent_map = disp_cc_parent_map_1,
	.freq_tbl = ftbl_disp_cc_mdss_dp_link_clk_src,
	.clkr.hw.init = &(const struct clk_init_data) {
		.name = "disp_cc_mdss_dp_link_clk_src",
		.parent_data = disp_cc_parent_data_1,
		.num_parents = ARRAY_SIZE(disp_cc_parent_data_1),
		.flags = CLK_SET_RATE_PARENT | CLK_GET_RATE_NOCACHE,
		.ops = &clk_rcg2_ops,
	},
};

static struct clk_rcg2 disp_cc_mdss_dp_pixel1_clk_src = {
	.cmd_rcgr = 0x21c4,
	.mnd_width = 16,
	.hid_width = 5,
	.parent_map = disp_cc_parent_map_1,
	.clkr.hw.init = &(const struct clk_init_data) {
		.name = "disp_cc_mdss_dp_pixel1_clk_src",
		.parent_data = disp_cc_parent_data_1,
		.num_parents = ARRAY_SIZE(disp_cc_parent_data_1),
		.flags = CLK_SET_RATE_PARENT | CLK_GET_RATE_NOCACHE,
		.ops = &clk_dp_ops,
	},
};

static struct clk_rcg2 disp_cc_mdss_dp_pixel_clk_src = {
	.cmd_rcgr = 0x21ac,
	.mnd_width = 16,
	.hid_width = 5,
	.parent_map = disp_cc_parent_map_1,
	.clkr.hw.init = &(const struct clk_init_data) {
		.name = "disp_cc_mdss_dp_pixel_clk_src",
		.parent_data = disp_cc_parent_data_1,
		.num_parents = ARRAY_SIZE(disp_cc_parent_data_1),
		.flags = CLK_SET_RATE_PARENT | CLK_GET_RATE_NOCACHE,
		.ops = &clk_dp_ops,
	},
};

static struct clk_rcg2 disp_cc_mdss_esc0_clk_src = {
	.cmd_rcgr = 0x2148,
	.mnd_width = 0,
	.hid_width = 5,
	.parent_map = disp_cc_parent_map_0,
	.freq_tbl = ftbl_disp_cc_mdss_byte0_clk_src,
	.clkr.hw.init = &(const struct clk_init_data) {
		.name = "disp_cc_mdss_esc0_clk_src",
		.parent_data = disp_cc_parent_data_0,
		.num_parents = ARRAY_SIZE(disp_cc_parent_data_0),
		.ops = &clk_rcg2_ops,
	},
};

static struct clk_rcg2 disp_cc_mdss_esc1_clk_src = {
	.cmd_rcgr = 0x2160,
	.mnd_width = 0,
	.hid_width = 5,
	.parent_map = disp_cc_parent_map_0,
	.freq_tbl = ftbl_disp_cc_mdss_byte0_clk_src,
	.clkr.hw.init = &(const struct clk_init_data) {
		.name = "disp_cc_mdss_esc1_clk_src",
		.parent_data = disp_cc_parent_data_0,
		.num_parents = ARRAY_SIZE(disp_cc_parent_data_0),
		.ops = &clk_rcg2_ops,
	},
};

static const struct freq_tbl ftbl_disp_cc_mdss_mdp_clk_src[] = {
	F(19200000, P_BI_TCXO, 1, 0, 0),
	F(85714286, P_GCC_DISP_GPLL0_CLK, 7, 0, 0),
	F(100000000, P_GCC_DISP_GPLL0_CLK, 6, 0, 0),
	F(150000000, P_GCC_DISP_GPLL0_CLK, 4, 0, 0),
	F(172000000, P_DISP_CC_PLL0_OUT_MAIN, 5, 0, 0),
	F(200000000, P_GCC_DISP_GPLL0_CLK, 3, 0, 0),
	F(286666667, P_DISP_CC_PLL0_OUT_MAIN, 3, 0, 0),
	F(300000000, P_GCC_DISP_GPLL0_CLK, 2, 0, 0),
	F(344000000, P_DISP_CC_PLL0_OUT_MAIN, 2.5, 0, 0),
	F(430000000, P_DISP_CC_PLL0_OUT_MAIN, 2, 0, 0),
	{ }
};

static struct clk_rcg2 disp_cc_mdss_mdp_clk_src = {
	.cmd_rcgr = 0x20c8,
	.mnd_width = 0,
	.hid_width = 5,
	.parent_map = disp_cc_parent_map_3,
	.freq_tbl = ftbl_disp_cc_mdss_mdp_clk_src,
	.clkr.hw.init = &(const struct clk_init_data) {
		.name = "disp_cc_mdss_mdp_clk_src",
		.parent_data = disp_cc_parent_data_3,
		.num_parents = ARRAY_SIZE(disp_cc_parent_data_3),
		.flags = CLK_SET_RATE_PARENT,
		.ops = &clk_rcg2_shared_ops,
	},
};

static struct clk_rcg2 disp_cc_mdss_pclk0_clk_src = {
	.cmd_rcgr = 0x2098,
	.mnd_width = 8,
	.hid_width = 5,
	.parent_map = disp_cc_parent_map_4,
	.clkr.hw.init = &(const struct clk_init_data) {
		.name = "disp_cc_mdss_pclk0_clk_src",
		.parent_data = disp_cc_parent_data_4,
		.num_parents = ARRAY_SIZE(disp_cc_parent_data_4),
		.flags = CLK_SET_RATE_PARENT | CLK_GET_RATE_NOCACHE,
		.ops = &clk_pixel_ops,
	},
};

static struct clk_rcg2 disp_cc_mdss_pclk1_clk_src = {
	.cmd_rcgr = 0x20b0,
	.mnd_width = 8,
	.hid_width = 5,
	.parent_map = disp_cc_parent_map_4,
	.clkr.hw.init = &(const struct clk_init_data) {
		.name = "disp_cc_mdss_pclk1_clk_src",
		.parent_data = disp_cc_parent_data_4,
		.num_parents = ARRAY_SIZE(disp_cc_parent_data_4),
		.flags = CLK_SET_RATE_PARENT | CLK_GET_RATE_NOCACHE,
		.ops = &clk_pixel_ops,
	},
};

static const struct freq_tbl ftbl_disp_cc_mdss_rot_clk_src[] = {
	F(19200000, P_BI_TCXO, 1, 0, 0),
	F(171428571, P_GCC_DISP_GPLL0_CLK, 3.5, 0, 0),
	F(200000000, P_GCC_DISP_GPLL0_CLK, 3, 0, 0),
	F(300000000, P_GCC_DISP_GPLL0_CLK, 2, 0, 0),
	F(344000000, P_DISP_CC_PLL0_OUT_MAIN, 2.5, 0, 0),
	F(430000000, P_DISP_CC_PLL0_OUT_MAIN, 2, 0, 0),
	{ }
};

static struct clk_rcg2 disp_cc_mdss_rot_clk_src = {
	.cmd_rcgr = 0x20e0,
	.mnd_width = 0,
	.hid_width = 5,
	.parent_map = disp_cc_parent_map_3,
	.freq_tbl = ftbl_disp_cc_mdss_rot_clk_src,
	.clkr.hw.init = &(const struct clk_init_data) {
		.name = "disp_cc_mdss_rot_clk_src",
		.parent_data = disp_cc_parent_data_3,
		.num_parents = ARRAY_SIZE(disp_cc_parent_data_3),
		.ops = &clk_rcg2_shared_ops,
	},
};

static struct clk_rcg2 disp_cc_mdss_vsync_clk_src = {
	.cmd_rcgr = 0x20f8,
	.mnd_width = 0,
	.hid_width = 5,
	.parent_map = disp_cc_parent_map_2,
	.freq_tbl = ftbl_disp_cc_mdss_byte0_clk_src,
	.clkr.hw.init = &(const struct clk_init_data) {
		.name = "disp_cc_mdss_vsync_clk_src",
		.parent_data = disp_cc_parent_data_2,
		.num_parents = ARRAY_SIZE(disp_cc_parent_data_2),
		.ops = &clk_rcg2_ops,
	},
};

static const struct freq_tbl ftbl_disp_cc_sleep_clk_src[] = {
	F(32000, P_CHIP_SLEEP_CLK, 1, 0, 0),
	{ }
};

static struct clk_rcg2 disp_cc_sleep_clk_src = {
	.cmd_rcgr = 0x6060,
	.mnd_width = 0,
	.hid_width = 5,
	.parent_map = disp_cc_parent_map_6,
	.freq_tbl = ftbl_disp_cc_sleep_clk_src,
	.clkr.hw.init = &(const struct clk_init_data) {
		.name = "disp_cc_sleep_clk_src",
		.parent_data = disp_cc_parent_data_6,
		.num_parents = ARRAY_SIZE(disp_cc_parent_data_6),
		.ops = &clk_rcg2_ops,
	},
};

static struct clk_rcg2 disp_cc_xo_clk_src = {
	.cmd_rcgr = 0x6044,
	.mnd_width = 0,
	.hid_width = 5,
	.parent_map = disp_cc_parent_map_2,
	.freq_tbl = ftbl_disp_cc_mdss_byte0_clk_src,
	.clkr.hw.init = &(const struct clk_init_data) {
		.name = "disp_cc_xo_clk_src",
		.parent_data = disp_cc_parent_data_2_ao,
		.num_parents = ARRAY_SIZE(disp_cc_parent_data_2_ao),
		.ops = &clk_rcg2_ops,
	},
};

static struct clk_branch disp_cc_mdss_ahb_clk = {
	.halt_reg = 0x2080,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x2080,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "disp_cc_mdss_ahb_clk",
			.parent_hws = (const struct clk_hw*[]){
				&disp_cc_mdss_ahb_clk_src.clkr.hw,
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch disp_cc_mdss_byte0_clk = {
	.halt_reg = 0x2028,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x2028,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "disp_cc_mdss_byte0_clk",
			.parent_hws = (const struct clk_hw*[]){
				&disp_cc_mdss_byte0_clk_src.clkr.hw,
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT | CLK_GET_RATE_NOCACHE,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_regmap_div disp_cc_mdss_byte0_div_clk_src = {
	.reg = 0x2128,
	.shift = 0,
	.width = 2,
	.clkr = {
		.hw.init = &(struct clk_init_data){
			.name = "disp_cc_mdss_byte0_div_clk_src",
			.parent_hws = (const struct clk_hw*[]){
				&disp_cc_mdss_byte0_clk_src.clkr.hw,
			},
			.num_parents = 1,
			.flags = CLK_GET_RATE_NOCACHE,
			.ops = &clk_regmap_div_ops,
		},
	},
};

static struct clk_branch disp_cc_mdss_byte0_intf_clk = {
	.halt_reg = 0x202c,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x202c,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "disp_cc_mdss_byte0_intf_clk",
			.parent_hws = (const struct clk_hw*[]){
				&disp_cc_mdss_byte0_div_clk_src.clkr.hw,
			},
			.num_parents = 1,
			.flags = CLK_GET_RATE_NOCACHE,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch disp_cc_mdss_byte1_clk = {
	.halt_reg = 0x2030,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x2030,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "disp_cc_mdss_byte1_clk",
			.parent_hws = (const struct clk_hw*[]){
				&disp_cc_mdss_byte1_clk_src.clkr.hw,
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT | CLK_GET_RATE_NOCACHE,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_regmap_div disp_cc_mdss_byte1_div_clk_src = {
	.reg = 0x2144,
	.shift = 0,
	.width = 2,
	.clkr = {
		.hw.init = &(struct clk_init_data){
			.name = "disp_cc_mdss_byte1_div_clk_src",
			.parent_hws = (const struct clk_hw*[]){
				&disp_cc_mdss_byte1_clk_src.clkr.hw,
			},
			.num_parents = 1,
			.flags = CLK_GET_RATE_NOCACHE,
			.ops = &clk_regmap_div_ops,
		},
	},
};

static struct clk_branch disp_cc_mdss_byte1_intf_clk = {
	.halt_reg = 0x2034,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x2034,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "disp_cc_mdss_byte1_intf_clk",
			.parent_hws = (const struct clk_hw*[]){
				&disp_cc_mdss_byte1_div_clk_src.clkr.hw,
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT | CLK_GET_RATE_NOCACHE,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch disp_cc_mdss_dp_aux_clk = {
	.halt_reg = 0x2054,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x2054,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "disp_cc_mdss_dp_aux_clk",
			.parent_hws = (const struct clk_hw*[]){
				&disp_cc_mdss_dp_aux_clk_src.clkr.hw,
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch disp_cc_mdss_dp_crypto_clk = {
	.halt_reg = 0x2048,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x2048,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "disp_cc_mdss_dp_crypto_clk",
			.parent_hws = (const struct clk_hw*[]){
				&disp_cc_mdss_dp_crypto_clk_src.clkr.hw,
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT | CLK_GET_RATE_NOCACHE,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch disp_cc_mdss_dp_link_clk = {
	.halt_reg = 0x2040,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x2040,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "disp_cc_mdss_dp_link_clk",
			.parent_hws = (const struct clk_hw*[]){
				&disp_cc_mdss_dp_link_clk_src.clkr.hw,
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT | CLK_GET_RATE_NOCACHE,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch disp_cc_mdss_dp_link_intf_clk = {
	.halt_reg = 0x2044,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x2044,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "disp_cc_mdss_dp_link_intf_clk",
			.parent_hws = (const struct clk_hw*[]){
				&disp_cc_mdss_dp_link_clk_src.clkr.hw,
			},
			.num_parents = 1,
			.flags = CLK_GET_RATE_NOCACHE,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch disp_cc_mdss_dp_pixel1_clk = {
	.halt_reg = 0x2050,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x2050,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "disp_cc_mdss_dp_pixel1_clk",
			.parent_hws = (const struct clk_hw*[]){
				&disp_cc_mdss_dp_pixel1_clk_src.clkr.hw,
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT | CLK_GET_RATE_NOCACHE,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch disp_cc_mdss_dp_pixel_clk = {
	.halt_reg = 0x204c,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x204c,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "disp_cc_mdss_dp_pixel_clk",
			.parent_hws = (const struct clk_hw*[]){
				&disp_cc_mdss_dp_pixel_clk_src.clkr.hw,
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT | CLK_GET_RATE_NOCACHE,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch disp_cc_mdss_esc0_clk = {
	.halt_reg = 0x2038,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x2038,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "disp_cc_mdss_esc0_clk",
			.parent_hws = (const struct clk_hw*[]){
				&disp_cc_mdss_esc0_clk_src.clkr.hw,
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch disp_cc_mdss_esc1_clk = {
	.halt_reg = 0x203c,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x203c,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "disp_cc_mdss_esc1_clk",
			.parent_hws = (const struct clk_hw*[]){
				&disp_cc_mdss_esc1_clk_src.clkr.hw,
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch disp_cc_mdss_mdp_clk = {
	.halt_reg = 0x200c,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x200c,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "disp_cc_mdss_mdp_clk",
			.parent_hws = (const struct clk_hw*[]){
				&disp_cc_mdss_mdp_clk_src.clkr.hw,
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch disp_cc_mdss_mdp_lut_clk = {
	.halt_reg = 0x201c,
	.halt_check = BRANCH_VOTED,
	.clkr = {
		.enable_reg = 0x201c,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "disp_cc_mdss_mdp_lut_clk",
			.parent_hws = (const struct clk_hw*[]){
				&disp_cc_mdss_mdp_clk_src.clkr.hw,
			},
			.num_parents = 1,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch disp_cc_mdss_non_gdsc_ahb_clk = {
	.halt_reg = 0x4004,
	.halt_check = BRANCH_VOTED,
	.clkr = {
		.enable_reg = 0x4004,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "disp_cc_mdss_non_gdsc_ahb_clk",
			.parent_hws = (const struct clk_hw*[]){
				&disp_cc_mdss_ahb_clk_src.clkr.hw,
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch disp_cc_mdss_pclk0_clk = {
	.halt_reg = 0x2004,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x2004,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "disp_cc_mdss_pclk0_clk",
			.parent_hws = (const struct clk_hw*[]){
				&disp_cc_mdss_pclk0_clk_src.clkr.hw,
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT | CLK_GET_RATE_NOCACHE,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch disp_cc_mdss_pclk1_clk = {
	.halt_reg = 0x2008,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x2008,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "disp_cc_mdss_pclk1_clk",
			.parent_hws = (const struct clk_hw*[]){
				&disp_cc_mdss_pclk1_clk_src.clkr.hw,
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT | CLK_GET_RATE_NOCACHE,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch disp_cc_mdss_rot_clk = {
	.halt_reg = 0x2014,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x2014,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "disp_cc_mdss_rot_clk",
			.parent_hws = (const struct clk_hw*[]){
				&disp_cc_mdss_rot_clk_src.clkr.hw,
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch disp_cc_mdss_rscc_ahb_clk = {
	.halt_reg = 0x400c,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x400c,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "disp_cc_mdss_rscc_ahb_clk",
			.parent_names = (const char *[]){
				"disp_cc_mdss_ahb_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch disp_cc_mdss_rscc_vsync_clk = {
	.halt_reg = 0x4008,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x4008,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "disp_cc_mdss_rscc_vsync_clk",
			.parent_hws = (const struct clk_hw*[]){
				&disp_cc_mdss_vsync_clk_src.clkr.hw,
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch disp_cc_mdss_vsync_clk = {
	.halt_reg = 0x2024,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x2024,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "disp_cc_mdss_vsync_clk",
			.parent_hws = (const struct clk_hw*[]){
				&disp_cc_mdss_vsync_clk_src.clkr.hw,
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch disp_cc_sleep_clk = {
	.halt_reg = 0x6078,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x6078,
		.enable_mask = BIT(0),
		.hw.init = &(struct clk_init_data){
			.name = "disp_cc_sleep_clk",
			.parent_names = (const char *[]){
				"disp_cc_sleep_clk_src",
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct gdsc mdss_gdsc = {
	.gdscr = 0x3000,
	.en_rest_wait_val = 0x2,
	.en_few_wait_val = 0x2,
	.clk_dis_wait_val = 0xf,
	.pd = {
		.name = "mdss_gdsc",
	},
	.pwrsts = PWRSTS_OFF_ON,
	.flags = HW_CTRL,
};

static struct clk_regmap *disp_cc_sm7150_clocks[] = {
	[DISP_CC_MDSS_AHB_CLK] = &disp_cc_mdss_ahb_clk.clkr,
	[DISP_CC_MDSS_AHB_CLK_SRC] = &disp_cc_mdss_ahb_clk_src.clkr,
	[DISP_CC_MDSS_BYTE0_CLK] = &disp_cc_mdss_byte0_clk.clkr,
	[DISP_CC_MDSS_BYTE0_CLK_SRC] = &disp_cc_mdss_byte0_clk_src.clkr,
	[DISP_CC_MDSS_BYTE0_DIV_CLK_SRC] = &disp_cc_mdss_byte0_div_clk_src.clkr,
	[DISP_CC_MDSS_BYTE0_INTF_CLK] = &disp_cc_mdss_byte0_intf_clk.clkr,
	[DISP_CC_MDSS_BYTE1_CLK] = &disp_cc_mdss_byte1_clk.clkr,
	[DISP_CC_MDSS_BYTE1_CLK_SRC] = &disp_cc_mdss_byte1_clk_src.clkr,
	[DISP_CC_MDSS_BYTE1_DIV_CLK_SRC] = &disp_cc_mdss_byte1_div_clk_src.clkr,
	[DISP_CC_MDSS_BYTE1_INTF_CLK] = &disp_cc_mdss_byte1_intf_clk.clkr,
	[DISP_CC_MDSS_DP_AUX_CLK] = &disp_cc_mdss_dp_aux_clk.clkr,
	[DISP_CC_MDSS_DP_AUX_CLK_SRC] = &disp_cc_mdss_dp_aux_clk_src.clkr,
	[DISP_CC_MDSS_DP_CRYPTO_CLK] = &disp_cc_mdss_dp_crypto_clk.clkr,
	[DISP_CC_MDSS_DP_CRYPTO_CLK_SRC] = &disp_cc_mdss_dp_crypto_clk_src.clkr,
	[DISP_CC_MDSS_DP_LINK_CLK] = &disp_cc_mdss_dp_link_clk.clkr,
	[DISP_CC_MDSS_DP_LINK_CLK_SRC] = &disp_cc_mdss_dp_link_clk_src.clkr,
	[DISP_CC_MDSS_DP_LINK_INTF_CLK] = &disp_cc_mdss_dp_link_intf_clk.clkr,
	[DISP_CC_MDSS_DP_PIXEL1_CLK] = &disp_cc_mdss_dp_pixel1_clk.clkr,
	[DISP_CC_MDSS_DP_PIXEL1_CLK_SRC] = &disp_cc_mdss_dp_pixel1_clk_src.clkr,
	[DISP_CC_MDSS_DP_PIXEL_CLK] = &disp_cc_mdss_dp_pixel_clk.clkr,
	[DISP_CC_MDSS_DP_PIXEL_CLK_SRC] = &disp_cc_mdss_dp_pixel_clk_src.clkr,
	[DISP_CC_MDSS_ESC0_CLK] = &disp_cc_mdss_esc0_clk.clkr,
	[DISP_CC_MDSS_ESC0_CLK_SRC] = &disp_cc_mdss_esc0_clk_src.clkr,
	[DISP_CC_MDSS_ESC1_CLK] = &disp_cc_mdss_esc1_clk.clkr,
	[DISP_CC_MDSS_ESC1_CLK_SRC] = &disp_cc_mdss_esc1_clk_src.clkr,
	[DISP_CC_MDSS_MDP_CLK] = &disp_cc_mdss_mdp_clk.clkr,
	[DISP_CC_MDSS_MDP_CLK_SRC] = &disp_cc_mdss_mdp_clk_src.clkr,
	[DISP_CC_MDSS_MDP_LUT_CLK] = &disp_cc_mdss_mdp_lut_clk.clkr,
	[DISP_CC_MDSS_NON_GDSC_AHB_CLK] = &disp_cc_mdss_non_gdsc_ahb_clk.clkr,
	[DISP_CC_MDSS_PCLK0_CLK] = &disp_cc_mdss_pclk0_clk.clkr,
	[DISP_CC_MDSS_PCLK0_CLK_SRC] = &disp_cc_mdss_pclk0_clk_src.clkr,
	[DISP_CC_MDSS_PCLK1_CLK] = &disp_cc_mdss_pclk1_clk.clkr,
	[DISP_CC_MDSS_PCLK1_CLK_SRC] = &disp_cc_mdss_pclk1_clk_src.clkr,
	[DISP_CC_MDSS_ROT_CLK] = &disp_cc_mdss_rot_clk.clkr,
	[DISP_CC_MDSS_ROT_CLK_SRC] = &disp_cc_mdss_rot_clk_src.clkr,
	[DISP_CC_MDSS_RSCC_AHB_CLK] = &disp_cc_mdss_rscc_ahb_clk.clkr,
	[DISP_CC_MDSS_RSCC_VSYNC_CLK] = &disp_cc_mdss_rscc_vsync_clk.clkr,
	[DISP_CC_MDSS_VSYNC_CLK] = &disp_cc_mdss_vsync_clk.clkr,
	[DISP_CC_MDSS_VSYNC_CLK_SRC] = &disp_cc_mdss_vsync_clk_src.clkr,
	[DISP_CC_PLL0] = &disp_cc_pll0.clkr,
	[DISP_CC_SLEEP_CLK] = &disp_cc_sleep_clk.clkr,
	[DISP_CC_SLEEP_CLK_SRC] = &disp_cc_sleep_clk_src.clkr,
	[DISP_CC_XO_CLK_SRC] = &disp_cc_xo_clk_src.clkr,
};

static struct gdsc *disp_cc_sm7150_gdscs[] = {
	[MDSS_GDSC] = &mdss_gdsc,
};

static const struct regmap_config disp_cc_sm7150_regmap_config = {
	.reg_bits	= 32,
	.reg_stride	= 4,
	.val_bits	= 32,
	.max_register	= 0x10000,
	.fast_io	= true,
};

static const struct qcom_cc_desc disp_cc_sm7150_desc = {
	.config = &disp_cc_sm7150_regmap_config,
	.clks = disp_cc_sm7150_clocks,
	.num_clks = ARRAY_SIZE(disp_cc_sm7150_clocks),
	.gdscs = disp_cc_sm7150_gdscs,
	.num_gdscs = ARRAY_SIZE(disp_cc_sm7150_gdscs),
};

static const struct of_device_id disp_cc_sm7150_match_table[] = {
	{ .compatible = "qcom,sm7150-dispcc" },
	{ }
};
MODULE_DEVICE_TABLE(of, disp_cc_sm7150_match_table);

static int disp_cc_sm7150_probe(struct platform_device *pdev)
{
	struct regmap *regmap;

	regmap = qcom_cc_map(pdev, &disp_cc_sm7150_desc);
	if (IS_ERR(regmap))
		return PTR_ERR(regmap);

	clk_fabia_pll_configure(&disp_cc_pll0, regmap, &disp_cc_pll0_config);
	/* Enable clock gating for DSI and MDP clocks */
	regmap_update_bits(regmap, 0x8000, 0x7f0, 0x7f0);

	/* Keep some clocks always-on */
	qcom_branch_set_clk_en(regmap, 0x605c); /* DISP_CC_XO_CLK */

	return qcom_cc_really_probe(pdev, &disp_cc_sm7150_desc, regmap);
}

static struct platform_driver disp_cc_sm7150_driver = {
	.probe = disp_cc_sm7150_probe,
	.driver = {
		.name = "disp_cc-sm7150",
		.of_match_table = disp_cc_sm7150_match_table,
	},
};

module_platform_driver(disp_cc_sm7150_driver);

MODULE_DESCRIPTION("Qualcomm SM7150 Display Clock Controller");
MODULE_LICENSE("GPL");
