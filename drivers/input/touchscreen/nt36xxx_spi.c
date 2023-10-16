// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * NT36XXX SPI Touchscreen Driver
 *
 * Copyright (C) 2020 - 2021 Goodix, Inc.
 * Copyright (C) 2023 Linaro Ltd.
 * Copyright (C) 2023 99degree <www.github.com/99degree>
 *
 * Based on goodix_ts_berlin driver.
 */
#include <linux/input.h>
#include <linux/input/touchscreen.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/regmap.h>
#include <linux/spi/spi.h>
#include <linux/unaligned.h>

#include "nt36xxx.h"

#define SPI_READ_PREFIX_LEN	1
#define SPI_WRITE_PREFIX_LEN	1

#define DEBUG 0

/*
 * there are two kinds of spi read/write:
 * 	(a)spi_read()/spi_write()/spi_write_then_read(),
 * 	(b)and the spi_sync itself.
 *
 * we have to choose one and stick together, cross-use otherwise caused problem.
 * the addressing mode is | 0xff 0xXX 0xYY | 0xZ1 ... data1...| 0xZ2 ...data2... | ...
 * 	0xXX is bit[23..16]
 * 	0xYY is bit[15..7]
 * above describe a 'page select' ops
 * 	0xZ1 is bit[7..0], addr for read ops
 *	0xZ2 is bit[7..0] | 0x80, addr for write ops
 * there is no restriction on the read write order.
*/
static int nt36xxx_spi_write(void *dev, const void *data,
                                   size_t len)
{
	struct spi_device *spi = to_spi_device((struct device *)dev);
	int32_t ret;

	u8 addr[4] = { 0xff, *(u32 *)data >> 15, *(u32 *)data >> 7,  (*(u32 *)data & 0x7f) | 0x80};

	memcpy((void *)data, addr, 4);

	dev_dbg(dev, "%s len=0x%lx", __func__, len);

	spi_write(spi, data, 3);
	ret = spi_write(spi, (u8 *)data + 3, len - 3);
	if (ret)
		dev_err(dev, "transfer err %d\n ", ret);
	else if (DEBUG) {

		print_hex_dump(KERN_INFO, __func__, DUMP_PREFIX_OFFSET,
			16, 1, data, 3, true);

		print_hex_dump(KERN_INFO, __func__, DUMP_PREFIX_OFFSET,
			16, 1, data + 3, (len - 3) > 0x20 ? 0x20 : len - 3 , true);
	}

	return ret;
}

static int nt36xxx_spi_read(void *dev, const void *reg_buf,
                                  size_t reg_size, void *val_buf,
                                  size_t val_size)
{
	struct spi_device *spi = to_spi_device(dev);
	int ret;
	u8 addr[4] = { 0xff, *(u32 *)reg_buf >> 15, *(u32 *)reg_buf >> 7,  *(u32 *)reg_buf & 0x7f };

	ret = spi_write(spi, addr, 3);
	if (ret) {
		dev_err(dev, "transfer0 err %s %d ret=%d", __func__, __LINE__, ret);
		return ret;
	}

	ret = spi_write_then_read(spi, &addr[3] , 1, val_buf, val_size);
	if (ret) {
		dev_err(dev, "transfer1 err %s %d ret=%d", __func__, __LINE__, ret);
		return ret;
	}

	if (DEBUG) {
		print_hex_dump(KERN_INFO, __func__, DUMP_PREFIX_OFFSET,
			16, 1, addr, 3, true);

		print_hex_dump(KERN_INFO, __func__, DUMP_PREFIX_OFFSET,
			16, 1, addr, (val_size) > 0x20 ? 0x20 : val_size % 0x20 , true);

		print_hex_dump(KERN_INFO, __func__, DUMP_PREFIX_OFFSET,
			16, 1, val_buf, (val_size > 0x20) ? 0x20 : val_size % 0x20 , true);
	}

	return ret;
}

const struct regmap_config nt36xxx_regmap_config_32bit = {
	.name = "nt36xxx_hw",
	.reg_bits = 32,
	.val_bits = 8,
	.read = nt36xxx_spi_read,
	.write = nt36xxx_spi_write,

	.max_raw_read = NT36XXX_TRANSFER_LEN + 8,
	.max_raw_write = NT36XXX_TRANSFER_LEN + 8,

	.zero_flag_mask = true, /* this is needed to make sure addr is not write_masked */
	.cache_type = REGCACHE_NONE,
};

static const struct input_id nt36xxx_spi_input_id = {
	.bustype = BUS_SPI,
};

static int nt36xxx_spi_probe(struct spi_device *spi)
{
	struct regmap_config *regmap_config;
	struct regmap *regmap;
	size_t max_size;
	int ret = 0;

	/* Debug log indicating entry into the probe function */
	dev_dbg(&spi->dev, "%s %d", __func__, __LINE__);

	/* Allocate and copy the default regmap configuration */
	regmap_config = devm_kmemdup(&spi->dev, &nt36xxx_regmap_config_32bit,
				     sizeof(*regmap_config), GFP_KERNEL);
	if (!regmap_config) {
		dev_err(&spi->dev, "memdup regmap_config fail\n");
		return -ENOMEM;
	}

	/* Set SPI mode and bits per word, and perform SPI setup */
	spi->mode = SPI_MODE_0;
	spi->bits_per_word = 8;
	ret = spi_setup(spi);
	if (ret) {
		dev_err(&spi->dev, "SPI setup error %d\n", ret);
		return ret;
	}

	/* Ensure SPI CLK frequency does not exceed the maximum specified frequency */
	if (spi->max_speed_hz > MAX_SPI_FREQ_HZ) {
		dev_err(&spi->dev, "SPI CLK %d Hz?\n", spi->max_speed_hz);
		return -EINVAL;
	}

	/* Calculate the maximum raw read and write sizes based on SPI transfer size */
	max_size = spi_max_transfer_size(spi);
	regmap_config->max_raw_read = max_size - SPI_READ_PREFIX_LEN;
	regmap_config->max_raw_write = max_size - SPI_WRITE_PREFIX_LEN;

	/* Initialize the regmap using the provided configuration */
	regmap = devm_regmap_init(&spi->dev, NULL, spi, regmap_config);
	if (IS_ERR(regmap))
		return PTR_ERR(regmap);

	/* Call the main probe function for nt36xxx devices */
	return nt36xxx_probe(&spi->dev, spi->irq,
				   &nt36xxx_spi_input_id, regmap);
}

const struct nt36xxx_chip_data novatek_nt36xxx = {
	.config = &nt36xxx_regmap_config_32bit,
	.mmap = nt36675_memory_maps,
	.abs_x_max = 1080,
	.abs_y_max = 2400,
	.id = &nt36xxx_spi_input_id,
};

static const struct spi_device_id nt36xxx_spi_ids[] = {
	{ "nt36xxx-spi", 0 },
	{ },
};
MODULE_DEVICE_TABLE(spi, nt36xxx_spi_ids);

static const struct of_device_id nt36xxx_spi_of_match[] = {
	{ .compatible = "novatek,nt36xxx-spi", .data = &novatek_nt36xxx, },
	{ }
};
MODULE_DEVICE_TABLE(of, nt36xxx_spi_of_match);

static struct spi_driver nt36xxx_spi_driver = {
	.driver = {
		.name   = "nt36xxx-spi",
		.of_match_table = nt36xxx_spi_of_match,
		.pm = pm_sleep_ptr(&nt36xxx_pm_ops),
	},
	.probe = nt36xxx_spi_probe,
	.id_table = nt36xxx_spi_ids,
};
module_spi_driver(nt36xxx_spi_driver);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("NT36XXX SPI Touchscreen driver");
MODULE_AUTHOR("Neil Armstrong <neil.armstrong@linaro.org>");
MODULE_AUTHOR("99degree <www.github.com/99degree>");
