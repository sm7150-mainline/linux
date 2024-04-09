// SPDX-License-Identifier: GPL-2.0-only
/*
 * Driver for Samsung S5K3L6XX 1/3" 13M CMOS Image Sensor.
 *
 * Copyright (C) 2020-2021 Purism SPC
 *
 * Based on S5K5BAF driver
 * Copyright (C) 2013, Samsung Electronics Co., Ltd.
 * Andrzej Hajda <a.hajda@samsung.com>
 *
 * Based on S5K6AA driver authored by Sylwester Nawrocki
 * Copyright (C) 2013, Samsung Electronics Co., Ltd.
 */

#include <linux/clk.h>
#include <linux/debugfs.h>
#include <linux/delay.h>
#include <linux/firmware.h>
#include <linux/gpio/consumer.h>
#include <linux/i2c.h>
#include <linux/media.h>
#include <linux/module.h>
#include <linux/of_graph.h>
#include <linux/regulator/consumer.h>
#include <linux/pm_runtime.h>
#include <linux/pm.h>
#include <linux/slab.h>

#include <media/media-entity.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-device.h>
#include <media/v4l2-subdev.h>
#include <media/v4l2-mediabus.h>
#include <media/v4l2-fwnode.h>

static int debug;
module_param(debug, int, 0644);

#define S5K3L6XX_DRIVER_NAME		"s5k3l6xx"
#define S5K3L6XX_DEFAULT_MCLK_FREQ	24000000U
#define S5K3L6XX_CLK_NAME		"mclk"

#define S5K3L6XX_REG_MODEL_ID_L		0x0000
#define S5K3L6XX_REG_MODEL_ID_H		0x0001
#define S5K3L6XX_MODEL_ID_L		0x30
#define S5K3L6XX_MODEL_ID_H		0xc6

#define S5K3L6XX_REG_REVISION_NUMBER	0x0002
#define S5K3L6XX_REVISION_NUMBER	0xb0

#define S5K3L6XX_REG_FRAME_COUNT	0x0005
#define S5K3L6XX_REG_LANE_MODE		0x0114
#define S5K3L6XX_REG_FINE_INTEGRATION_TIME		0x0200 // 2 bytes
#define S5K3L6XX_REG_COARSE_INTEGRATION_TIME		0x0202 // 2 bytes
#define S5K3L6XX_REG_ANALOG_GAIN		0x0204 // 2 bytes
#define S5K3L6XX_REG_DIGITAL_GAIN		0x020e // 2 bytes

#define S5K3L6XX_REG_TEST_PATTERN_MODE	0x0601
#define S5K3L6XX_TEST_PATTERN_SOLID_COLOR	0x01
#define S5K3L6XX_TEST_PATTERN_COLOR_BAR	0x02

#define S5K3L6XX_REG_TEST_DATA_RED	0x0602
#define S5K3L6XX_REG_TEST_DATA_GREENR	0x0604
#define S5K3L6XX_REG_TEST_DATA_BLUE	0x0606
#define S5K3L6XX_REG_TEST_DATA_GREENB	0x0608

#define S5K3L6XX_REG_AF			0x3403
#define S5K3L6XX_REG_AF_BIT_FILTER	0b100

#define S5K3L6XX_REG_PLL_PD		0x3c1e

#define S5K3L6XX_REG_MODE_SELECT	0x100
#define S5K3L6XX_MODE_STREAMING		0x1
#define S5K3L6XX_MODE_STANDBY		0x0

#define S5K3L6XX_REG_DATA_FORMAT	0x0112
#define S5K3L6XX_DATA_FORMAT_RAW8	0x0808

#define S5K3L6XX_CIS_WIDTH		4208
#define S5K3L6XX_CIS_HEIGHT		3120


static const char * const s5k3l6xx_supply_names[] = {
	"vddio", /* Digital I/O (1.8V or 2.8V) */
	"vdda", /* Analog (2.8V) */
	"vddd", /* Digital Core (1.05V expected) */
};

#define S5K3L6XX_NUM_SUPPLIES ARRAY_SIZE(s5k3l6xx_supply_names)


struct s5k3l6xx_reg {
	u16 address;
	u16 val;
	// Size of a single write.
	u8 size;
};

// Downscaled 1:4 in both directions.
// Spans the entire sensor. Fps unknown.
// Relies on defaults to be set correctly.
static const struct s5k3l6xx_reg frame_1052x780px_8bit_xfps_2lane[] = {
	// extclk freq 25MHz (doesn't seem to matter)
	// FIXME: this is mclk, and should be set accordingly
	{ 0x0136, 0x1900,       2 },

	// x_output_size
	{ 0x034c, 0x041c,       2 },
	// line length in pixel clocks. at least x_output_size * 1.16
	// if using binning multiply x_output_size by the binning factor first
	{ 0x0342, 0x1980,       2 },
	// y_output_size
	{ 0x034e, 0x030c,       2 },

	// op_pll_multiplier, default 0064
	{ 0x030e, 0x0036,       2 },

	// y_addr_start
	{ 0x0346, 0x0000,       2 },
	// end = y_output_size * binning_factor + y_addr_start
	{ 0x034a, 0x0c30,       2 },
	// x_addr_start
	{ 0x0344, 0x0008,       2 },
	// end = x_output_size * binning_factor + x_addr_start - 1
	{ 0x0348, 0x1077,       2 },

	// binning enable
	{ 0x0900, 0x01, 1 },
	// type: 1/?x, 1/?y, full binning when matching skips
	{ 0x0901, 0x44, 1 },
	// y_odd_inc
	{ 0x0387, 0x07, 1 },

	// Noise reduction
	// The last 3 bits (0x0007) control some global brightness/noise pattern.
	// They work slightly differently depending on the value of 307b:80
	// Lower values seem to make analog gain behave in a non-linear way.
	{ 0x3074, 0x0977, 2},
};

// Downscaled 1:2 in both directions.
// Spans the entire sensor. Fps unknown.
// Relies on defaults to be set correctly.
static const struct s5k3l6xx_reg frame_2104x1560px_8bit_xfps_2lane[] = {
	// extclk freq 25MHz (doesn't seem to matter)
	{ 0x0136, 0x1900,       2 },

	// x_output_size
	{ 0x034c, 0x0838,       2 },
	// y_output_size
	{ 0x034e, 0x0618,       2 },
	// op_pll_multiplier, default 0064
	// 0036 is ok when 175MHz selected on mipi side (IMX8MQ_CLK_CSI2_PHY_REF), although it makes the source clock 216MHz (double-check)
	// 0042 ok for 200MHz
	// 0052 ok for 250MHz
	{ 0x030e, 0x0053,       2 },

	// y_addr_start
	{ 0x0346, 0x0000,       2 },
	// end
	{ 0x034a, 0x0c30,       2 },
	// x_addr_start
	{ 0x0344, 0x0000,       2 },
	// end to match sensor
	{ 0x0348, 0x1068,       2 },

	// binning in 1:2 mode seems to average out focus pixels.
	// binning enable
	{ 0x0900, 0x01, 1 },
	// type: 1/?x, 1/?y, full binning when matching skips
	{ 0x0901, 0x22, 1 },
	// x binning skips 8-pixel bocks, making it useless
	// y_odd_inc
	{ 0x0387, 0x03, 1 },

	// Noise reduction
	// The last 3 bits (0x0007) control some global brightness/noise pattern.
	// They work slightly differently depending on the value of 307b:80
	// 0x0972 makes focus pixels appear.
	// Lower values seem to make analog gain behave in a non-linear way.
	{ 0x3074, 0x0977, 2}, // 74, 75, 76, 77 all good for binning 1:2.

	// filter out autofocus pixels
	// FIXME: this should be behind a custom control instead
	{ 0x3403, 0x42 | S5K3L6XX_REG_AF_BIT_FILTER, 1 },
};

// Not scaled.
// Spans the entire sensor. Fps unknown.
// Relies on defaults to be set correctly.
static const struct s5k3l6xx_reg frame_4208x3120px_8bit_xfps_2lane[] = {
	// extclk freq (doesn't actually matter)
	{ 0x0136, 0x1900,       2 },

	// x_output_size
	{ 0x034c, 0x1070,       2 },
	// y_output_size
	{ 0x034e, 0x0c30,       2 },
	// op_pll_multiplier, default 0064
	// 0036 is good (max) when 175MHz selected on mipi side, although it makes the source clock 216MHz (double-check)
	// 0042 ok for 200MHz
	// 0052 ok for 250MHz
	// 006c for 333Mhz
	{ 0x030e, 0x006c,       2 },

	// y_addr_start
	{ 0x0346, 0x0000,       2 },
	// end
	{ 0x034a, 0x0c30,       2 },
	// x_addr_start
	{ 0x0344, 0x0000,       2 },
	// end to match sensor
	{ 0x0348, 0x1068,       2 },
	// line length in pixel clocks
	{ 0x0342, 0x1980,       2 },

	// Noise reduction
	// The last 3 bits (0x0007) control some global brightness/noise pattern.
	// They work slightly differently depending on the value of 307b:80
	{ 0x3074, 0x0977, 2}, // 74, 75, 76, 77 all good for binning 1:!, might introduce banding.

	// filter out autofocus pixels
	// FIXME: this should be behind a custom control instead
	{ 0x3403, 0x42 | S5K3L6XX_REG_AF_BIT_FILTER, 1 },
};

#define PAD_CIS 0
#define NUM_CIS_PADS 1
#define NUM_ISP_PADS 1


struct s5k3l6xx_frame {
	char *name;
	u32 width;
	u32 height;
	u32 code; // Media bus code
	const struct s5k3l6xx_reg  *streamregs;
	u16 streamregcount;
	u32 mipi_multiplier; // MIPI clock multiplier coming from PLL config, before multiplying by ext_clock
};

struct s5k3l6xx_ctrls {
	struct v4l2_ctrl_handler handler;
	struct { /* Auto / manual white balance cluster */
		struct v4l2_ctrl *awb;
		struct v4l2_ctrl *gain_red;
		struct v4l2_ctrl *gain_blue;
	};
	struct { /* Mirror cluster */
		struct v4l2_ctrl *hflip;
		struct v4l2_ctrl *vflip;
	};
	struct { /* Auto exposure / manual exposure and gain cluster */
		struct v4l2_ctrl *auto_exp;
		struct v4l2_ctrl *exposure;
		struct v4l2_ctrl *analog_gain;
		struct v4l2_ctrl *digital_gain;
	};
	struct { /* Link properties */
		struct v4l2_ctrl *link_freq;
		struct v4l2_ctrl *pixel_rate;
	};
	struct { /* Frame properties */
		struct v4l2_ctrl *hblank;
		struct v4l2_ctrl *vblank;
	};
};

struct regstable_entry {
	u16 address;
	u8 value;
};

#define REGSTABLE_SIZE 4096

struct regstable {
	unsigned entry_count;
	struct regstable_entry entries[REGSTABLE_SIZE];
};

struct s5k3l6xx {
	struct gpio_desc *rst_gpio;
	struct regulator_bulk_data supplies[S5K3L6XX_NUM_SUPPLIES];
	enum v4l2_mbus_type bus_type;
	u8 nlanes;

	struct clk *clock;
	u32 mclk_frequency;

	struct v4l2_subdev cis_sd;
	struct media_pad cis_pad;

	struct v4l2_subdev sd;

	/* protects the struct members below */
	struct mutex lock;

	int error;

	/* Currently selected frame format */
	const struct s5k3l6xx_frame *frame_fmt;

	struct s5k3l6xx_ctrls ctrls;

	/* Solid color test pattern is in effect,
	 * write needs to happen after color choice writes.
	 * It doesn't seem that controls guarantee any order of application. */
	unsigned int apply_test_solid:1;

	unsigned int streaming:1;
	unsigned int apply_cfg:1;
	unsigned int apply_crop:1;
	unsigned int valid_auto_alg:1;
	unsigned int power;

	u8 debug_frame; // Enables any size, sets empty debug frame.
	/* For debug address temporary value */
	u16 debug_address;
	struct regstable debug_regs;
};

static const struct s5k3l6xx_reg no_regs[0] = {};

static const struct s5k3l6xx_frame s5k3l6xx_frame_debug = {
	.name = "debug_empty",
	.width = 640, .height = 480,
	.streamregs = no_regs,
	.streamregcount = 0,
	.code = MEDIA_BUS_FMT_SGRBG8_1X8,
};

// Frame sizes are only available in RAW, so this effectively replaces pixfmt.
// Supported frame configurations.
static const struct s5k3l6xx_frame s5k3l6xx_frames[] = {
	{
		.name = "1:4 8bpp ?fps",
		.width = 1052, .height = 780,
		.streamregs = frame_1052x780px_8bit_xfps_2lane,
		.streamregcount = ARRAY_SIZE(frame_1052x780px_8bit_xfps_2lane),

		.code = MEDIA_BUS_FMT_SGRBG8_1X8,
		.mipi_multiplier = 0x36 / 4,
	},
	{
		.name = "1:2 8bpp +fps",
		.width = 2104, .height = 1560,
		.streamregs = frame_2104x1560px_8bit_xfps_2lane,
		.streamregcount = ARRAY_SIZE(frame_2104x1560px_8bit_xfps_2lane),
		.code = MEDIA_BUS_FMT_SGRBG8_1X8,
		.mipi_multiplier = 0x53 / 4,
	},
	{
		.name = "1:1 8bpp ?fps",
		.width = 4208, .height = 3120,
		.streamregs = frame_4208x3120px_8bit_xfps_2lane,
		.streamregcount = ARRAY_SIZE(frame_4208x3120px_8bit_xfps_2lane),
		.code = MEDIA_BUS_FMT_SGRBG8_1X8,
		.mipi_multiplier = 0x33 / 4,
	},
};

static inline struct v4l2_subdev *ctrl_to_sd(struct v4l2_ctrl *ctrl)
{
	return &container_of(ctrl->handler, struct s5k3l6xx, ctrls.handler)->sd;
}

static inline bool s5k5baf_is_cis_subdev(struct v4l2_subdev *sd)
{
	return sd->entity.function == MEDIA_ENT_F_CAM_SENSOR;
}

static inline struct s5k3l6xx *to_s5k3l6xx(struct v4l2_subdev *sd)
{
	return container_of(sd, struct s5k3l6xx, sd);
}

struct s5k3l6xx_read_result {
	u8 value;
	int retcode;
};

static struct s5k3l6xx_read_result __s5k3l6xx_i2c_read(struct s5k3l6xx *state, u16 addr)
{
	struct i2c_client *c = v4l2_get_subdevdata(&state->sd);
	__be16 w;
	struct s5k3l6xx_read_result ret;
	struct i2c_msg msg[] = {
		{ .addr = c->addr, .flags = 0,
		  .len = 2, .buf = (u8 *)&w },
		{ .addr = c->addr, .flags = I2C_M_RD,
		  .len = 1, .buf = (u8 *)&ret.value },
	};
	w = cpu_to_be16(addr);
	ret.retcode = i2c_transfer(c->adapter, msg, 2);

	if (ret.retcode != 2) {
		v4l2_err(c, "i2c_read: error during transfer (%d)\n", ret.retcode);
	}
	return ret;
}

static struct s5k3l6xx_read_result s5k3l6xx_i2c_read(struct s5k3l6xx *state, u16 addr)
{
	struct i2c_client *c = v4l2_get_subdevdata(&state->sd);
	struct s5k3l6xx_read_result ret;
	ret = __s5k3l6xx_i2c_read(state, addr);
	v4l2_dbg(3, debug, c, "i2c_read: 0x%04x : 0x%02x\n", addr, ret.value);
	return ret;
}

static int s5k3l6xx_i2c_write(struct s5k3l6xx *state, u16 addr, u8 val)
{
	struct i2c_client *c = v4l2_get_subdevdata(&state->sd);
	u8 buf[3] = { addr >> 8, addr & 0xFF, val };

	struct i2c_msg msg[] = {
		{ .addr = c->addr, .flags = 0,
		  .len = 3, .buf = buf },
	};
	int ret;
	int actual;

	v4l2_dbg(3, debug, c, "i2c_write to 0x%04x : 0x%02x\n", addr, val);

	ret = i2c_transfer(c->adapter, msg, 1);

	if (ret != 1) {
		v4l2_err(c, "i2c_write: error during transfer (%d)\n", ret);
		return ret;
	}
	// Not sure if actually needed. So really debugging code at the moment.
	actual = s5k3l6xx_i2c_read(state, addr).value;
	if (actual != val)
		v4l2_err(c, "i2c_write: value didn't stick. 0x%04x = 0x%02x != 0x%02x", addr, actual, val);
	return 0;
}

static int s5k3l6xx_i2c_write2(struct s5k3l6xx *state, u16 addr, u16 val)
{
	struct i2c_client *c = v4l2_get_subdevdata(&state->sd);
	u8 buf[4] = { addr >> 8, addr & 0xFF, (val >> 8) & 0xff, val & 0xff };

	struct i2c_msg msg[] = {
		{ .addr = c->addr, .flags = 0,
		  .len = 4, .buf = buf },
	};
	int ret;

	v4l2_dbg(3, debug, c, "i2c_write to 0x%04x : 0x%04x\n", addr, val);

	ret = i2c_transfer(c->adapter, msg, 1);

	// 1 message expected
	if (ret != 1) {
		v4l2_err(c, "i2c_write: error during transfer (%d)\n", ret);
		return ret;
	}
	return 0;
}

static int s5k3l6xx_submit_regs(struct s5k3l6xx *state, const struct s5k3l6xx_reg *regs, u16 regcount) {
       unsigned i;
       int ret = 0;
       for (i = 0; i < regcount; i++) {
	       if (regs[i].size == 2)
		       ret = s5k3l6xx_i2c_write2(state, regs[i].address, regs[i].val);
	       else
		       ret = s5k3l6xx_i2c_write(state, regs[i].address, (u8)regs[i].val);
	       if (ret < 0)
		       break;
       }
       return ret;
}

static struct s5k3l6xx_read_result s5k3l6xx_read(struct s5k3l6xx *state, u16 addr)
{
	return s5k3l6xx_i2c_read(state, addr);
}

static int s5k3l6xx_write(struct s5k3l6xx *state, u16 addr, u8 val)
{
	return s5k3l6xx_i2c_write(state, addr, val);
}

static int s5k3l6xx_submit_regstable(struct s5k3l6xx *state, const struct regstable *regs)
{
	struct i2c_client *c = v4l2_get_subdevdata(&state->sd);
	unsigned i;
	int ret = 0;

	for (i = 0; i < regs->entry_count; i++) {
		u16 addr = regs->entries[i].address;
		u8 val = regs->entries[i].value;
		if (debug >= 5) {
			struct s5k3l6xx_read_result res = __s5k3l6xx_i2c_read(state, addr);
			if (res.retcode >= 0 && res.value != val) {
				v4l2_dbg(5, debug, c, "overwriting: 0x%04x : 0x%02x\n", addr, res.value);
			}
		}
		ret = s5k3l6xx_i2c_write(state, addr, val);
		if (ret < 0)
			break;
	}
	return ret;
}

static int s5k3l6xx_find_pixfmt(const struct v4l2_mbus_framefmt *mf)
{
	int i, c = -1;

	for (i = 0; i < ARRAY_SIZE(s5k3l6xx_frames); i++) {
		if ((mf->colorspace != V4L2_COLORSPACE_DEFAULT)
				&& (mf->colorspace != V4L2_COLORSPACE_RAW))
			continue;
		if ((mf->width != s5k3l6xx_frames[i].width) || (mf->height != s5k3l6xx_frames[i].height))
			continue;
		if (mf->code == s5k3l6xx_frames[i].code)
			return i;
	}
	return c;
}

static const struct s5k3l6xx_reg setstream[] = {
	{ S5K3L6XX_REG_DATA_FORMAT, S5K3L6XX_DATA_FORMAT_RAW8, 2 },
	// Noise reduction
	// Bit 0x0080 will create noise when off (by default)
	// Raises data pedestal to 15-16.
	{ 0x307a, 0x0d00, 2 },
};

static int s5k3l6xx_hw_set_config(struct s5k3l6xx *state) {
	const struct s5k3l6xx_frame *frame_fmt = state->frame_fmt;
	int ret;
	v4l2_dbg(3, debug, &state->sd, "Setting frame format %s", frame_fmt->name);
	ret = s5k3l6xx_submit_regs(state, frame_fmt->streamregs, frame_fmt->streamregcount);
	if (ret < 0)
		return ret;

	// This may mess up PLL settings...
	// If the above already enabled streaming (setfile A), we're also in trouble.
	ret = s5k3l6xx_submit_regs(state, setstream, ARRAY_SIZE(setstream));
	if (ret < 0)
		return ret;

	ret = s5k3l6xx_write(state, S5K3L6XX_REG_LANE_MODE, state->nlanes - 1);
	if (ret < 0)
		return ret;

	return s5k3l6xx_submit_regstable(state, &state->debug_regs);
}

static int s5k3l6xx_hw_set_test_pattern(struct s5k3l6xx *state, int id)
{
	return s5k3l6xx_write(state, S5K3L6XX_REG_TEST_PATTERN_MODE, (u8)id);
}

static int s5k3l6xx_power_on(struct s5k3l6xx *state)
{
	int ret;

	v4l2_dbg(1, debug, &state->sd, "power_ON\n");

	ret = regulator_bulk_enable(S5K3L6XX_NUM_SUPPLIES, state->supplies);
	if (ret < 0)
		goto err;

	usleep_range(10, 20);

	ret = clk_set_rate(state->clock, state->mclk_frequency);
	if (ret < 0)
		goto err_reg_dis;

	ret = clk_prepare_enable(state->clock);
	if (ret < 0)
		goto err_reg_dis;

	v4l2_dbg(1, debug, &state->sd, "ON. clock frequency: %ld\n",
		 clk_get_rate(state->clock));

	/* 1ms = 25000 cycles at 25MHz */
	usleep_range(1000, 1500);
	gpiod_set_value_cansleep(state->rst_gpio, 0);

	/*
	 * this additional reset-active is not documented but makes power-on
	 * more stable.
	 */
	usleep_range(400, 800);
	gpiod_set_value_cansleep(state->rst_gpio, 1);
	usleep_range(400, 800);
	gpiod_set_value_cansleep(state->rst_gpio, 0);

	/* t4+t5 delays (depending on coarse integration time) + safety margin */
	msleep(10);

	return 0;

err_reg_dis:
	regulator_bulk_disable(S5K3L6XX_NUM_SUPPLIES, state->supplies);
err:
	v4l2_err(&state->sd, "%s() failed (%d)\n", __func__, ret);
	return ret;
}

static int s5k3l6xx_power_off(struct s5k3l6xx *state)
{
	int ret;

	v4l2_dbg(1, debug, &state->sd, "power_OFF\n");

	state->apply_cfg = 0;
	state->apply_crop = 0;

	usleep_range(20, 40);

	gpiod_set_value_cansleep(state->rst_gpio, 1);

	if (!IS_ERR(state->clock))
		clk_disable_unprepare(state->clock);

	ret = regulator_bulk_disable(S5K3L6XX_NUM_SUPPLIES, state->supplies);
	if (ret < 0)
		v4l2_err(&state->sd, "failed to disable regulators\n");
	else
		v4l2_dbg(1, debug, &state->sd, "OFF\n");
	return 0;
}

/*
 * V4L2 subdev core and video operations
 */

static int s5k3l6xx_set_power(struct v4l2_subdev *sd, int on)
{
	struct s5k3l6xx *state = to_s5k3l6xx(sd);
	int ret = 0;

	mutex_lock(&state->lock);

	if (state->power != !on)
		goto out;

	if (on) {
		ret = s5k3l6xx_power_on(state);
		if (ret < 0)
			goto out;
		state->power++;
	} else {
		ret = s5k3l6xx_power_off(state);
		state->power--;
	}

out:
	mutex_unlock(&state->lock);
	return ret;
}

static int s5k3l6xx_hw_set_stream(struct s5k3l6xx *state, int enable)
{
	int ret;

	v4l2_dbg(3, debug, &state->sd, "set_stream %d", enable);

	if (!enable) {
		ret = s5k3l6xx_i2c_write(state, S5K3L6XX_REG_MODE_SELECT,
					 S5K3L6XX_MODE_STANDBY);
		if (ret)
			return ret;

		return 0;
	}

	ret = s5k3l6xx_i2c_write(state, S5K3L6XX_REG_PLL_PD, 0x01);
	if (ret)
		return ret;

	ret = s5k3l6xx_i2c_write(state, S5K3L6XX_REG_MODE_SELECT,
				 S5K3L6XX_MODE_STREAMING);
	if (ret)
		return ret;

	return s5k3l6xx_i2c_write(state, S5K3L6XX_REG_PLL_PD, 0x00);
}

static int s5k3l6xx_s_stream(struct v4l2_subdev *sd, int on)
{
	struct s5k3l6xx *state = to_s5k3l6xx(sd);
	struct i2c_client *c = v4l2_get_subdevdata(&state->sd);
	int ret = 0;

	if (state->streaming == !!on) {
		return 0;
	}

	if (on) {
		ret = pm_runtime_get_sync(&c->dev);
		if (ret < 0) {
			dev_err(&c->dev, "%s: pm_runtime_get failed: %d\n",
				__func__, ret);
			// Not actually sure why this is _noidle.
			// Because the device was not actually acquired?
			pm_runtime_put_noidle(&c->dev);
			return ret;
		}

		ret = v4l2_ctrl_handler_setup(&state->ctrls.handler);
		if (ret < 0)
			goto fail_ctrl;
		mutex_lock(&state->lock);
		ret = s5k3l6xx_hw_set_config(state);
		if (ret < 0)
			goto fail_to_start;
		ret = s5k3l6xx_hw_set_stream(state, 1);
		if (ret < 0)
			goto fail_to_start;
	} else {
		mutex_lock(&state->lock);
		ret = s5k3l6xx_hw_set_stream(state, 0);
		if (ret < 0) {
			mutex_unlock(&state->lock);
			return ret;
		}
		pm_runtime_put(&c->dev);
	}
	state->streaming = !state->streaming;
	mutex_unlock(&state->lock);
	return 0;
fail_to_start:
	mutex_unlock(&state->lock);
fail_ctrl:
	pm_runtime_put(&c->dev);
	dev_err(&c->dev, "failed to start stream: %d\n", ret);
	return ret;
}

/*
 * V4L2 subdev pad level and video operations
 */
static int s5k3l6xx_try_cis_format(struct v4l2_mbus_framefmt *mf)
{
	int pixfmt;
	const struct s5k3l6xx_frame *mode = v4l2_find_nearest_size(s5k3l6xx_frames,
				      ARRAY_SIZE(s5k3l6xx_frames),
				      width, height,
				      mf->width, mf->height);
	struct v4l2_mbus_framefmt candidate = *mf;
	candidate.width = mode->width;
	candidate.height = mode->height;

	pixfmt = s5k3l6xx_find_pixfmt(&candidate);
	if (pixfmt < 0)
		return pixfmt;

	mf->colorspace = V4L2_COLORSPACE_RAW;
	mf->code = s5k3l6xx_frames[pixfmt].code;
	mf->field = V4L2_FIELD_NONE;

	return pixfmt;
}

static int s5k3l6xx_init_state(struct v4l2_subdev *sd,
			     struct v4l2_subdev_state *sd_state)
{
	struct v4l2_mbus_framefmt *mf;

	mf = v4l2_subdev_state_get_format(sd_state, PAD_CIS);
	s5k3l6xx_try_cis_format(mf);
	return 0;
}

static int s5k3l6xx_enum_mbus_code(struct v4l2_subdev *sd,
				   struct v4l2_subdev_state *sd_state,
				   struct v4l2_subdev_mbus_code_enum *code)
{
	unsigned repeats[ARRAY_SIZE(s5k3l6xx_frames)] = {0};
	unsigned i, j;
	unsigned matching = 0;

	/* Find unique codes within the frame configs array.
	 * The algorithm is O(n^2), but there's only a handful of configs,
	 * meaning that it's unlikely to take a long time.
	 * The repeats array's size is determined at compile time.
	 */
	for (i = 0; i < ARRAY_SIZE(s5k3l6xx_frames); i++) {
		for (j = 0; j < i; j++) {
			if (s5k3l6xx_frames[j].code == s5k3l6xx_frames[i].code) {
				repeats[i]++;
			}
		}
	}

	for (i = 0; i < ARRAY_SIZE(s5k3l6xx_frames); i++) {
		if (repeats[i] != 0)
			continue;

		if (matching == code->index) {
			code->code = s5k3l6xx_frames[i].code;
			return 0;
		}
		matching++;
	}

	return -EINVAL;
}

static int s5k3l6xx_enum_frame_size(struct v4l2_subdev *sd,
				    struct v4l2_subdev_state *sd_state,
				    struct v4l2_subdev_frame_size_enum *fse)
{
	unsigned i;
	unsigned matching = 0;

	for (i = 0; i < ARRAY_SIZE(s5k3l6xx_frames); i++) {
		if (fse->code != s5k3l6xx_frames[i].code)
			continue;

		if (fse->index == matching) {
			fse->code = s5k3l6xx_frames[i].code;
			fse->min_width = s5k3l6xx_frames[i].width;
			fse->max_width = s5k3l6xx_frames[i].width;
			fse->max_height = s5k3l6xx_frames[i].height;
			fse->min_height = s5k3l6xx_frames[i].height;

			return 0;
		}
		matching++;
	}

	dev_dbg(sd->dev, "fsize i %d m %d", i, matching);

	return -EINVAL;
}

static void s5k3l6xx_get_current_cis_format(struct v4l2_subdev *sd, struct v4l2_mbus_framefmt *mf)
{
	struct s5k3l6xx *state = to_s5k3l6xx(sd);
	// FIXME: This won't work for debug mode,
	// which is meant to adjust to whatever userspace wants.
	// Maybe save what userspace set last.
	mf->width = state->frame_fmt->width;
	mf->height = state->frame_fmt->height;
	mf->code = state->frame_fmt->code;
	mf->colorspace = V4L2_COLORSPACE_RAW;
}

static int s5k3l6xx_get_fmt(struct v4l2_subdev *sd,
			    struct v4l2_subdev_state *sd_state,
			    struct v4l2_subdev_format *fmt)
{
	struct v4l2_mbus_framefmt *mf;

	if (fmt->which == V4L2_SUBDEV_FORMAT_TRY) {
		mf = v4l2_subdev_state_get_format(sd_state, fmt->pad);
		fmt->format = *mf;
		dev_dbg(sd->dev, "try mf %dx%d", mf->width, mf->height);
		return 0;
	}

	mf = &fmt->format;
	if (fmt->pad == PAD_CIS) {
		s5k3l6xx_get_current_cis_format(sd, mf);
		dev_dbg(sd->dev, "mf %dx%d", mf->width, mf->height);
		return 0;
	}
	dev_err(sd->dev, "Not a CIS pad! %d", fmt->pad);
	return 0;
}

/** Calculating the MIPI clock
 *
 * Cast:
 * - extclk (typically 25MHz)
 * - MIPI predivider (mipi_div)
 * - MIPI multiplier (mipi_multiplier)
 * - MIPI postscaler (mipi_scaler)
 *
 * Calculation:
 * extclk * mipi_multiplier * 2 ^ mipi_scaler / mipi_div
 *
 * With defaults:
 * 24 * 0x64 * 2 ^ 0 / 4
 * = 24 * 100 * 2 ^ 0 / 4
 * = 600 [MHz]
 */
#define MIPI_CLOCK(extclk, div, multiplier, scaler) ((extclk / div * multiplier) << scaler)

// FIXME: this assumes ext clock of 25MHz (as seen on the L5).
// It probably can't be computed at compile time
// because mclk is not fixed.
static const s64 s5k3l6xx_link_freqs_menu[] = {
	MIPI_CLOCK(25000000, 4, 0x33, 0), // 0x33 from 1:1 mode
	MIPI_CLOCK(25000000, 4, 0x36, 0), // 0x36 from 1:4 mode
	MIPI_CLOCK(25000000, 4, 0x53, 0), // 0x53 from 1:2 mode
	MIPI_CLOCK(24000000, 4, 0x64, 0), // all from defaults (600MHz, seems right? MIPI maxes out at 625 according to module datasheet)
};


static unsigned s5k3l6xx_calculate_pixel_rate(unsigned mipi_clock, u8 bits_per_pixel, u8 lanes_count) {
	return mipi_clock * 2 * lanes_count / bits_per_pixel;
}

static int s5k3l6xx_set_fmt(struct v4l2_subdev *sd,
			    struct v4l2_subdev_state *sd_state,
			    struct v4l2_subdev_format *fmt)
{
	struct v4l2_mbus_framefmt *mf = &fmt->format;
	struct s5k3l6xx *state = to_s5k3l6xx(sd);
	int pixfmt_idx = 0;
	unsigned mipi_clk;
	s32 i;

	mf->field = V4L2_FIELD_NONE;

	if (fmt->which == V4L2_SUBDEV_FORMAT_TRY) {
		*v4l2_subdev_state_get_format(sd_state, fmt->pad) = *mf;
		return 0;
	}

	mutex_lock(&state->lock);

	if (state->streaming) {
		mutex_unlock(&state->lock);
		return -EBUSY;
	}


	if (state->debug_frame) {
		state->frame_fmt = &s5k3l6xx_frame_debug;
		// Keep frame width/height as requested.
	} else {
		pixfmt_idx = s5k3l6xx_try_cis_format(mf);
		if (pixfmt_idx == -1) {
			v4l2_err(sd, "set_fmt choice unsupported");
			mutex_unlock(&state->lock);
			return -EINVAL; // could not find the format. Unsupported
		}
		state->frame_fmt = &s5k3l6xx_frames[pixfmt_idx];
		mf->width = state->frame_fmt->width;
		mf->height = state->frame_fmt->height;
	}

	mipi_clk = state->mclk_frequency * state->frame_fmt->mipi_multiplier;

	// Report the smallest freq larger than configured.
	// Freqs array must increase.
	for (i = ARRAY_SIZE(s5k3l6xx_link_freqs_menu); i > 0; i--)
		if (mipi_clk < s5k3l6xx_link_freqs_menu[i - 1])
			break;

	mf->code = state->frame_fmt->code;
	mf->colorspace = V4L2_COLORSPACE_RAW;
	mutex_unlock(&state->lock);

	/*
	Change of controls is conceptually atomic
	and should maybe be done within the same lock acquisition,
	but this ends up calling s_ctrl.
	s_ctrl can be called from an unguarded context,
	and it acquires the lock itself,
	so those two are placed outside of the lock to avoid deadlocking.
	*/
	__v4l2_ctrl_s_ctrl(state->ctrls.link_freq, i);
	__v4l2_ctrl_s_ctrl_int64(state->ctrls.pixel_rate,
				 (s64)s5k3l6xx_calculate_pixel_rate(mipi_clk, 8, state->nlanes));

	return 0;
}

enum selection_rect { R_CIS, R_CROP_SINK, R_COMPOSE, R_CROP_SOURCE, R_INVALID };


static struct v4l2_rect get_crop(const struct s5k3l6xx_frame *fmt)
{
	struct v4l2_rect ret = {
		.top = 0,
		.left = 0,
		.width = fmt->width,
		.height = fmt->height,
	};
	return ret;
}

static int s5k3l6xx_get_selection(struct v4l2_subdev *sd,
			       struct v4l2_subdev_state *sd_state,
			       struct v4l2_subdev_selection *sel)
{
	struct s5k3l6xx *state = to_s5k3l6xx(sd);

	// FIXME: does crop rectangle affect vblank/hblank?
	// If no, then it can be independent of mode (frame format).
	switch (sel->target) {
	case V4L2_SEL_TGT_CROP:
		mutex_lock(&state->lock);
		switch (sel->which) {
		case V4L2_SUBDEV_FORMAT_TRY:
			v4l2_subdev_state_get_crop(sd_state, sel->pad);
			break;
		case V4L2_SUBDEV_FORMAT_ACTIVE:
			sel->r = get_crop(state->frame_fmt);
			break;
		}
		mutex_unlock(&state->lock);
		return 0;
	case V4L2_SEL_TGT_CROP_DEFAULT:
	case V4L2_SEL_TGT_CROP_BOUNDS:
	case V4L2_SEL_TGT_NATIVE_SIZE:
		sel->r.top = 0;
		sel->r.left = 0;
		sel->r.width = S5K3L6XX_CIS_WIDTH;
		sel->r.height = S5K3L6XX_CIS_HEIGHT;
		return 0;
	default:
		return -EINVAL;
	}
}

static const struct v4l2_subdev_pad_ops s5k3l6xx_pad_ops = {
	.enum_mbus_code		= s5k3l6xx_enum_mbus_code,
	.enum_frame_size	= s5k3l6xx_enum_frame_size,
//	.enum_frame_interval	= s5k5baf_enum_frame_interval,
	// doesn't seem to be used... ioctl(3, VIDIOC_S_FMT, ...)
	// instead seems to call enum_fmt, which does enum_mbus_code here.
	.get_fmt		= s5k3l6xx_get_fmt,
	.set_fmt		= s5k3l6xx_set_fmt,
	.get_selection		= s5k3l6xx_get_selection,
	// TODO: add set_selection
};

static const struct v4l2_subdev_video_ops s5k3l6xx_video_ops = {
	//.g_frame_interval	= s5k5baf_g_frame_interval,
	//.s_frame_interval	= s5k5baf_s_frame_interval,
	.s_stream		= s5k3l6xx_s_stream,
};

/*
 * V4L2 subdev controls
 */

static int s5k3l6xx_set_test_color(struct s5k3l6xx *state, struct v4l2_ctrl *ctrl, u16 color_channel) {
	int ret = s5k3l6xx_i2c_write2(state, color_channel, (u16)ctrl->val & 0x3ff);
	if (ret < 0)
		return ret;
	if (state->apply_test_solid)
		ret = s5k3l6xx_hw_set_test_pattern(state, S5K3L6XX_TEST_PATTERN_SOLID_COLOR);
	return ret;
}

static int s5k3l6xx_s_ctrl(struct v4l2_ctrl *ctrl)
{
	struct v4l2_subdev *sd = ctrl_to_sd(ctrl);
	struct s5k3l6xx *state = to_s5k3l6xx(sd);
	struct i2c_client *c = v4l2_get_subdevdata(&state->sd);
	int in_use;
	int ret = 0;

	v4l2_dbg(1, debug, sd, "ctrl: %s, value: %d\n", ctrl->name, ctrl->val);

	mutex_lock(&state->lock);

	// Don't do anything when powered off.
	// It will get called again when powering up.
	if (state->power == 0)
		goto unlock;
	/* v4l2_ctrl_handler_setup() function may not be used in the device’s runtime PM
	 * runtime_resume callback, as it has no way to figure out the power state of the device.
	 * https://www.kernel.org/doc/html/latest/driver-api/media/camera-sensor.html#control-framework
	 * Okay, so what's the right way to do it? So far relying on state->power.
	 */

	in_use = pm_runtime_get_if_in_use(&c->dev);

	switch (ctrl->id) {
	case V4L2_CID_ANALOGUE_GAIN:
		// Analog gain supported up to 0x200 (16). Gain = register / 32, so 0x20 gives gain 1.
		ret = s5k3l6xx_i2c_write2(state, S5K3L6XX_REG_ANALOG_GAIN, (u16)ctrl->val & 0x3ff);
		break;
	case V4L2_CID_DIGITAL_GAIN:
		ret = s5k3l6xx_i2c_write2(state, S5K3L6XX_REG_DIGITAL_GAIN, (u16)ctrl->val & 0xfff);
		break;
	case V4L2_CID_EXPOSURE:
		ret = s5k3l6xx_i2c_write2(state, S5K3L6XX_REG_COARSE_INTEGRATION_TIME, (u16)ctrl->val);
		break;
	case V4L2_CID_TEST_PATTERN:
		state->apply_test_solid = (ctrl->val == S5K3L6XX_TEST_PATTERN_SOLID_COLOR);
		v4l2_dbg(3, debug, sd, "Setting pattern %d", ctrl->val);
		ret = s5k3l6xx_hw_set_test_pattern(state, ctrl->val);
		break;
	case V4L2_CID_TEST_PATTERN_RED:
		ret = s5k3l6xx_set_test_color(state, ctrl, S5K3L6XX_REG_TEST_DATA_RED);
		break;
	case V4L2_CID_TEST_PATTERN_GREENR:
		ret = s5k3l6xx_set_test_color(state, ctrl, S5K3L6XX_REG_TEST_DATA_GREENR);
		break;
	case V4L2_CID_TEST_PATTERN_BLUE:
		ret = s5k3l6xx_set_test_color(state, ctrl, S5K3L6XX_REG_TEST_DATA_BLUE);
		break;
	case V4L2_CID_TEST_PATTERN_GREENB:
		ret = s5k3l6xx_set_test_color(state, ctrl, S5K3L6XX_REG_TEST_DATA_GREENB);
		break;
	}

	if (in_use) { // came from other context than resume, need to manage PM
		pm_runtime_put(&c->dev);
	}
unlock:
	mutex_unlock(&state->lock);
	return ret;
}

static const struct v4l2_ctrl_ops s5k3l6xx_ctrl_ops = {
	.s_ctrl	= s5k3l6xx_s_ctrl,
};

static const char * const s5k3l6xx_test_pattern_menu[] = {
	"Disabled",
	"Solid", // Color selectable
	"Bars", // 8 bars 100% saturation: black, blue, red, magents, green, cyan, yellow, white
	"Fade", // Bars fading towards 50% at the bottom. 512px high. Subdivided into left smooth and right quantized halves.
	"PN9", // pseudo-random noise
	"White",
	"LFSR32"
	"Address",
};


static int s5k3l6xx_initialize_ctrls(struct s5k3l6xx *state)
{
	const struct v4l2_ctrl_ops *ops = &s5k3l6xx_ctrl_ops;
	struct s5k3l6xx_ctrls *ctrls = &state->ctrls;
	struct v4l2_ctrl_handler *hdl = &ctrls->handler;
	struct i2c_client *c = v4l2_get_subdevdata(&state->sd);
	struct v4l2_fwnode_device_properties props;
	int ret;

	ret = v4l2_ctrl_handler_init(hdl, 16);
	if (ret < 0) {
		v4l2_err(&state->sd, "cannot init ctrl handler (%d)\n", ret);
		return ret;
	}

	// FIXME: dummy values. They should be tied to frame properties.
	// Some derived from frame, some influencing frame.

	// Exposure time (min: 2; max: frame length lines - 2; default: reset value)
	ctrls->exposure = v4l2_ctrl_new_std(hdl, ops, V4L2_CID_EXPOSURE,
					    2, 3118, 1, 0x03de);

	ctrls->pixel_rate = v4l2_ctrl_new_std(hdl, ops, V4L2_CID_PIXEL_RATE,
					      0,
					      s5k3l6xx_link_freqs_menu[0],
					      s5k3l6xx_link_freqs_menu[ARRAY_SIZE(s5k3l6xx_link_freqs_menu) - 1],
					      s5k3l6xx_link_freqs_menu[0]);
	ctrls->vblank = v4l2_ctrl_new_std(hdl, ops, V4L2_CID_VBLANK,
					  1, 1, 1, 1);
	ctrls->hblank = v4l2_ctrl_new_std(hdl, ops, V4L2_CID_HBLANK,
					  1, 1, 1, 1);
	if (ctrls->pixel_rate)
		ctrls->pixel_rate->flags |= V4L2_CTRL_FLAG_READ_ONLY;
	if (ctrls->vblank)
		ctrls->vblank->flags |= V4L2_CTRL_FLAG_READ_ONLY;
	if (ctrls->hblank)
		ctrls->hblank->flags |= V4L2_CTRL_FLAG_READ_ONLY;

	// Total gain: 32 <=> 1x
	ctrls->analog_gain = v4l2_ctrl_new_std(hdl, ops, V4L2_CID_ANALOGUE_GAIN,
					0x20, 0x200, 1, 0x20);

	// Digital gain range: 1.0x - 3.0x
	ctrls->digital_gain = v4l2_ctrl_new_std(hdl, ops, V4L2_CID_DIGITAL_GAIN,
					0x100, 0x300, 1, 0x100);

	v4l2_ctrl_new_std_menu_items(hdl, ops, V4L2_CID_TEST_PATTERN,
				     ARRAY_SIZE(s5k3l6xx_test_pattern_menu) - 1,
				     0, 0, s5k3l6xx_test_pattern_menu);

	v4l2_ctrl_new_std(hdl, ops, V4L2_CID_TEST_PATTERN_RED, 0, 1023, 1, 512);
	v4l2_ctrl_new_std(hdl, ops, V4L2_CID_TEST_PATTERN_GREENR, 0, 1023, 1, 512);
	v4l2_ctrl_new_std(hdl, ops, V4L2_CID_TEST_PATTERN_BLUE, 0, 1023, 1, 512);
	v4l2_ctrl_new_std(hdl, ops, V4L2_CID_TEST_PATTERN_GREENB, 0, 1023, 1, 512);


	ctrls->link_freq = v4l2_ctrl_new_int_menu(hdl, ops, V4L2_CID_LINK_FREQ,
						  ARRAY_SIZE(s5k3l6xx_link_freqs_menu) - 1,
						  0, s5k3l6xx_link_freqs_menu);

	ctrls->link_freq->flags |= V4L2_CTRL_FLAG_READ_ONLY;

	if (hdl->error) {
		v4l2_err(&state->sd, "error creating controls (%d)\n",
			 hdl->error);
		ret = hdl->error;
		v4l2_ctrl_handler_free(hdl);
		return ret;
	}

	ret = v4l2_fwnode_device_parse(&c->dev, &props);
	if (ret)
		return ret;

	ret = v4l2_ctrl_new_fwnode_properties(hdl, ops, &props);
	if (ret)
		return ret;

	state->sd.ctrl_handler = hdl;
	return 0;
}

/*
 * V4L2 subdev internal operations
 */
static const struct v4l2_subdev_core_ops s5k3l6xx_core_ops = {
	.log_status = v4l2_ctrl_subdev_log_status,
};

static const struct v4l2_subdev_ops s5k3l6xx_subdev_ops = {
	.core = &s5k3l6xx_core_ops,
	.pad = &s5k3l6xx_pad_ops,
	.video = &s5k3l6xx_video_ops,
};

static const struct v4l2_subdev_internal_ops s5k3l6xx_subdev_internal_ops = {
    .init_state = s5k3l6xx_init_state,
};

static int __maybe_unused s5k3l6xx_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct s5k3l6xx *state = to_s5k3l6xx(sd);

	dev_dbg(dev, "%s\n", __func__);

	if (state->streaming)
		s5k3l6xx_hw_set_stream(state, 0);

	return s5k3l6xx_set_power(sd, FALSE);
}

static int __maybe_unused s5k3l6xx_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct s5k3l6xx *state = to_s5k3l6xx(sd);
	int ret;

	dev_dbg(dev, "%s\n", __func__);

	ret = s5k3l6xx_set_power(sd, TRUE);

	if (ret == 0 && state->streaming) {
		ret = s5k3l6xx_hw_set_config(state);
		if (ret < 0) {
			state->streaming = 0;
			return ret;
		}

		ret = s5k3l6xx_hw_set_stream(state, 1);
		if (ret)
			state->streaming = 0;
	}

	return ret;
}

static int s5k3l6xx_parse_device_node(struct s5k3l6xx *state, struct device *dev)
{
	struct device_node *node = dev->of_node;
	struct device_node *node_ep;
	struct v4l2_fwnode_endpoint ep = { .bus_type = 0 };
	int ret;

	if (!node) {
		dev_err(dev, "no device-tree node provided\n");
		return -EINVAL;
	}

	ret = of_property_read_u32(node, "clock-frequency",
				   &state->mclk_frequency);
	if (ret < 0) {
		state->mclk_frequency = S5K3L6XX_DEFAULT_MCLK_FREQ;
		dev_warn(dev, "using default %u Hz clock frequency\n",
			 state->mclk_frequency);
	}

	state->rst_gpio = devm_gpiod_get(dev, "rstn", GPIOD_OUT_LOW);
	if (IS_ERR(state->rst_gpio)) {
		dev_err(dev, "failed to get rstn gpio: %pe\n", state->rst_gpio);
		return PTR_ERR(state->rst_gpio);
	}

	node_ep = of_graph_get_next_endpoint(node, NULL);
	if (!node_ep) {
		dev_err(dev, "no endpoint defined at node %pOF\n", node);
		return -EINVAL;
	}

	ret = v4l2_fwnode_endpoint_parse(of_fwnode_handle(node_ep), &ep);
	of_node_put(node_ep);
	if (ret) {
		dev_err(dev, "fwnode endpoint parse failed\n");
		return ret;
	}

	state->bus_type = ep.bus_type;

	switch (state->bus_type) {
	case V4L2_MBUS_CSI2_DPHY:
		state->nlanes = (unsigned char)ep.bus.mipi_csi2.num_data_lanes;
		break;
	default:
		dev_err(dev, "unsupported bus %d in endpoint defined at node %pOF\n",
			state->bus_type, (void*)node);
		return -EINVAL;
	}

	return 0;
}

static int s5k3l6xx_configure_subdevs(struct s5k3l6xx *state,
				     struct i2c_client *c)
{
	struct v4l2_subdev *sd;
	int ret;

	/*sd = &state->cis_sd;
	v4l2_subdev_init(sd, &s5k5baf_cis_subdev_ops);
	sd->owner = THIS_MODULE;
	v4l2_set_subdevdata(sd, state);
	snprintf(sd->name, sizeof(sd->name), "S5K5BAF-CIS %d-%04x",
		 i2c_adapter_id(c->adapter), c->addr);

	sd->internal_ops = &s5k5baf_cis_subdev_internal_ops;
	sd->flags |= V4L2_SUBDEV_FL_HAS_DEVNODE;

	state->cis_pad.flags = MEDIA_PAD_FL_SOURCE;
	sd->entity.function = MEDIA_ENT_F_CAM_SENSOR;
	ret = media_entity_pads_init(&sd->entity, NUM_CIS_PADS, &state->cis_pad);
	if (ret < 0)
		goto err;*/
	sd = &state->sd;
	v4l2_i2c_subdev_init(sd, c, &s5k3l6xx_subdev_ops);
	v4l2_info(sd, "probe i2c %px", (void*)c);

	sd->internal_ops = &s5k3l6xx_subdev_internal_ops;
	sd->flags |= V4L2_SUBDEV_FL_HAS_DEVNODE;

	state->cis_pad.flags = MEDIA_PAD_FL_SOURCE;
	sd->entity.function = MEDIA_ENT_F_CAM_SENSOR;
	ret = media_entity_pads_init(&sd->entity, NUM_CIS_PADS, &state->cis_pad);

	if (!ret)
		return 0;

	//media_entity_cleanup(&state->cis_sd.entity);
//err:
	dev_err(&c->dev, "cannot init media entity %s\n", sd->name);
	return ret;
}

static int debug_add(void *data, u64 value)
{
	struct s5k3l6xx *state = data;
	struct regstable_entry entry = {
		.address = state->debug_address,
		.value = (u8)value,
	};
	struct i2c_client *c = v4l2_get_subdevdata(&state->sd);
	v4l2_dbg(1, debug, c, "debug add override 0x%04x 0x%02x\n", entry.address, entry.value);
	/* Not sure which error flag to set here.
	 * EOF is not available. E2BIG seems to be used too. */
	if (state->debug_regs.entry_count >= REGSTABLE_SIZE)
		return -EFBIG;
	if (value != entry.value)
		return -EINVAL;
	state->debug_regs.entries[state->debug_regs.entry_count] = entry;
	state->debug_regs.entry_count++;
	return 0;
}

static int debug_clear(void *data, u64 value)
{
	struct s5k3l6xx *state = data;
	struct i2c_client *c = v4l2_get_subdevdata(&state->sd);
	(void)value;
	v4l2_dbg(1, debug, c, "debug clear\n");
	state->debug_regs.entry_count = 0;
	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(debug_add_ops, NULL, debug_add, "%llx\n");
DEFINE_SIMPLE_ATTRIBUTE(debug_clear_ops, NULL, debug_clear, "%llu\n");

static int s5k3l6xx_probe(struct i2c_client *c)
{
	struct s5k3l6xx *state;
	int ret;
	unsigned i;
	unsigned long mclk_freq;
	struct s5k3l6xx_read_result test;
	struct dentry *d;

	state = devm_kzalloc(&c->dev, sizeof(*state), GFP_KERNEL);
	if (!state)
		return -ENOMEM;

	mutex_init(&state->lock);

	ret = s5k3l6xx_parse_device_node(state, &c->dev);
	if (ret < 0) {
		pr_err("s5k3l6xx_parse_device_node: failed");
		return ret;
	}

	ret = s5k3l6xx_configure_subdevs(state, c);
	if (ret < 0) {
		pr_err("s5k3l6xx_configure_subdevs: failed");
		return ret;
	}

	for (i = 0; i < S5K3L6XX_NUM_SUPPLIES; i++)
		state->supplies[i].supply = s5k3l6xx_supply_names[i];

	ret = devm_regulator_bulk_get(&c->dev, S5K3L6XX_NUM_SUPPLIES,
				      state->supplies);
	if (ret < 0) {
		pr_err("s5k3l6xx configure regulators: failed");
		goto err_me;
	}

	state->clock = devm_clk_get(state->sd.dev, S5K3L6XX_CLK_NAME);
	if (IS_ERR(state->clock)) {
		pr_err("get clk failed: failed");
		ret = -EPROBE_DEFER;
		goto err_me;
	}

	mclk_freq = clk_get_rate(state->clock);
	/* The sensor supports between 6MHz and 32MHz,
	 * but I can't properly test that.
	 */
	if ((mclk_freq < 19200000) || (mclk_freq > 25000000)) {
		dev_err(&c->dev,
			"External clock frequency out of range: %lu.\n",
			mclk_freq);
		goto err_me;
	}

	ret = s5k3l6xx_power_on(state);
	if (ret < 0) {
		pr_err("s5k3l6xx_power_on: failed");
		goto err_me;
	}
	state->power = 1;

	test = s5k3l6xx_read(state, S5K3L6XX_REG_MODEL_ID_L);
	if (test.retcode < 0) {
		ret = test.retcode;
		goto err_power;
	} else if (test.value != S5K3L6XX_MODEL_ID_L) {
		dev_err(&c->dev, "model mismatch: 0x%X != 0x30\n", test.value);
		ret = -EINVAL;
		goto err_power;
	} else
		dev_info(&c->dev, "model low: 0x%X\n", test.value);

	test = s5k3l6xx_read(state, S5K3L6XX_REG_MODEL_ID_H);
	if (test.retcode < 0) {
		ret = test.retcode;
		goto err_power;
	} else if (test.value != S5K3L6XX_MODEL_ID_H) {
		dev_err(&c->dev, "model mismatch: 0x%X != 0xC6\n", test.value);
		ret = -EINVAL;
		goto err_power;
	} else
		dev_info(&c->dev, "model high: 0x%X\n", test.value);

	test = s5k3l6xx_read(state, S5K3L6XX_REG_REVISION_NUMBER);
	if (test.retcode < 0) {
		ret = test.retcode;
		goto err_power;
	} else if (test.value != S5K3L6XX_REVISION_NUMBER) {
		dev_err(&c->dev, "revision mismatch: 0x%X != 0xB0\n", test.value);
		ret = -EINVAL;
		goto err_power;
	} else
		dev_info(&c->dev, "revision number: 0x%X\n", test.value);

	ret = s5k3l6xx_initialize_ctrls(state);
	if (ret < 0)
		goto err_power;

	ret = v4l2_async_register_subdev_sensor(&state->sd);
	if (ret < 0)
		goto err_ctrl;

	pm_runtime_set_active(&c->dev);
	pm_runtime_enable(&c->dev);
	// I don't really know why this idle is needed
	pm_runtime_idle(&c->dev);

	// Default frame.
	state->frame_fmt = &s5k3l6xx_frames[0];

	d = debugfs_create_dir("s5k3l6", NULL);
	// When set to 1, then any frame size is accepted in frame set.
	// In addition, no sensor registers will be set, except stream on and bits per pixel.
	state->debug_frame = 0;
	debugfs_create_u8("debug_frame", S_IRUSR | S_IWUSR, d, &state->debug_frame);

	/* Can't be bothered to expose the entire register set in one file, so here it is.
	 * 1. Write u16 as hex to `address`.
	 * 2. Write u8 as hex to `add_value` and the *address = value will be saved.
	 * 3. Repeat if needed.
	 * 4. Reset the device (a suspend cycle will do)
	 * 5. Take pictures.
	 * 6. Write `1` to `clear` to erase all the added values.
	 */
	debugfs_create_x16("address", S_IRUSR | S_IWUSR, d, &state->debug_address);
	debugfs_create_file("add_value", S_IWUSR, d, (void*)state, &debug_add_ops);
	debugfs_create_file("clear", S_IWUSR, d, (void*)state, &debug_clear_ops);

	return 0;

err_ctrl:
	v4l2_ctrl_handler_free(state->sd.ctrl_handler);
err_power:
	s5k3l6xx_power_off(state);
err_me:
	media_entity_cleanup(&state->sd.entity);
	media_entity_cleanup(&state->cis_sd.entity);
	return ret;
}

static void s5k3l6xx_remove(struct i2c_client *c)
{
	struct v4l2_subdev *sd = i2c_get_clientdata(c);
	struct s5k3l6xx *state = to_s5k3l6xx(sd);

	v4l2_async_unregister_subdev(sd);
	v4l2_ctrl_handler_free(sd->ctrl_handler);
	media_entity_cleanup(&sd->entity);

	sd = &state->cis_sd;
	v4l2_device_unregister_subdev(sd);
	media_entity_cleanup(&sd->entity);

	pm_runtime_disable(&c->dev);
	pm_runtime_set_suspended(&c->dev);
	pm_runtime_put_noidle(&c->dev);
}

static const struct dev_pm_ops s5k3l6xx_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(pm_runtime_force_suspend,
				pm_runtime_force_resume)
	SET_RUNTIME_PM_OPS(s5k3l6xx_suspend, s5k3l6xx_resume, NULL)
};

static const struct i2c_device_id s5k3l6xx_id[] = {
	{ S5K3L6XX_DRIVER_NAME, 0 },
	{ },
};
MODULE_DEVICE_TABLE(i2c, s5k3l6xx_id);

static const struct of_device_id s5k3l6xx_of_match[] = {
	{ .compatible = "samsung,s5k3l6xx" },
	{ }
};
MODULE_DEVICE_TABLE(of, s5k3l6xx_of_match);

static struct i2c_driver s5k3l6xx_i2c_driver = {
	.driver = {
		.pm = &s5k3l6xx_pm_ops,
		.of_match_table = s5k3l6xx_of_match,
		.name = S5K3L6XX_DRIVER_NAME
	},
	.probe		= s5k3l6xx_probe,
	.remove		= s5k3l6xx_remove,
	.id_table	= s5k3l6xx_id,
};

module_i2c_driver(s5k3l6xx_i2c_driver);

MODULE_DESCRIPTION("Samsung S5K3L6XX 13M camera driver");
MODULE_AUTHOR("Martin Kepplinger <martin.kepplinger@puri.sm>");
MODULE_AUTHOR("Dorota Czaplejewicz <dorota.czaplejewicz@puri.sm>");
MODULE_LICENSE("GPL v2");
