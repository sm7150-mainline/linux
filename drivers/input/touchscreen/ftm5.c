// SPDX-License-Identifier: GPL-2.0-only
/*
 * STMicroelectronics FTM5 (FingerTip) touchscreen driver.
 *
 * Mainline driver for the FTM5 controller found on the Google Pixel 4a
 * (sunfish). Handles power/reset sequencing, chip detection, and multitouch
 * reporting via the controller's event FIFO.
 *
 * Touch scanning is coupled to the display via the DRM panel follower API:
 * the controller only senses (and only raises interrupts) while the panel is
 * prepared, so a blanked screen also stops the touch controller. This covers
 * both DPMS blanking and system suspend.
 *
 * Protocol reference: downstream kernel/msm-modules/fts_touch (ftm5),
 * android-msm-sunfish-4.14. The controller keeps its firmware resident, so no
 * firmware download is performed here.
 */
#include <linux/bitops.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/gpio/consumer.h>
#include <linux/i2c.h>
#include <linux/input.h>
#include <linux/input/mt.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/property.h>
#include <linux/regulator/consumer.h>
#include <linux/string.h>

#include <drm/drm_panel.h>

/* Op-codes (I2C interface). */
#define FTM5_CMD_HW_REG		0xFA	/* HW-register read/write prefix */
#define FTM5_CMD_SCAN_MODE	0xA0	/* set scan mode */
#define FTM5_CMD_FIFO_READ	0x86	/* read events from the FIFO */

#define FTM5_SCAN_MODE_ACTIVE	0x00
#define FTM5_SCAN_BANKS_ALL	0xFF	/* enable all sensing banks */
#define FTM5_SCAN_BANKS_NONE	0x00	/* disable sensing */

/* 32-bit HW-register address of the chip id. */
#define FTM5_ADDR_CHIP_ID	0x20000000U
/* DCHIP_ID_0 (LSB) = 0x36, DCHIP_ID_1 (MSB) = 0x48 -> 0x4836 (sunfish). */
#define FTM5_CHIP_ID		0x4836

#define FTM5_EVENT_SIZE		8
#define FTM5_FIFO_DEPTH		32
#define FTM5_MAX_TOUCHES	10

/* Event ids live in the high nibble of event[0]. */
#define FTM5_EV_ENTER		0x1	/* touch enter  (0x13) */
#define FTM5_EV_MOTION		0x2	/* touch motion (0x23) */
#define FTM5_EV_LEAVE		0x3	/* touch leave  (0x33) */
#define FTM5_EV_ERROR		0xF	/* error event  (0xF3) */

/* Full event[0] value for a (re)boot notification from the controller. */
#define FTM5_EVT_CONTROLLER_READY	0x03

/* Error types (event[1]) that warrant re-initialising the controller. */
#define FTM5_ERR_HARD_FAULT	0x02
#define FTM5_ERR_WATCHDOG	0x06
#define FTM5_ERR_ESD		0xF0

#define FTM5_EVENTS_REMAINING_MASK	0x1F

#define FTM5_DEFAULT_X		1080
#define FTM5_DEFAULT_Y		2340
#define FTM5_MAX_AREA		255

struct ftm5 {
	struct i2c_client *client;
	struct input_dev *input;
	struct regulator *vdd;
	struct gpio_desc *reset_gpio;
	struct gpio_desc *switch_gpio;
	struct drm_panel_follower panel_follower;
	unsigned int x_max;
	unsigned int y_max;
	unsigned long touchbits;
};

/*
 * FTM5 HW-register read: write { op-code, addr[MSB..LSB] } then read @len bytes
 * in a single repeated-start transaction. No dummy byte over I2C.
 */
static int ftm5_read_hw_reg(struct ftm5 *ts, u32 addr, u8 *buf, size_t len)
{
	u8 cmd[5] = {
		FTM5_CMD_HW_REG,
		(addr >> 24) & 0xff,
		(addr >> 16) & 0xff,
		(addr >> 8) & 0xff,
		addr & 0xff,
	};
	struct i2c_msg msgs[2] = {
		{
			.addr = ts->client->addr,
			.flags = 0,
			.len = sizeof(cmd),
			.buf = cmd,
		},
		{
			.addr = ts->client->addr,
			.flags = I2C_M_RD,
			.len = len,
			.buf = buf,
		},
	};
	int ret;

	ret = i2c_transfer(ts->client->adapter, msgs, 2);
	if (ret != 2)
		return ret < 0 ? ret : -EIO;
	return 0;
}

static int ftm5_write(struct ftm5 *ts, const u8 *buf, size_t len)
{
	int ret = i2c_master_send(ts->client, buf, len);

	if (ret < 0)
		return ret;
	return ret != len ? -EIO : 0;
}

/*
 * Set the sensing scan mode. Mirrors downstream setScanMode(): a HW-register
 * write to wake the sensing block, then the scan-mode command. @banks selects
 * which sensing banks are enabled (0xFF = all, 0x00 = sensing off).
 */
static int ftm5_set_scan(struct ftm5 *ts, u8 banks)
{
	static const u8 wake[] = { FTM5_CMD_HW_REG, 0x20, 0x00, 0x00, 0x00,
				   0x00, 0x00 };
	u8 scan[] = { FTM5_CMD_SCAN_MODE, FTM5_SCAN_MODE_ACTIVE, banks };
	int ret;

	ret = ftm5_write(ts, wake, sizeof(wake));
	if (ret)
		return ret;
	return ftm5_write(ts, scan, sizeof(scan));
}

static void ftm5_report_event(struct ftm5 *ts, const u8 *e)
{
	int id = (e[1] & 0xF0) >> 4;

	if (id >= FTM5_MAX_TOUCHES)
		return;

	switch (e[0] >> 4) {
	case FTM5_EV_ENTER:
	case FTM5_EV_MOTION: {
		int x = ((e[3] & 0x0F) << 8) | e[2];
		int y = (e[4] << 4) | ((e[3] & 0xF0) >> 4);
		int z = e[5] ? e[5] : 1;
		int major = ((e[0] & 0x0C) << 2) | ((e[6] & 0xF0) >> 4);
		int minor = ((e[7] & 0xC0) >> 2) | (e[6] & 0x0F);

		x = min_t(int, x, ts->x_max);
		y = min_t(int, y, ts->y_max);

		input_mt_slot(ts->input, id);
		input_mt_report_slot_state(ts->input, MT_TOOL_FINGER, true);
		input_report_abs(ts->input, ABS_MT_POSITION_X, x);
		input_report_abs(ts->input, ABS_MT_POSITION_Y, y);
		input_report_abs(ts->input, ABS_MT_TOUCH_MAJOR, major);
		input_report_abs(ts->input, ABS_MT_TOUCH_MINOR, minor);
		input_report_abs(ts->input, ABS_MT_PRESSURE, z);
		__set_bit(id, &ts->touchbits);
		break;
	}
	case FTM5_EV_LEAVE:
		input_mt_slot(ts->input, id);
		input_mt_report_slot_state(ts->input, MT_TOOL_FINGER, false);
		__clear_bit(id, &ts->touchbits);
		break;
	default:
		/* controller-ready / status / error events: nothing to do */
		break;
	}
}

static void ftm5_release_all(struct ftm5 *ts)
{
	int i;

	for (i = 0; i < FTM5_MAX_TOUCHES; i++) {
		input_mt_slot(ts->input, i);
		input_mt_report_slot_state(ts->input, MT_TOOL_FINGER, false);
	}
	ts->touchbits = 0;
	input_report_key(ts->input, BTN_TOUCH, 0);
	input_sync(ts->input);
}

/*
 * Recover from a controller fault. Drop all touches, optionally hard-reset the
 * chip, and re-arm scanning. Runs in the (sleepable) IRQ thread.
 */
static void ftm5_recover(struct ftm5 *ts, bool hw_reset)
{
	dev_warn(&ts->client->dev, "controller %s, re-initialising\n",
		 hw_reset ? "fault" : "reset");

	ftm5_release_all(ts);

	if (hw_reset && ts->reset_gpio) {
		gpiod_set_value_cansleep(ts->reset_gpio, 1);
		usleep_range(10000, 12000);
		gpiod_set_value_cansleep(ts->reset_gpio, 0);
		msleep(200);
	}

	ftm5_set_scan(ts, FTM5_SCAN_BANKS_ALL);
}

static irqreturn_t ftm5_irq(int irq, void *dev_id)
{
	struct ftm5 *ts = dev_id;
	bool need_reset = false, need_reinit = false;
	u8 data[FTM5_EVENT_SIZE * FTM5_FIFO_DEPTH];
	u8 cmd = FTM5_CMD_FIFO_READ;
	struct i2c_msg msgs[2] = {
		{
			.addr = ts->client->addr,
			.flags = 0,
			.len = 1,
			.buf = &cmd,
		},
		{
			.addr = ts->client->addr,
			.flags = I2C_M_RD,
			.len = FTM5_EVENT_SIZE,
			.buf = data,
		},
	};
	int remaining, ret, i;

	/* Read the first event; its low 5 bits of byte 7 hold how many more. */
	ret = i2c_transfer(ts->client->adapter, msgs, 2);
	if (ret != 2)
		return IRQ_HANDLED;

	remaining = data[7] & FTM5_EVENTS_REMAINING_MASK;
	remaining = min(remaining, FTM5_FIFO_DEPTH - 1);

	if (remaining > 0) {
		msgs[1].len = FTM5_EVENT_SIZE * remaining;
		msgs[1].buf = data + FTM5_EVENT_SIZE;
		ret = i2c_transfer(ts->client->adapter, msgs, 2);
		if (ret != 2)
			return IRQ_HANDLED;
	}

	for (i = 0; i <= remaining; i++) {
		const u8 *e = &data[i * FTM5_EVENT_SIZE];

		if (e[0] == 0x00)	/* EVT_ID_NOEVENT */
			break;

		if (e[0] == FTM5_EVT_CONTROLLER_READY) {
			need_reinit = true;
		} else if ((e[0] >> 4) == FTM5_EV_ERROR) {
			if (e[1] == FTM5_ERR_ESD || e[1] == FTM5_ERR_HARD_FAULT ||
			    e[1] == FTM5_ERR_WATCHDOG)
				need_reset = true;
		} else {
			ftm5_report_event(ts, e);
		}
	}

	input_report_key(ts->input, BTN_TOUCH, ts->touchbits != 0);
	input_sync(ts->input);

	/* A hard fault needs a reset; a spontaneous reboot just needs re-arming. */
	if (need_reset || need_reinit)
		ftm5_recover(ts, need_reset);

	return IRQ_HANDLED;
}

/* Enable sensing and start accepting interrupts. */
static int ftm5_start(struct ftm5 *ts)
{
	int ret = ftm5_set_scan(ts, FTM5_SCAN_BANKS_ALL);

	if (ret)
		return ret;
	enable_irq(ts->client->irq);
	return 0;
}

/* Stop interrupts, turn sensing off and release any held touches. */
static void ftm5_stop(struct ftm5 *ts)
{
	disable_irq(ts->client->irq);
	ftm5_set_scan(ts, FTM5_SCAN_BANKS_NONE);
	ftm5_release_all(ts);
}

static int ftm5_power_on(struct ftm5 *ts)
{
	int ret;

	/* Route the touch controller's I2C to the AP (not the SLPI/ADSP). */
	if (ts->switch_gpio)
		gpiod_set_value_cansleep(ts->switch_gpio, 0);

	ret = regulator_enable(ts->vdd);
	if (ret)
		return ret;
	/* Let the analog rail settle before releasing reset. */
	msleep(20);

	if (ts->reset_gpio) {
		/* Assert (active-low) then release the reset line. */
		gpiod_set_value_cansleep(ts->reset_gpio, 1);
		usleep_range(10000, 12000);
		gpiod_set_value_cansleep(ts->reset_gpio, 0);
	}

	/* Firmware boot after reset release (emits a controller-ready event). */
	msleep(200);
	return 0;
}

static void ftm5_power_off(void *data)
{
	struct ftm5 *ts = data;

	if (ts->reset_gpio)
		gpiod_set_value_cansleep(ts->reset_gpio, 1);
	regulator_disable(ts->vdd);
}

static int ftm5_panel_prepared(struct drm_panel_follower *follower)
{
	struct ftm5 *ts = container_of(follower, struct ftm5, panel_follower);

	return ftm5_start(ts);
}

static int ftm5_panel_unpreparing(struct drm_panel_follower *follower)
{
	struct ftm5 *ts = container_of(follower, struct ftm5, panel_follower);

	ftm5_stop(ts);
	return 0;
}

static const struct drm_panel_follower_funcs ftm5_panel_follower_funcs = {
	.panel_prepared = ftm5_panel_prepared,
	.panel_unpreparing = ftm5_panel_unpreparing,
};

static int ftm5_probe(struct i2c_client *client)
{
	struct device *dev = &client->dev;
	struct ftm5 *ts;
	u8 id[2];
	u16 chip_id;
	int ret;

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C))
		return -ENODEV;

	if (client->irq <= 0)
		return dev_err_probe(dev, -EINVAL, "no IRQ\n");

	ts = devm_kzalloc(dev, sizeof(*ts), GFP_KERNEL);
	if (!ts)
		return -ENOMEM;
	ts->client = client;
	i2c_set_clientdata(client, ts);

	ts->x_max = FTM5_DEFAULT_X;
	ts->y_max = FTM5_DEFAULT_Y;
	device_property_read_u32(dev, "touchscreen-size-x", &ts->x_max);
	device_property_read_u32(dev, "touchscreen-size-y", &ts->y_max);

	ts->vdd = devm_regulator_get(dev, "vdd");
	if (IS_ERR(ts->vdd))
		return dev_err_probe(dev, PTR_ERR(ts->vdd), "no vdd supply\n");

	/* Active-low reset; start asserted. */
	ts->reset_gpio = devm_gpiod_get_optional(dev, "reset", GPIOD_OUT_HIGH);
	if (IS_ERR(ts->reset_gpio))
		return dev_err_probe(dev, PTR_ERR(ts->reset_gpio),
				     "failed to get reset gpio\n");

	/* AP/SLPI mux: drive low = AP. Optional. */
	ts->switch_gpio = devm_gpiod_get_optional(dev, "st,switch", GPIOD_OUT_LOW);
	if (IS_ERR(ts->switch_gpio))
		return dev_err_probe(dev, PTR_ERR(ts->switch_gpio),
				     "failed to get switch gpio\n");

	ret = ftm5_power_on(ts);
	if (ret)
		return dev_err_probe(dev, ret, "power-on failed\n");

	ret = devm_add_action_or_reset(dev, ftm5_power_off, ts);
	if (ret)
		return ret;

	ret = ftm5_read_hw_reg(ts, FTM5_ADDR_CHIP_ID, id, sizeof(id));
	if (ret)
		return dev_err_probe(dev, ret, "chip-id read failed\n");

	chip_id = (id[1] << 8) | id[0];
	if (chip_id == FTM5_CHIP_ID)
		dev_info(dev, "STMicroelectronics FTM5 detected, chip id 0x%04x\n",
			 chip_id);
	else
		dev_warn(dev, "unexpected chip id 0x%04x (raw %02x %02x); continuing\n",
			 chip_id, id[0], id[1]);

	ts->input = devm_input_allocate_device(dev);
	if (!ts->input)
		return -ENOMEM;

	ts->input->name = "STMicroelectronics FTM5";
	ts->input->id.bustype = BUS_I2C;

	input_set_capability(ts->input, EV_KEY, BTN_TOUCH);
	input_set_abs_params(ts->input, ABS_MT_POSITION_X, 0, ts->x_max, 0, 0);
	input_set_abs_params(ts->input, ABS_MT_POSITION_Y, 0, ts->y_max, 0, 0);
	input_set_abs_params(ts->input, ABS_MT_TOUCH_MAJOR, 0, FTM5_MAX_AREA, 0, 0);
	input_set_abs_params(ts->input, ABS_MT_TOUCH_MINOR, 0, FTM5_MAX_AREA, 0, 0);
	input_set_abs_params(ts->input, ABS_MT_PRESSURE, 0, FTM5_MAX_AREA, 0, 0);

	ret = input_mt_init_slots(ts->input, FTM5_MAX_TOUCHES, INPUT_MT_DIRECT);
	if (ret)
		return dev_err_probe(dev, ret, "failed to init MT slots\n");

	ret = input_register_device(ts->input);
	if (ret)
		return dev_err_probe(dev, ret, "failed to register input\n");

	/*
	 * Start with the IRQ disabled; sensing (and thus the IRQ) is enabled
	 * from ftm5_start(), driven either by the panel follower callback or
	 * directly below when no panel is wired up.
	 */
	ret = devm_request_threaded_irq(dev, client->irq, NULL, ftm5_irq,
					IRQF_ONESHOT | IRQF_NO_AUTOEN, "ftm5", ts);
	if (ret)
		return dev_err_probe(dev, ret, "failed to request irq\n");

	if (drm_is_panel_follower(dev)) {
		ts->panel_follower.funcs = &ftm5_panel_follower_funcs;
		/*
		 * Registers the follower and, if the panel is already prepared,
		 * calls ->panel_prepared() synchronously to start sensing.
		 */
		ret = devm_drm_panel_add_follower(dev, &ts->panel_follower);
		if (ret)
			return dev_err_probe(dev, ret,
					     "failed to add panel follower\n");
	} else {
		ret = ftm5_start(ts);
		if (ret)
			return dev_err_probe(dev, ret, "failed to start sensing\n");
	}

	return 0;
}

static const struct of_device_id ftm5_of_match[] = {
	{ .compatible = "st,fts" },
	{ }
};
MODULE_DEVICE_TABLE(of, ftm5_of_match);

static const struct i2c_device_id ftm5_id[] = {
	{ "ftm5" },
	{ }
};
MODULE_DEVICE_TABLE(i2c, ftm5_id);

static struct i2c_driver ftm5_driver = {
	.driver = {
		.name = "ftm5",
		.of_match_table = ftm5_of_match,
	},
	.probe = ftm5_probe,
	.id_table = ftm5_id,
};
module_i2c_driver(ftm5_driver);

MODULE_DESCRIPTION("STMicroelectronics FTM5 touchscreen driver");
MODULE_LICENSE("GPL");
