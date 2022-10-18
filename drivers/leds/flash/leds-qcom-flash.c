// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#include <linux/bitfield.h>
#include <linux/bits.h>
#include <linux/leds.h>
#include <linux/led-class-flash.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/property.h>
#include <linux/regmap.h>
#include <media/v4l2-flash-led-class.h>

/* registers definitions */
#define FLASH_TYPE_REG			0x04
#define FLASH_TYPE_VAL			0x18

#define FLASH_SUBTYPE_REG		0x05
#define FLASH_SUBTYPE_3CH_VAL		0x04
#define FLASH_SUBTYPE_4CH_VAL		0x07

#define FLASH_TIMER_EN_BIT		BIT(7)
#define FLASH_TIMER_VAL_MASK		GENMASK(6, 0)
#define FLASH_TIMER_STEP_MS		10

#define FLASH_STROBE_HW_SW_SEL_BIT	BIT(2)
#define SW_STROBE_VAL			0
#define HW_STROBE_VAL			1
#define FLASH_HW_STROBE_TRIGGER_SEL_BIT	BIT(1)
#define STROBE_LEVEL_TRIGGER_VAL	0
#define STROBE_EDGE_TRIGGER_VAL		1
#define FLASH_STROBE_POLARITY_BIT	BIT(0)
#define STROBE_ACTIVE_HIGH_VAL		1

#define FLASH_IRES_MASK_4CH		BIT(0)
#define FLASH_IRES_MASK_3CH		GENMASK(1, 0)
#define FLASH_IRES_12P5MA_VAL		0
#define FLASH_IRES_5MA_VAL_4CH		1
#define FLASH_IRES_5MA_VAL_3CH		3

/* constants */
#define FLASH_CURRENT_MAX_UA		1500000
#define TORCH_CURRENT_MAX_UA		500000
#define FLASH_TOTAL_CURRENT_MAX_UA	2000000
#define FLASH_CURRENT_DEFAULT_UA	1000000
#define TORCH_CURRENT_DEFAULT_UA	200000

#define TORCH_IRES_UA			5000
#define FLASH_IRES_UA			12500

#define FLASH_TIMEOUT_MAX_US		1280000
#define FLASH_TIMEOUT_STEP_US		10000

enum hw_type {
	QCOM_MVFLASH_3CH,
	QCOM_MVFLASH_4CH,
};

enum led_mode {
	FLASH_MODE,
	TORCH_MODE,
};

enum led_strobe {
	SW_STROBE,
	HW_STROBE,
};

enum {
	REG_STATUS1,
	REG_STATUS2,
	REG_STATUS3,
	REG_CHAN_TIMER,
	REG_ITARGET,
	REG_MODULE_EN,
	REG_IRESOLUTION,
	REG_CHAN_STROBE,
	REG_CHAN_EN,
	REG_MAX_COUNT,
};

struct reg_field mvflash_3ch_regs[REG_MAX_COUNT] = {
	REG_FIELD(0x08, 0, 7),			/* status1	*/
	REG_FIELD(0x09, 0, 7),                  /* status2	*/
	REG_FIELD(0x0a, 0, 7),                  /* status3	*/
	REG_FIELD_ID(0x40, 0, 7, 3, 1),         /* chan_timer	*/
	REG_FIELD_ID(0x43, 0, 6, 3, 1),         /* itarget	*/
	REG_FIELD(0x46, 7, 7),                  /* module_en	*/
	REG_FIELD(0x47, 0, 5),                  /* iresolution	*/
	REG_FIELD_ID(0x49, 0, 2, 3, 1),         /* chan_strobe	*/
	REG_FIELD(0x4c, 0, 2),                  /* chan_en	*/
};

struct reg_field mvflash_4ch_regs[REG_MAX_COUNT] = {
	REG_FIELD(0x06, 0, 7),			/* status1	*/
	REG_FIELD(0x07, 0, 6),			/* status2	*/
	REG_FIELD(0x09, 0, 7),			/* status3	*/
	REG_FIELD_ID(0x3e, 0, 7, 4, 1),		/* chan_timer	*/
	REG_FIELD_ID(0x42, 0, 6, 4, 1),		/* itarget	*/
	REG_FIELD(0x46, 7, 7),			/* module_en	*/
	REG_FIELD(0x49, 0, 3),			/* iresolution	*/
	REG_FIELD_ID(0x4a, 0, 6, 4, 1),		/* chan_strobe	*/
	REG_FIELD(0x4e, 0, 3),			/* chan_en	*/
};

struct qcom_flash_led {
	struct qcom_flash_chip		*chip;
	struct led_classdev_flash	flash;
	struct v4l2_flash		*v4l2_flash;
	u32				max_flash_current_ma;
	u32				max_torch_current_ma;
	u32				max_timeout_ms;
	u32				flash_current_ma;
	u32				flash_timeout_ms;
	u8				*chan_id;
	u8				chan_count;
	bool				enabled;
};

struct qcom_flash_chip {
	struct qcom_flash_led	*leds;
	struct regmap_field     *r_fields[REG_MAX_COUNT];
	struct device		*dev;
	struct mutex		lock;
	enum hw_type		hw_type;
	u8			leds_count;
	u8			max_channels;
	u8			chan_en_bits;
};

static int set_flash_module_en(struct qcom_flash_led *led, bool en)
{
	struct qcom_flash_chip *chip = led->chip;
	u8 led_mask = 0, val;
	int i, rc;

	for (i = 0; i < led->chan_count; i++)
		led_mask |= BIT(led->chan_id[i] - 1);

	mutex_lock(&chip->lock);
	if (en)
		chip->chan_en_bits |= led_mask;
	else
		chip->chan_en_bits &= ~led_mask;

	val = !!chip->chan_en_bits;
	rc = regmap_field_write(chip->r_fields[REG_MODULE_EN], val);
	if (rc < 0)
		dev_err(chip->dev, "write module_en failed, rc=%d\n", rc);
	mutex_unlock(&chip->lock);

	return rc;
}

static int set_flash_current(struct qcom_flash_led *led, u32 current_ma, enum led_mode mode)
{
	struct qcom_flash_chip *chip = led->chip;
	u32 itarg_ua = current_ma * 1000 / led->chan_count + 1;
	u32 ires_ua = (mode == FLASH_MODE) ? FLASH_IRES_UA : TORCH_IRES_UA;
	u8 val, shift, ires_mask = 0, ires_val = 0, chan_id;
	int i, rc;

	/*
	 * Split the current across the channels and set the
	 * IRESOLUTION and ITARGET registers accordingly.
	 */
	for (i = 0; i < led->chan_count; i++) {
		chan_id = led->chan_id[i];
		if (itarg_ua < ires_ua)
			val = 0;
		else
			val = itarg_ua / ires_ua - 1;

		rc = regmap_fields_write(chip->r_fields[REG_ITARGET], chan_id - 1, val);
		if (rc < 0)
			return rc;

		if (chip->hw_type == QCOM_MVFLASH_3CH) {
			shift = (chan_id - 1) * 2;
			ires_mask |= FLASH_IRES_MASK_3CH << shift;
			ires_val |= ((mode == FLASH_MODE) ?
				(FLASH_IRES_12P5MA_VAL << shift) :
				(FLASH_IRES_5MA_VAL_3CH << shift));
		} else if (chip->hw_type == QCOM_MVFLASH_4CH) {
			shift = chan_id - 1;
			ires_mask |= FLASH_IRES_MASK_4CH << shift;
			ires_val |= ((mode == FLASH_MODE) ?
				(FLASH_IRES_12P5MA_VAL << shift) :
				(FLASH_IRES_5MA_VAL_4CH << shift));
		}
	}

	return regmap_field_update_bits(chip->r_fields[REG_IRESOLUTION], ires_mask, ires_val);
}

static int set_flash_timeout(struct qcom_flash_led *led, u32 timeout_ms)
{
	struct qcom_flash_chip *chip = led->chip;
	u8 val, chan_id;
	int rc, i;

	/* set SAFETY_TIMER for all the channels connected to the same LED */
	timeout_ms = min_t(u32, timeout_ms, led->max_timeout_ms);
	for (i = 0; i < led->chan_count; i++) {
		chan_id = led->chan_id[i];
		val = timeout_ms / FLASH_TIMER_STEP_MS;
		val = clamp_t(u8, val, 0, FLASH_TIMER_VAL_MASK);
		if (timeout_ms)
			val |= FLASH_TIMER_EN_BIT;

		rc = regmap_fields_write(chip->r_fields[REG_CHAN_TIMER], chan_id - 1, val);
		if (rc < 0)
			return rc;
	}

	return 0;
}

static int set_flash_strobe(struct qcom_flash_led *led, enum led_strobe strobe, bool state)
{
	struct qcom_flash_chip *chip = led->chip;
	u8 mask, val, chan_id = 0, chan_mask = 0;
	int rc, i;

	/* Set SW strobe config for all channels connected to the LED */
	for (i = 0; i < led->chan_count; i++) {
		chan_id = led->chan_id[i];
		if (strobe == SW_STROBE)
			val = FIELD_PREP(FLASH_STROBE_HW_SW_SEL_BIT, SW_STROBE_VAL);
		else
			val = FIELD_PREP(FLASH_STROBE_HW_SW_SEL_BIT, HW_STROBE_VAL);

		val |= FIELD_PREP(FLASH_HW_STROBE_TRIGGER_SEL_BIT, STROBE_LEVEL_TRIGGER_VAL) |
			FIELD_PREP(FLASH_STROBE_POLARITY_BIT, STROBE_ACTIVE_HIGH_VAL);
		rc = regmap_fields_write(chip->r_fields[REG_CHAN_STROBE], chan_id - 1, val);
		if (rc < 0)
			return rc;

		chan_mask |= BIT(chan_id - 1);
	}

	/* enable/disable flash channels */
	mask = chan_mask;
	val = state ? mask : 0;
	rc = regmap_field_update_bits(chip->r_fields[REG_CHAN_EN], mask, val);
	if (rc < 0)
		return rc;

	led->enabled = state;
	return 0;
}

static int qcom_flash_brightness_set(struct led_classdev_flash *fled_cdev, u32 brightness)
{
	struct qcom_flash_led *led = container_of(fled_cdev, struct qcom_flash_led, flash);

	led->flash_current_ma = min_t(u32, led->max_flash_current_ma, brightness / 1000);
	return 0;
}

static int qcom_flash_timeout_set(struct led_classdev_flash *fled_cdev, u32 timeout)
{
	struct qcom_flash_led *led = container_of(fled_cdev, struct qcom_flash_led, flash);

	led->flash_timeout_ms = timeout / 1000;
	return 0;
}

static int qcom_flash_strobe_set(struct led_classdev_flash *fled_cdev, bool state)
{
	struct qcom_flash_led *led = container_of(fled_cdev, struct qcom_flash_led, flash);
	int rc;

	rc = set_flash_current(led, led->flash_current_ma, FLASH_MODE);
	if (rc < 0)
		return rc;

	rc = set_flash_timeout(led, led->flash_timeout_ms);
	if (rc < 0)
		return rc;

	rc = set_flash_module_en(led, state);
	if (rc < 0)
		return rc;

	return set_flash_strobe(led, SW_STROBE, state);
}

static int qcom_flash_strobe_get(struct led_classdev_flash *fled_cdev, bool *state)
{
	struct qcom_flash_led *led = container_of(fled_cdev, struct qcom_flash_led, flash);

	*state = led->enabled;
	return 0;
}

static int qcom_flash_fault_get(struct led_classdev_flash *fled_cdev, u32 *fault)
{
	struct qcom_flash_led *led = container_of(fled_cdev, struct qcom_flash_led, flash);
	struct qcom_flash_chip *chip = led->chip;
	u8 shift, chan_id = 0, chan_mask = 0;
	u8 ot_mask = 0, oc_mask = 0, uv_mask = 0;
	u32 val, fault_sts = 0;
	int i, rc;

	rc = regmap_field_read(chip->r_fields[REG_STATUS1], &val);
	if (rc < 0)
		return rc;

	for (i = 0; i < led->chan_count; i++) {
		chan_id = led->chan_id[i];
		shift = (chan_id - 1) * 2;
		if (val & BIT(shift))
			fault_sts |= LED_FAULT_SHORT_CIRCUIT;
		chan_mask |= BIT(chan_id - 1);
	}

	rc = regmap_field_read(chip->r_fields[REG_STATUS2], &val);
	if (rc < 0)
		return rc;

	if (chip->hw_type == QCOM_MVFLASH_3CH) {
		ot_mask = 0x0f;
		oc_mask = 0xe0;
		uv_mask = 0x10;
	} else if (chip->hw_type == QCOM_MVFLASH_4CH) {
		ot_mask = 0x70;
		oc_mask = 0x0e;
		uv_mask = 0x01;
	}

	if (val & ot_mask)
		fault_sts |= LED_FAULT_OVER_TEMPERATURE;
	if (val & oc_mask)
		fault_sts |= LED_FAULT_OVER_CURRENT;
	if (val & uv_mask)
		fault_sts |= LED_FAULT_INPUT_VOLTAGE;

	rc = regmap_field_read(chip->r_fields[REG_STATUS3], &val);
	if (rc < 0)
		return rc;

	if (chip->hw_type == QCOM_MVFLASH_3CH) {
		if (val & chan_mask)
			fault_sts |= LED_FAULT_TIMEOUT;
	} else if (chip->hw_type == QCOM_MVFLASH_4CH) {
		for (i = 0; i < led->chan_count; i++) {
			chan_id = led->chan_id[i];
			shift = (chan_id - 1) * 2;
			if (val & BIT(shift))
				fault_sts |= LED_FAULT_TIMEOUT;
		}
	}

	*fault = fault_sts;
	return 0;
}

static int qcom_flash_led_brightness_set(struct led_classdev *led_cdev,
					enum led_brightness brightness)
{
	struct led_classdev_flash *fled_cdev =
		container_of(led_cdev, struct led_classdev_flash, led_cdev);
	struct qcom_flash_led *led =
		container_of(fled_cdev, struct qcom_flash_led, flash);
	u32 current_ma = brightness * led->max_torch_current_ma / LED_FULL;
	bool enable = !!brightness;
	int rc;

	rc = set_flash_current(led, current_ma, TORCH_MODE);
	if (rc < 0)
		return rc;

	/* disable flash timeout for torch LED */
	rc = set_flash_timeout(led, 0);
	if (rc < 0)
		return rc;

	rc = set_flash_module_en(led, enable);
	if (rc < 0)
		return rc;

	return set_flash_strobe(led, SW_STROBE, enable);
}

static const struct led_flash_ops qcom_flash_ops = {
	.flash_brightness_set = qcom_flash_brightness_set,
	.strobe_set = qcom_flash_strobe_set,
	.strobe_get = qcom_flash_strobe_get,
	.timeout_set = qcom_flash_timeout_set,
	.fault_get = qcom_flash_fault_get,
};

#if IS_ENABLED(CONFIG_V4L2_FLASH_LED_CLASS)
static int qcom_flash_external_strobe_set(struct v4l2_flash *v4l2_flash, bool enable)
{
	struct led_classdev_flash *flash = v4l2_flash->fled_cdev;
	struct qcom_flash_led *led = container_of(flash, struct qcom_flash_led, flash);
	int rc;

	rc = set_flash_module_en(led, enable);
	if (rc < 0)
		return rc;

	if (enable)
		return set_flash_strobe(led, HW_STROBE, true);
	else
		return set_flash_strobe(led, SW_STROBE, false);
}

static enum led_brightness qcom_flash_intensity_to_led_brightness(
		struct v4l2_flash *v4l2_flash, s32 intensity)
{
	struct led_classdev_flash *flash = v4l2_flash->fled_cdev;
	struct qcom_flash_led *led = container_of(flash, struct qcom_flash_led, flash);
	u32 current_ma = intensity / 1000;

	current_ma = min_t(u32, current_ma, led->max_torch_current_ma);
	if (!current_ma)
		return LED_OFF;

	return current_ma * LED_FULL / led->max_torch_current_ma;
}

static s32 qcom_flash_brightness_to_led_intensity(struct v4l2_flash *v4l2_flash,
					enum led_brightness brightness)
{
	struct led_classdev_flash *flash = v4l2_flash->fled_cdev;
	struct qcom_flash_led *led = container_of(flash, struct qcom_flash_led, flash);

	return (brightness * led->max_torch_current_ma * 1000) / LED_FULL;
}

static const struct v4l2_flash_ops qcom_v4l2_flash_ops = {
	.external_strobe_set = qcom_flash_external_strobe_set,
	.intensity_to_led_brightness = qcom_flash_intensity_to_led_brightness,
	.led_brightness_to_intensity = qcom_flash_brightness_to_led_intensity,
};

static int qcom_flash_v4l2_init(struct qcom_flash_led *led, struct fwnode_handle *fwnode)
{
	struct v4l2_flash_config v4l2_cfg = {0};
	struct led_flash_setting *s = &v4l2_cfg.intensity;

	if (!(led->flash.led_cdev.flags & LED_DEV_CAP_FLASH))
		return 0;

	s->min = s->step = TORCH_IRES_UA * led->chan_count;
	s->max = led->max_torch_current_ma * 1000;
	s->val = min_t(u32, s->max, TORCH_CURRENT_DEFAULT_UA);

	strscpy(v4l2_cfg.dev_name, led->flash.led_cdev.dev->kobj.name,
					sizeof(v4l2_cfg.dev_name));
	v4l2_cfg.has_external_strobe = 1;
	v4l2_cfg.flash_faults = LED_FAULT_INPUT_VOLTAGE | LED_FAULT_OVER_CURRENT |
		LED_FAULT_SHORT_CIRCUIT | LED_FAULT_OVER_TEMPERATURE | LED_FAULT_TIMEOUT;

	led->v4l2_flash = v4l2_flash_init(led->chip->dev, fwnode, &led->flash,
					&qcom_v4l2_flash_ops, &v4l2_cfg);
	return PTR_ERR_OR_ZERO(led->v4l2_flash);
}
# else
static int qcom_flash_v4l2_init(struct qcom_flash_led *led, struct fwnode_handle *fwnode)
{
	return 0;
}
#endif

static int qcom_flash_register_led_device(struct device *parent,
		struct fwnode_handle *node, struct qcom_flash_led *led)
{
	struct qcom_flash_chip *chip = led->chip;
	struct led_init_data init_data;
	struct led_classdev_flash *flash;
	struct led_flash_setting *s;
	u32 count, val;
	u32 channels[4];
	int i, rc;

	flash = &led->flash;
	count = fwnode_property_count_u32(node, "led-sources");
	if (count <= 0) {
		dev_err(chip->dev, "No led-sources specified\n");
		return -ENODEV;
	}

	if (count > chip->max_channels) {
		dev_err(chip->dev, "led-sources count %u exceeds maximum channel count %u\n",
				count, chip->max_channels);
		return -EINVAL;
	}

	rc = fwnode_property_read_u32_array(node, "led-sources", channels, count);
	if (rc < 0) {
		dev_err(chip->dev, "get led-sources failed, rc=%d\n", rc);
		return rc;
	}

	led->chan_count = count;
	led->chan_id = devm_kcalloc(chip->dev, count, sizeof(u8), GFP_KERNEL);
	if (!led->chan_id)
		return -ENOMEM;

	for (i = 0; i < count; i++) {
		if (channels[i] > chip->max_channels) {
			dev_err(chip->dev, "led-source out of HW support range [1-%u]\n",
					chip->max_channels);
			return -EINVAL;
		}

		led->chan_id[i] = channels[i];
	}

	rc = fwnode_property_read_u32(node, "led-max-microamp", &val);
	if (rc < 0) {
		dev_err(chip->dev, "Get led-max-microamp failed, rc=%d\n", rc);
		return rc;
	}

	if (!val) {
		dev_err(chip->dev, "led-max-microamp shouldn't be 0\n");
		return -EINVAL;
	}

	val = min_t(u32, val, TORCH_CURRENT_MAX_UA * led->chan_count);
	led->max_torch_current_ma = val / 1000;

	if (fwnode_property_present(node, "flash-max-microamp")) {
		flash->led_cdev.flags |= LED_DEV_CAP_FLASH;
		rc = fwnode_property_read_u32(node, "flash-max-microamp", &val);
		if (rc < 0) {
			dev_err(chip->dev, "Get flash-max-microamp failed, rc=%d\n", rc);
			return rc;
		}

		val = min_t(u32, val, FLASH_CURRENT_MAX_UA * led->chan_count);
		val = min_t(u32, val, FLASH_TOTAL_CURRENT_MAX_UA);
		s = &flash->brightness;
		s->min = s->step = FLASH_IRES_UA * led->chan_count;
		s->max = val;
		s->val = min_t(u32, val, FLASH_CURRENT_DEFAULT_UA);
		led->max_flash_current_ma = val / 1000;
		led->flash_current_ma = s->val / 1000;

		rc = fwnode_property_read_u32(node, "flash-max-timeout-us", &val);
		if (rc < 0) {
			dev_err(chip->dev, "Get flash-max-timeout-us failed, rc=%d\n", rc);
			return rc;
		}

		val = min_t(u32, val, FLASH_TIMEOUT_MAX_US);
		s = &flash->timeout;
		s->min = s->step = FLASH_TIMEOUT_STEP_US;
		s->val = s->max = val;
		led->max_timeout_ms = led->flash_timeout_ms = val / 1000;

		flash->ops = &qcom_flash_ops;
	}

	flash->led_cdev.brightness_set_blocking = qcom_flash_led_brightness_set;
	init_data.fwnode = node;
	init_data.devicename = NULL;
	init_data.default_label = NULL;
	init_data.devname_mandatory = false;
	rc = devm_led_classdev_flash_register_ext(parent, flash, &init_data);
	if (rc < 0) {
		dev_err(chip->dev, "Register flash LED classdev failed, rc=%d\n", rc);
		return rc;
	}

	return qcom_flash_v4l2_init(led, node);
}

static int qcom_flash_led_probe(struct platform_device *pdev)
{
	struct qcom_flash_chip *chip;
	struct qcom_flash_led *led;
	struct fwnode_handle *child;
	struct device *dev = &pdev->dev;
	struct regmap *map;
	struct reg_field *regs;
	int count, i, rc;
	u32 val, reg_base;

	chip = devm_kzalloc(dev, sizeof(*chip), GFP_KERNEL);
	if (!chip)
		return -ENOMEM;

	map = dev_get_regmap(dev->parent, NULL);
	if (!map) {
		dev_err(dev, "Failed to get parent regmap\n");
		return -EINVAL;
	}

	rc = fwnode_property_read_u32(dev->fwnode, "reg", &reg_base);
	if (rc < 0) {
		dev_err(dev, "Failed to get register base address, rc=%d\n", rc);
		return rc;
	}

	rc = regmap_read(map, reg_base + FLASH_TYPE_REG, &val);
	if (rc < 0) {
		dev_err(dev, "Read flash module type failed, rc=%d\n", rc);
		return rc;
	}

	if (val != FLASH_TYPE_VAL) {
		dev_err(dev, "type %#x is not a flash module\n", val);
		return -ENODEV;
	}

	rc = regmap_read(map, reg_base + FLASH_SUBTYPE_REG, &val);
	if (rc < 0) {
		dev_err(dev, "Read flash module subtype failed, rc=%d\n", rc);
		return rc;
	}

	if (val == FLASH_SUBTYPE_3CH_VAL) {
		chip->hw_type = QCOM_MVFLASH_3CH;
		chip->max_channels = 3;
		regs = mvflash_3ch_regs;
	} else if (val == FLASH_SUBTYPE_4CH_VAL) {
		chip->hw_type = QCOM_MVFLASH_4CH;
		chip->max_channels = 4;
		regs = mvflash_4ch_regs;
	} else {
		dev_err(dev, "flash subtype %#x is not yet supported\n", val);
		return -ENODEV;
	}

	for (i = 0; i < REG_MAX_COUNT; i++)
		regs[i].reg += reg_base;

	rc = devm_regmap_field_bulk_alloc(dev, map, chip->r_fields, regs, REG_MAX_COUNT);
	if (rc < 0) {
		dev_err(dev, "failed to alloc regmap filed, rc=%d\n", rc);
		return rc;
	}

	chip->dev = dev;
	platform_set_drvdata(pdev, chip);
	mutex_init(&chip->lock);
	count = device_get_child_node_count(dev);
	if (count == 0 || count > chip->max_channels) {
		dev_err(dev, "No child or child count exceeds %d\n", chip->max_channels);
		return -EINVAL;
	}

	chip->leds = devm_kcalloc(dev, count, sizeof(*chip->leds), GFP_KERNEL);
	if (!chip->leds)
		return -ENOMEM;

	device_for_each_child_node(dev, child) {
		led = &chip->leds[chip->leds_count];
		led->chip = chip;
		rc = qcom_flash_register_led_device(dev, child, led);
		if (rc < 0)
			goto release;

		chip->leds_count++;
	}

	return 0;
release:
	while (chip->leds && chip->leds_count--)
		v4l2_flash_release(chip->leds[chip->leds_count].v4l2_flash);
	return rc;
}

static int qcom_flash_led_remove(struct platform_device *pdev)
{
	struct qcom_flash_chip *chip = platform_get_drvdata(pdev);

	while (chip->leds_count--)
		v4l2_flash_release(chip->leds[chip->leds_count].v4l2_flash);

	mutex_destroy(&chip->lock);
	return 0;
}

static const struct of_device_id qcom_flash_led_match_table[] = {
	{ .compatible = "qcom,spmi-flash-led" },
	{ }
};

static struct platform_driver qcom_flash_led_driver = {
	.driver = {
		.name = "leds-qcom-flash",
		.of_match_table = qcom_flash_led_match_table,
	},
	.probe = qcom_flash_led_probe,
	.remove = qcom_flash_led_remove,
};

module_platform_driver(qcom_flash_led_driver);

MODULE_DESCRIPTION("QCOM Flash LED driver");
MODULE_LICENSE("GPL");
