// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Semtech SX1301 LoRa transceiver
 *
 * Copyright (c) 2018 Andreas Färber
 */

#include <linux/debugfs.h>
#include <linux/delay.h>
#include <linux/module.h>
#include <linux/netdevice.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/spi/spi.h>

static int sx1301_read(struct spi_device *spi, u8 reg, u8 *val)
{
	u8 addr = reg & 0x7f;
	return spi_write_then_read(spi, &addr, 1, val, 1);
}

static int sx1301_probe(struct spi_device *spi)
{
	struct gpio_desc *rst;
	int ret;
	u8 val;

	rst = devm_gpiod_get_optional(&spi->dev, "reset", GPIOD_OUT_LOW);
	if (IS_ERR(rst))
		return PTR_ERR(rst);

	gpiod_set_value_cansleep(rst, 1);
	msleep(100);
	gpiod_set_value_cansleep(rst, 0);
	msleep(100);

	spi->bits_per_word = 8;
	spi_setup(spi);

	ret = sx1301_read(spi, 1, &val);
	if (ret) {
		dev_err(&spi->dev, "version read failed");
		return ret;
	}

	dev_info(&spi->dev, "SX1301 module probed (%u)\n", (unsigned)val);

	return 0;
}

static int sx1301_remove(struct spi_device *spi)
{
	dev_info(&spi->dev, "SX1301 module removed\n");

	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id sx1301_dt_ids[] = {
	{ .compatible = "semtech,sx1301" },
	{}
};
MODULE_DEVICE_TABLE(of, sx1301_dt_ids);
#endif

static struct spi_driver sx1301_spi_driver = {
	.driver = {
		.name = "sx1301",
		.of_match_table = of_match_ptr(sx1301_dt_ids),
	},
	.probe = sx1301_probe,
	.remove = sx1301_remove,
};

module_spi_driver(sx1301_spi_driver);

MODULE_DESCRIPTION("SX1301 SPI driver");
MODULE_AUTHOR("Andreas Färber <afaerber@suse.de>");
MODULE_LICENSE("GPL");
