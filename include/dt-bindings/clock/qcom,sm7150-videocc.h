/* SPDX-License-Identifier: (GPL-2.0-only OR BSD-2-Clause) */
/*
 * Copyright (c) 2018, The Linux Foundation. All rights reserved.
 * Copyright (c) 2024, Danila Tikhonov <danila@jiaxyga.com>
 */

#ifndef _DT_BINDINGS_CLK_QCOM_VIDEO_CC_SM7150_H
#define _DT_BINDINGS_CLK_QCOM_VIDEO_CC_SM7150_H

#define VIDEO_PLL0			0
#define VIDEO_CC_IRIS_AHB_CLK		1
#define VIDEO_CC_IRIS_CLK_SRC		2
#define VIDEO_CC_MVS0_AXI_CLK		3
#define VIDEO_CC_MVS0_CORE_CLK		4
#define VIDEO_CC_MVS1_AXI_CLK		5
#define VIDEO_CC_MVS1_CORE_CLK		6
#define VIDEO_CC_MVSC_CORE_CLK		7
#define VIDEO_CC_MVSC_CTL_AXI_CLK	8
#define VIDEO_CC_VENUS_AHB_CLK		9
#define VIDEO_CC_XO_CLK			10
#define VIDEO_CC_XO_CLK_SRC		11

/* VIDEO_CC GDSCRs */
#define VENUS_GDSC			0
#define VCODEC0_GDSC			1
#define VCODEC1_GDSC			2

#endif
