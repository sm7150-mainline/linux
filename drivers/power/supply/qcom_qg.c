// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2024, Danila Tikhonov <danila@jiaxyga.com>
 */

#include <linux/iio/consumer.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/jiffies.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/nvmem-consumer.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/platform_device.h>
#include <linux/power_supply.h>
#include <linux/regmap.h>

/* BATT offsets */
#define QG_S2_NORMAL_AVG_V_DATA0_REG	0x80 /* 2-byte 0x80-0x81 */
#define QG_S2_NORMAL_AVG_I_DATA0_REG	0x82 /* 2-byte 0x82-0x83 */
#define QG_LAST_ADC_V_DATA0_REG		0xc0 /* 2-byte 0xc0-0xc1 */
#define QG_LAST_ADC_I_DATA0_REG		0xc2 /* 2-byte 0xc2-0xc3 */

/* PERPH offsets */
#define QG_PERPH_SUBTYPE_REG		0x05
#define QG_ADC_IBAT_5A			0x03
#define QG_ADC_IBAT_10A			0x04

/* SRAM offsets */
#define QG_SDAM_OCV_OFFSET		0x4c /* 4-byte 0x4c-0x4f */
#define QG_SDAM_LEARNED_CAPACITY_OFFSET	0x68 /* 2-byte 0x68-0x69 */

/* Larger IR drops come from glitched current readings, not the battery */
#define QG_MAX_IR_DROP_UV		300000

/* Fastest change of the reported capacity, 1% per step */
#define QG_MSOC_STEP_MS			20000

struct qcom_qg_chip {
	struct device *dev;
	struct regmap *regmap;
	unsigned int base;
	int ibat_lsb_na;

	/* Reported capacity, -1 until the first read */
	struct mutex lock;
	int msoc;
	unsigned long msoc_jiffies;

	struct iio_channel *batt_therm_chan;

	struct nvmem_device *sdam;

	struct power_supply *batt_psy;
	struct power_supply_battery_info *batt_info;
};

static int qcom_qg_get_current(struct qcom_qg_chip *chip, u8 offset, int *val)
{
	s16 temp;
	u8 readval[2];
	int ret;

	ret = regmap_bulk_read(chip->regmap, chip->base + offset, readval, 2);
	if (ret) {
		dev_err(chip->dev, "Failed to read current: %d\n", ret);
		return ret;
	}

	temp = (s16)(readval[1] << 8 | readval[0]);
	*val = div_s64((s64)temp * chip->ibat_lsb_na, 1000);

	/*
	 * PSY API expects charging batteries to report a positive current, which is inverted
	 * to what the PMIC reports.
	 */
	*val = -*val;

	return 0;
}

static int qcom_qg_get_voltage(struct qcom_qg_chip *chip, u8 offset, int *val)
{
	int ret, temp;
	u8 readval[2];

	ret = regmap_bulk_read(chip->regmap, chip->base + offset, readval, 2);
	if (ret) {
		dev_err(chip->dev, "Failed to read voltage: %d\n", ret);
		return ret;
	}

	temp = readval[1] << 8 | readval[0];
	*val = div_u64((u64)temp * 194637, 1000);

	return 0;
}

/*
 * Follow the charger while it is online, it knows when charging has
 * terminated. Otherwise guess from the direction of the battery current.
 */
static int qcom_qg_get_status(struct qcom_qg_chip *chip,
			      struct power_supply *psy, int *val)
{
	union power_supply_propval charger;
	int ret, current_now;

	ret = power_supply_get_property_from_supplier(psy, POWER_SUPPLY_PROP_STATUS,
						      &charger);
	if (!ret && (charger.intval == POWER_SUPPLY_STATUS_CHARGING ||
		     charger.intval == POWER_SUPPLY_STATUS_FULL ||
		     charger.intval == POWER_SUPPLY_STATUS_NOT_CHARGING)) {
		*val = charger.intval;
		return 0;
	}

	ret = qcom_qg_get_current(chip, QG_LAST_ADC_I_DATA0_REG, &current_now);
	if (ret)
		return ret;

	*val = current_now > 0 ? POWER_SUPPLY_STATUS_CHARGING :
				 POWER_SUPPLY_STATUS_DISCHARGING;

	return 0;
}

/*
 * Voltage based estimates jump with the load, e.g. by ~15% when a charger is
 * plugged in. Let the reported capacity follow them by at most 1% per
 * QG_MSOC_STEP_MS, and only in the direction the battery current flows.
 */
static int qcom_qg_smooth_capacity(struct qcom_qg_chip *chip, int soc,
				   bool charging)
{
	unsigned long now = jiffies;
	unsigned int steps;

	guard(mutex)(&chip->lock);

	if (chip->msoc < 0) {
		chip->msoc = soc;
		chip->msoc_jiffies = now;
		return soc;
	}

	steps = (now - chip->msoc_jiffies) / msecs_to_jiffies(QG_MSOC_STEP_MS);
	if (!steps)
		return chip->msoc;

	if (charging && soc > chip->msoc)
		chip->msoc += min_t(unsigned int, soc - chip->msoc, steps);
	else if (!charging && soc < chip->msoc)
		chip->msoc -= min_t(unsigned int, chip->msoc - soc, steps);
	chip->msoc_jiffies = now;

	return chip->msoc;
}

/*
 * With an OCV table, estimate the open-circuit voltage by removing the IR drop
 * of the battery and look it up. Otherwise interpolate the average voltage
 * linearly between the design limits.
 */
static int qcom_qg_get_capacity(struct qcom_qg_chip *chip,
				struct power_supply *psy, int *val)
{
	struct power_supply_battery_info *info = chip->batt_info;
	int ret, voltage_now, current_now, ir_drop = 0, temp, soc, status;
	int voltage_min = chip->batt_info->voltage_min_design_uv;
	int voltage_max = chip->batt_info->voltage_max_design_uv;

	ret = qcom_qg_get_current(chip, QG_LAST_ADC_I_DATA0_REG, &current_now);
	if (ret)
		return ret;

	if (info->ocv_table[0]) {
		ret = qcom_qg_get_voltage(chip, QG_LAST_ADC_V_DATA0_REG,
					  &voltage_now);
		if (ret)
			return ret;

		ret = iio_read_channel_processed(chip->batt_therm_chan, &temp);
		if (ret < 0)
			return ret;

		if (info->factory_internal_resistance_uohm > 0)
			ir_drop = div_s64((s64)current_now *
					  info->factory_internal_resistance_uohm,
					  1000000);
		ir_drop = clamp(ir_drop, -QG_MAX_IR_DROP_UV, QG_MAX_IR_DROP_UV);

		/* millidegC to degC as used by ocv-capacity-celsius */
		soc = power_supply_batinfo_ocv2cap(info, voltage_now - ir_drop,
						   temp / 1000);
	} else {
		ret = qcom_qg_get_voltage(chip,
					QG_S2_NORMAL_AVG_V_DATA0_REG, &voltage_now);
		if (ret) {
			dev_err(chip->dev, "Failed to get current voltage: %d\n", ret);
			return ret;
		}

		if (voltage_now <= voltage_min)
			soc = 0;
		else if (voltage_now >= voltage_max)
			soc = 100;
		else
			soc = (((voltage_now - voltage_min) * 100) /
						(voltage_max - voltage_min));
	}

	ret = qcom_qg_get_status(chip, psy, &status);
	if (ret)
		return ret;

	/* A terminated charge means full, whatever the estimate says */
	if (status == POWER_SUPPLY_STATUS_FULL)
		soc = 100;

	*val = qcom_qg_smooth_capacity(chip, soc, current_now > 0 ||
					status == POWER_SUPPLY_STATUS_FULL);

	return 0;
}

static enum power_supply_property qcom_qg_props[] = {
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_TECHNOLOGY,
	POWER_SUPPLY_PROP_VOLTAGE_MAX_DESIGN,
	POWER_SUPPLY_PROP_VOLTAGE_MIN_DESIGN,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_VOLTAGE_AVG,
	POWER_SUPPLY_PROP_VOLTAGE_OCV,
	POWER_SUPPLY_PROP_CURRENT_NOW,
	POWER_SUPPLY_PROP_CURRENT_AVG,
	POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN,
	POWER_SUPPLY_PROP_CHARGE_FULL,
	POWER_SUPPLY_PROP_CAPACITY,
	POWER_SUPPLY_PROP_TEMP,
};

static int qcom_qg_get_property(struct power_supply *psy,
				enum power_supply_property psp,
				union power_supply_propval *val)
{
	struct qcom_qg_chip *chip = power_supply_get_drvdata(psy);
	int ret;

	switch (psp) {
	case POWER_SUPPLY_PROP_STATUS:
		ret = qcom_qg_get_status(chip, psy, &val->intval);
		if (ret)
			return ret;
		break;
	case POWER_SUPPLY_PROP_TECHNOLOGY:
		val->intval = POWER_SUPPLY_TECHNOLOGY_LION;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_MAX_DESIGN:
		val->intval = chip->batt_info->voltage_max_design_uv;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_MIN_DESIGN:
		val->intval = chip->batt_info->voltage_min_design_uv;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		ret = qcom_qg_get_voltage(chip,
				QG_LAST_ADC_V_DATA0_REG, &val->intval);
		if (ret)
			return ret;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_AVG:
		ret = qcom_qg_get_voltage(chip,
				QG_S2_NORMAL_AVG_V_DATA0_REG, &val->intval);
		if (ret)
			return ret;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_OCV:
		ret = nvmem_device_read(chip->sdam, QG_SDAM_OCV_OFFSET, 4, &val->intval);
		if (ret < 0)
			return ret;
		break;
	case POWER_SUPPLY_PROP_CURRENT_NOW:
		ret = qcom_qg_get_current(chip,
				QG_LAST_ADC_I_DATA0_REG, &val->intval);
		if (ret)
			return ret;
		break;
	case POWER_SUPPLY_PROP_CURRENT_AVG:
		ret = qcom_qg_get_current(chip,
				QG_S2_NORMAL_AVG_I_DATA0_REG, &val->intval);
		if (ret)
			return ret;
		break;
	case POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN:
		val->intval = chip->batt_info->charge_full_design_uah;
		break;
	case POWER_SUPPLY_PROP_CHARGE_FULL:
		ret = nvmem_device_read(chip->sdam,
				QG_SDAM_LEARNED_CAPACITY_OFFSET, 2, &val->intval);
		if (ret < 0)
			return ret;
		val->intval *= 1000; /* mAh to uAh */
		break;
	case POWER_SUPPLY_PROP_CAPACITY:
		ret = qcom_qg_get_capacity(chip, psy, &val->intval);
		if (ret)
			return ret;
		break;
	case POWER_SUPPLY_PROP_TEMP:
		ret = iio_read_channel_processed
					(chip->batt_therm_chan, &val->intval);
		if (ret < 0)
			return ret;
		val->intval /= 100; /* 1/1000 °C (millidegC) to 1/10 °C */
		break;
	default:
		dev_err(chip->dev, "invalid property: %d\n", psp);
		return -EINVAL;
	}
	return 0;
}

static struct power_supply_desc batt_psy_desc = {
	.name = "qcom_qg",
	.type = POWER_SUPPLY_TYPE_BATTERY,
	.properties = qcom_qg_props,
	.num_properties = ARRAY_SIZE(qcom_qg_props),
	.get_property = qcom_qg_get_property,
	.external_power_changed = power_supply_changed,
};

static int qcom_qg_probe(struct platform_device *pdev)
{
	struct qcom_qg_chip *chip;
	struct power_supply_config psy_cfg = {};
	unsigned int subtype;
	int ret;

	chip = devm_kzalloc(&pdev->dev, sizeof(*chip), GFP_KERNEL);
	if (!chip)
		return -ENOMEM;

	chip->dev = &pdev->dev;

	/* Regmap */
	chip->regmap = dev_get_regmap(chip->dev->parent, NULL);
	if (!chip->regmap)
		return dev_err_probe(chip->dev, -ENODEV,
				     "Failed to locate the regmap\n");

	/* Get base address */
	ret = device_property_read_u32(chip->dev, "reg", &chip->base);
	if (ret < 0)
		return dev_err_probe(chip->dev, ret,
				     "Couldn't read base address\n");

	/* The subtype tells the IBAT ADC range and thus the current LSB */
	ret = regmap_read(chip->regmap, chip->base + QG_PERPH_SUBTYPE_REG, &subtype);
	if (ret)
		return dev_err_probe(chip->dev, ret,
				     "Couldn't read subtype\n");

	switch (subtype) {
	case QG_ADC_IBAT_10A:
		chip->ibat_lsb_na = 305176;
		break;
	default:
		dev_warn(chip->dev, "Unknown subtype %#x, assuming 5A range\n",
			 subtype);
		fallthrough;
	case QG_ADC_IBAT_5A:
		chip->ibat_lsb_na = 152588;
		break;
	}

	/* ADC for thermal channel */
	chip->batt_therm_chan = devm_iio_channel_get(chip->dev, "batt-therm");
	if (IS_ERR(chip->batt_therm_chan))
		return dev_err_probe(chip->dev, PTR_ERR(chip->batt_therm_chan),
				     "Couldn't get batt-therm IIO channel\n");

	/* NVMEM for SDAM access */
	chip->sdam = devm_nvmem_device_get(chip->dev, NULL);
	if (IS_ERR(chip->sdam))
		return dev_err_probe(chip->dev, PTR_ERR(chip->sdam),
				     "Couldn't get SDAM nvmem device\n");

	chip->msoc = -1;
	ret = devm_mutex_init(chip->dev, &chip->lock);
	if (ret)
		return ret;

	psy_cfg.drv_data = chip;
	psy_cfg.fwnode = dev_fwnode(chip->dev);

	/* Power supply */
	chip->batt_psy =
		devm_power_supply_register(chip->dev, &batt_psy_desc, &psy_cfg);
	if (IS_ERR(chip->batt_psy))
		return dev_err_probe(chip->dev, PTR_ERR(chip->batt_psy),
				     "Failed to register power supply\n");

	/* Battery info */
	ret = power_supply_get_battery_info(chip->batt_psy, &chip->batt_info);
	if (ret)
		return dev_err_probe(chip->dev, ret,
				     "Failed to get battery info\n");

	platform_set_drvdata(pdev, chip);

	return 0;
}

static const struct of_device_id qcom_qg_of_match[] = {
	{ .compatible = "qcom,pm6150-qg", },
	{ /* sentinel */ },
};
MODULE_DEVICE_TABLE(of, qcom_qg_of_match);

static struct platform_driver qcom_qg_driver = {
	.driver = {
		.name = "qcom,qcom_qg",
		.of_match_table = qcom_qg_of_match,
	},
	.probe = qcom_qg_probe,
};

module_platform_driver(qcom_qg_driver);

MODULE_AUTHOR("Danila Tikhonov <danila@jiaxyga.com>");
MODULE_DESCRIPTION("Qualcomm PMIC QGauge (QG) driver");
MODULE_LICENSE("GPL");
