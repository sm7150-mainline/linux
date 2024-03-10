// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2024, Danila Tikhonov <danila@jiaxyga.com>
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/of_address.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/power_supply.h>
#include <linux/iio/consumer.h>
#include <linux/regmap.h>

struct smb5_chip {
	struct device *dev;
	const char *name;
	unsigned int base;
	struct regmap *regmap;

	struct power_supply *chg_psy;
	struct power_supply_battery_info *batt_info;
};

static enum power_supply_property smb5_props[] = {
	POWER_SUPPLY_PROP_MANUFACTURER,
};

static int smb5_get_property(struct power_supply *psy,
			     enum power_supply_property psp,
			     union power_supply_propval *val)
{
	struct smb5_chip *chip = power_supply_get_drvdata(psy);

	switch (psp) {
	case POWER_SUPPLY_PROP_MANUFACTURER:
		val->strval = "Qualcomm";
		return 0;
	default:
		dev_err(chip->dev, "invalid property: %d\n", psp);
		return -EINVAL;
	}
}

static const struct power_supply_desc smb5_psy_desc = {
	.name = "smb5_charger",
	.type = POWER_SUPPLY_TYPE_USB,
	.properties = smb5_props,
	.num_properties = ARRAY_SIZE(smb5_props),
	.get_property = smb5_get_property,
};

static int smb5_probe(struct platform_device *pdev)
{
	struct power_supply_config psy_cfg = {};
	struct smb5_chip *chip;
	int ret;

	chip = devm_kzalloc(&pdev->dev, sizeof(*chip), GFP_KERNEL);
	if (!chip)
		return -ENOMEM;

	chip->dev = &pdev->dev;
	chip->name = pdev->name;

	/* Regmap */
	chip->regmap = dev_get_regmap(pdev->dev.parent, NULL);
	if (!chip->regmap)
		return dev_err_probe(chip->dev, -ENODEV,
				     "Failed to locate the regmap\n");

	/* Get base address */
	ret = device_property_read_u32(chip->dev, "reg", &chip->base);
	if (ret < 0)
		return dev_err_probe(chip->dev, ret,
				     "Couldn't read base address\n");

	psy_cfg.drv_data = chip;
	psy_cfg.of_node = pdev->dev.of_node;

	/* Charger power supply */
	chip->chg_psy =
		devm_power_supply_register(chip->dev, &smb5_psy_desc, &psy_cfg);
	if (IS_ERR(chip->chg_psy))
		return dev_err_probe(chip->dev, PTR_ERR(chip->chg_psy),
				     "Failed to register power supply\n");

	/* Battery info */
	ret = power_supply_get_battery_info(chip->chg_psy, &chip->batt_info);
	if (ret)
		return dev_err_probe(chip->dev, ret,
				     "Failed to get battery info\n");

	platform_set_drvdata(pdev, chip);

	return 0;
}

static const struct of_device_id smb5_of_match[] = {
	{ .compatible = "qcom,pm6150-charger", },
	{ /* sentinel */ },
};
MODULE_DEVICE_TABLE(of, smb5_of_match);

static struct platform_driver smb5_driver = {
	.driver = {
		.name = "qcom,smb5",
		.of_match_table = smb5_of_match,
	},
	.probe = smb5_probe,
};

module_platform_driver(smb5_driver);

MODULE_AUTHOR("Danila Tikhonov <danila@jiaxyga.com>");
MODULE_DESCRIPTION("Qualcomm PMIC smb5 Charger Driver");
MODULE_LICENSE("GPL");
