// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Semtech SX1301 LoRa transceiver
 *
 * Copyright (c) 2018 Andreas Färber
 *
 * Based on code:
 * Copyright (c) 2013 Semtech-Cycleo
 */

#include <linux/bitops.h>
#include <linux/debugfs.h>
#include <linux/delay.h>
#include <linux/module.h>
#include <linux/netdevice.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/spi/spi.h>

#define REG_PAGE_RESET			0
#define REG_VERSION			1
#define REG_2_SPI_RADIO_A_DATA		33
#define REG_2_SPI_RADIO_A_DATA_READBACK	34
#define REG_2_SPI_RADIO_A_ADDR		35
#define REG_2_SPI_RADIO_A_CS		37
#define REG_2_SPI_RADIO_B_DATA		38
#define REG_2_SPI_RADIO_B_DATA_READBACK	39
#define REG_2_SPI_RADIO_B_ADDR		40
#define REG_2_SPI_RADIO_B_CS		42

#define REG_PAGE_RESET_SOFT_RESET	BIT(7)

#define REG_16_GLOBAL_EN		BIT(3)

#define REG_17_CLK32M_EN		BIT(0)

#define REG_2_43_RADIO_A_EN		BIT(0)
#define REG_2_43_RADIO_B_EN		BIT(1)
#define REG_2_43_RADIO_RST		BIT(2)

static int sx1301_read(struct spi_device *spi, u8 reg, u8 *val)
{
	u8 addr = reg & 0x7f;
	return spi_write_then_read(spi, &addr, 1, val, 1);
}

static int sx1301_write(struct spi_device *spi, u8 reg, u8 val)
{
	u8 buf[2];

	buf[0] = reg | BIT(7);
	buf[1] = val;
	return spi_write(spi, buf, 2);
}

static int sx1301_page_switch(struct spi_device *spi, u8 page)
{
	dev_dbg(&spi->dev, "switching to page %u\n", (unsigned)page);
	return sx1301_write(spi, REG_PAGE_RESET, page & 0x3);
}

static int sx1301_soft_reset(struct spi_device *spi)
{
	return sx1301_write(spi, REG_PAGE_RESET, REG_PAGE_RESET_SOFT_RESET);
}

static int sx1301_radio_spi_common(struct spi_device *spi, u8 radio, u8 addr, u8 val)
{
	char *name = (radio == 1) ? "B" : "A";
	u8 reg_data, reg_addr, reg_cs;
	u8 cs;
	int ret;

	switch (radio)
	{
	case 0:
		reg_data = REG_2_SPI_RADIO_A_DATA;
		reg_addr = REG_2_SPI_RADIO_A_ADDR;
		reg_cs = REG_2_SPI_RADIO_A_CS;
		break;
	case 1:
		reg_data = REG_2_SPI_RADIO_B_DATA;
		reg_addr = REG_2_SPI_RADIO_B_ADDR;
		reg_cs = REG_2_SPI_RADIO_B_CS;
		break;
	default:
		return -EINVAL;
	}

	ret = sx1301_read(spi, reg_cs, &cs);
	if (ret) {
		dev_err(&spi->dev, "SPI radio %s CS read failed\n", name);
		return ret;
	}

	cs &= ~BIT(0);

	ret = sx1301_write(spi, reg_cs, cs);
	if (ret) {
		dev_err(&spi->dev, "SPI radio %s CS write failed\n", name);
		return ret;
	}

	ret = sx1301_write(spi, reg_addr, addr);
	if (ret) {
		dev_err(&spi->dev, "SPI radio %s address write failed\n", name);
		return ret;
	}

	ret = sx1301_write(spi, reg_data, val);
	if (ret) {
		dev_err(&spi->dev, "SPI radio %s data write failed\n", name);
		return ret;
	}

	ret = sx1301_read(spi, reg_cs, &cs);
	if (ret) {
		dev_err(&spi->dev, "SPI radio %s CS read failed\n", name);
		return ret;
	}

	cs |= BIT(0);

	ret = sx1301_write(spi, reg_cs, cs);
	if (ret) {
		dev_err(&spi->dev, "SPI radio %s CS write failed\n", name);
		return ret;
	}

	ret = sx1301_read(spi, reg_cs, &cs);
	if (ret) {
		dev_err(&spi->dev, "SPI radio %s CS read failed\n", name);
		return ret;
	}

	cs &= ~BIT(0);

	ret = sx1301_write(spi, reg_cs, cs);
	if (ret) {
		dev_err(&spi->dev, "SPI radio %s CS write failed\n", name);
		return ret;
	}

	return 0;
}

static int sx1301_radio_spi_write(struct spi_device *spi, u8 radio, u8 reg, u8 val)
{
	return sx1301_radio_spi_common(spi, radio, BIT(7) | reg, val);
}

static int sx1301_radio_spi_read(struct spi_device *spi, u8 radio, u8 reg, u8 *val)
{
	u8 reg_data_readback;
	int ret;

	switch (radio)
	{
	case 0:
		reg_data_readback = REG_2_SPI_RADIO_A_DATA_READBACK;
		break;
	case 1:
		reg_data_readback = REG_2_SPI_RADIO_B_DATA_READBACK;
		break;
	default:
		return -EINVAL;
	}

	ret = sx1301_radio_spi_common(spi, radio, reg & 0x7f, 0);
	if (ret)
		return ret;

	ret = sx1301_read(spi, reg_data_readback, val);
	if (ret) {
		dev_err(&spi->dev, "SPI radio %s data read failed\n",
			(radio == 1) ? "B" : "A");
		return ret;
	}

	return 0;
}

static int sx1301_radio_a_spi_read(struct spi_device *spi, u8 reg, u8 *val)
{
	return sx1301_radio_spi_read(spi, 0, reg, val);
}

static int sx1301_radio_b_spi_read(struct spi_device *spi, u8 reg, u8 *val)
{
	return sx1301_radio_spi_read(spi, 1, reg, val);
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

	ret = sx1301_read(spi, REG_VERSION, &val);
	if (ret) {
		dev_err(&spi->dev, "version read failed\n");
		return ret;
	}

	if (val != 103) {
		dev_err(&spi->dev, "unexpected version: %u\n", val);
		return -ENXIO;
	}

	ret = sx1301_write(spi, REG_PAGE_RESET, 0);
	if (ret) {
		dev_err(&spi->dev, "page/reset write failed\n");
		return ret;
	}

	ret = sx1301_soft_reset(spi);
	if (ret) {
		dev_err(&spi->dev, "soft reset failed\n");
		return ret;
	}

	ret = sx1301_read(spi, 16, &val);
	if (ret) {
		dev_err(&spi->dev, "16 read failed\n");
		return ret;
	}

	val &= ~REG_16_GLOBAL_EN;

	ret = sx1301_write(spi, 16, val);
	if (ret) {
		dev_err(&spi->dev, "16 write failed\n");
		return ret;
	}

	ret = sx1301_read(spi, 17, &val);
	if (ret) {
		dev_err(&spi->dev, "17 read failed\n");
		return ret;
	}

	val &= ~REG_17_CLK32M_EN;

	ret = sx1301_write(spi, 17, val);
	if (ret) {
		dev_err(&spi->dev, "17 write failed\n");
		return ret;
	}

	ret = sx1301_page_switch(spi, 2);
	if (ret) {
		dev_err(&spi->dev, "page 2 switch failed\n");
		return ret;
	}

	ret = sx1301_read(spi, 43, &val);
	if (ret) {
		dev_err(&spi->dev, "2|43 read failed\n");
		return ret;
	}

	val |= REG_2_43_RADIO_B_EN | REG_2_43_RADIO_A_EN;

	ret = sx1301_write(spi, 43, val);
	if (ret) {
		dev_err(&spi->dev, "2|43 write failed\n");
		return ret;
	}

	msleep(500);

	ret = sx1301_read(spi, 43, &val);
	if (ret) {
		dev_err(&spi->dev, "2|43 read failed\n");
		return ret;
	}

	val |= REG_2_43_RADIO_RST;

	ret = sx1301_write(spi, 43, val);
	if (ret) {
		dev_err(&spi->dev, "2|43 write failed\n");
		return ret;
	}

	msleep(5);

	ret = sx1301_read(spi, 43, &val);
	if (ret) {
		dev_err(&spi->dev, "2|43 read failed\n");
		return ret;
	}

	val &= ~REG_2_43_RADIO_RST;

	ret = sx1301_write(spi, 43, val);
	if (ret) {
		dev_err(&spi->dev, "2|43 write failed\n");
		return ret;
	}

	/* radio A */

	if (false) {
		ret = sx1301_radio_a_spi_read(spi, 0x07, &val);
		if (ret) {
			dev_err(&spi->dev, "radio A version read failed\n");
			return ret;
		}

		dev_info(&spi->dev, "radio A SX125x version: %02x\n", (unsigned)val);
	}

	ret = sx1301_radio_spi_write(spi, 0, 0x10, 1);
	if (ret) {
		dev_err(&spi->dev, "radio A clk write failed\n");
		return ret;
	}

	if (true) {
		ret = sx1301_radio_spi_write(spi, 0, 0x26, 13 + 2 * 16);
		if (ret) {
			dev_err(&spi->dev, "radio A xosc write failed\n");
			return ret;
		}
	}

	/* radio B */

	if (false) {
		ret = sx1301_radio_b_spi_read(spi, 0x07, &val);
		if (ret) {
			dev_err(&spi->dev, "radio B version read failed\n");
			return ret;
		}

		dev_info(&spi->dev, "radio B SX125x version: %02x\n", (unsigned)val);
	}

	ret = sx1301_radio_spi_write(spi, 1, 0x10, 1 + 2);
	if (ret) {
		dev_err(&spi->dev, "radio B clk write failed\n");
		return ret;
	}

	if (true) {
		ret = sx1301_radio_spi_write(spi, 1, 0x26, 13 + 2 * 16);
		if (ret) {
			dev_err(&spi->dev, "radio B xosc write failed\n");
			return ret;
		}
	}

	dev_info(&spi->dev, "SX1301 module probed\n");

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
