/*
 * Semtech SX1276 LoRa transceiver
 *
 * Copyright (c) 2016-2017 Andreas Färber
 *
 * SPDX-License-Identifier: GPL-2.0+
 */

#include <linux/delay.h>
#include <linux/module.h>
#include <linux/netdevice.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/spi/spi.h>

#include "af_lora.h"
#include "lora.h"

#define REG_FIFO			0x00
#define REG_OPMODE			0x01
#define REG_FRF_MSB			0x06
#define REG_FRF_MID			0x07
#define REG_FRF_LSB			0x08
#define LORA_REG_FIFO_ADDR_PTR		0x0d
#define LORA_REG_FIFO_TX_BASE_ADDR	0x0e
#define REG_VERSION			0x42

#define REG_OPMODE_LONG_RANGE_MODE		BIT(7)
#define REG_OPMODE_LOW_FREQUENCY_MODE_ON	BIT(3)
#define REG_OPMODE_MODE_MASK			GENMASK(2, 0)
#define REG_OPMODE_MODE_SLEEP			(0x0 << 0)
#define REG_OPMODE_MODE_STDBY			(0x1 << 0)
#define REG_OPMODE_MODE_TX			(0x3 << 0)
#define REG_OPMODE_MODE_RXCONTINUOUS		(0x5 << 0)
#define REG_OPMODE_MODE_RXSINGLE		(0x6 << 0)

struct sx1276_priv {
	struct lora_priv lora;
	size_t fifosize;
};

static int sx1276_read_single(struct spi_device *spi, u8 reg, u8 *val)
{
	u8 addr = reg & 0x7f;
	return spi_write_then_read(spi, &addr, 1, val, 1);
}

static int sx1276_write_single(struct spi_device *spi, u8 reg, u8 val)
{
	u8 buf[2];

	buf[0] = reg | BIT(7);
	buf[1] = val;
	return spi_write(spi, buf, 2);
}

static int sx1276_write_burst(struct spi_device *spi, u8 reg, size_t len, void *val)
{
	u8 buf = reg | BIT(7);
	struct spi_transfer xfers[2] = {
		[0] = {
			.tx_buf = &buf,
			.len = 1,
		},
		[1] = {
			.tx_buf = val,
			.len = len,
		},
	};

	return spi_sync_transfer(spi, xfers, 2);
}

static int sx1276_write_fifo(struct spi_device *spi, size_t len, void *val)
{
	return sx1276_write_burst(spi, REG_FIFO, len, val);
}

static netdev_tx_t sx1276_loradev_start_xmit(struct sk_buff *skb, struct net_device *netdev)
{
	struct spi_device *spi = to_spi_device(netdev->dev.parent);
	u8 addr, val;
	int ret;

	if (skb->protocol != htons(ETH_P_LORA)) {
		kfree_skb(skb);
		netdev->stats.tx_dropped++;
		return NETDEV_TX_OK;
	}

	netif_stop_queue(netdev);

	ret = sx1276_read_single(spi, LORA_REG_FIFO_TX_BASE_ADDR, &addr);
	if (ret < 0) {
		netdev->stats.tx_dropped++;
		return NETDEV_TX_OK;
	}

	ret = sx1276_write_single(spi, LORA_REG_FIFO_ADDR_PTR, addr);
	if (ret < 0) {
		netdev->stats.tx_dropped++;
		return NETDEV_TX_OK;
	}

	ret = sx1276_write_fifo(spi, skb->data_len, skb->data);
	if (ret < 0) {
		netdev->stats.tx_dropped++;
		return NETDEV_TX_OK;
	}

	ret = sx1276_read_single(spi, REG_OPMODE, &val);
	if (ret) {
		netdev_err(netdev, "Failed to read RegOpMode (%d)", ret);
		return ret;
	}

	val &= REG_OPMODE_MODE_MASK;
	val |= REG_OPMODE_MODE_TX;
	ret = sx1276_write_single(spi, REG_OPMODE, val);
	if (ret) {
		netdev_err(netdev, "Failed to write RegOpMode (%d)", ret);
		return ret;
	}

	return NETDEV_TX_OK;
}

static int sx1276_loradev_open(struct net_device *netdev)
{
	struct spi_device *spi = to_spi_device(netdev->dev.parent);
	u8 val;
	int ret;

	netdev_dbg(netdev, "%s", __func__);

	ret = sx1276_read_single(spi, REG_OPMODE, &val);
	if (ret) {
		netdev_err(netdev, "Failed to read RegOpMode (%d)", ret);
		return ret;
	}

	val &= REG_OPMODE_MODE_MASK;
	val |= REG_OPMODE_MODE_STDBY;
	ret = sx1276_write_single(spi, REG_OPMODE, val);
	if (ret) {
		netdev_err(netdev, "Failed to write RegOpMode (%d)", ret);
		return ret;
	}

	ret = open_loradev(netdev);
	if (ret)
		return ret;

	netif_start_queue(netdev);

	return 0;
}

static int sx1276_loradev_stop(struct net_device *netdev)
{
	struct spi_device *spi = to_spi_device(netdev->dev.parent);
	u8 val;
	int ret;

	netdev_dbg(netdev, "%s", __func__);

	netif_stop_queue(netdev);
	close_loradev(netdev);

	ret = sx1276_read_single(spi, REG_OPMODE, &val);
	if (ret) {
		netdev_warn(netdev, "Failed to read RegOpMode (%d)", ret);
		return ret;
	}

	val &= REG_OPMODE_MODE_MASK;
	val |= REG_OPMODE_MODE_SLEEP;
	ret = sx1276_write_single(spi, REG_OPMODE, val);
	if (ret) {
		netdev_warn(netdev, "Failed to write RegOpMode (%d)", ret);
		return ret;
	}

	return 0;
}

static const struct net_device_ops sx1276_netdev_ops =  {
	.ndo_open = sx1276_loradev_open,
	.ndo_stop = sx1276_loradev_stop,
	.ndo_start_xmit = sx1276_loradev_start_xmit,
};

static int sx1276_probe(struct spi_device *spi)
{
	struct net_device *netdev;
	int rst, dio[6], ret, model, i;
	u32 freq_xosc, freq_band;
	u8 val;

	rst = of_get_named_gpio(spi->dev.of_node, "reset-gpio", 0);
	if (rst == -ENOENT)
		dev_warn(&spi->dev, "no reset GPIO available, ignoring");

	for (i = 0; i < 6; i++) {
		dio[i] = of_get_named_gpio(spi->dev.of_node, "dio-gpios", i);
		if (dio[i] == -ENOENT)
			dev_dbg(&spi->dev, "DIO%d not available, ignoring", i);
		else {
			ret = gpio_direction_input(dio[i]);
			if (ret)
				dev_err(&spi->dev, "couldn't set DIO%d to input", i);
		}
	}

	if (gpio_is_valid(rst)) {
		gpio_set_value(rst, 1);
		udelay(100);
		gpio_set_value(rst, 0);
		msleep(5);
	}

	spi->bits_per_word = 8;
	spi_setup(spi);

	ret = sx1276_read_single(spi, REG_VERSION, &val);
	if (ret) {
		dev_err(&spi->dev, "version read failed");
		return ret;
	}

	if (val == 0x22)
		model = 1272;
	else {
		if (gpio_is_valid(rst)) {
			gpio_set_value(rst, 0);
			udelay(100);
			gpio_set_value(rst, 1);
			msleep(5);
		}

		ret = sx1276_read_single(spi, REG_VERSION, &val);
		if (ret) {
			dev_err(&spi->dev, "version read failed");
			return ret;
		}

		if (val == 0x12)
			model = 1276;
		else {
			dev_err(&spi->dev, "transceiver not recognized (RegVersion = 0x%02x)", (unsigned)val);
			return -EINVAL;
		}
	}

	ret = of_property_read_u32(spi->dev.of_node, "clock-frequency", &freq_xosc);
	if (ret) {
		dev_err(&spi->dev, "failed reading clock-frequency");
		return ret;
	}

	ret = of_property_read_u32(spi->dev.of_node, "radio-frequency", &freq_band);
	if (ret) {
		dev_err(&spi->dev, "failed reading radio-frequency");
		return ret;
	}

	val = REG_OPMODE_LONG_RANGE_MODE;
	if (freq_band < 525000000)
		val |= REG_OPMODE_LOW_FREQUENCY_MODE_ON;
	ret = sx1276_write_single(spi, REG_OPMODE, val);
	if (ret) {
		dev_err(&spi->dev, "failed writing opmode");
		return ret;
	}

	freq_band = freq_band / freq_xosc * (2 << 19);
	dev_dbg(&spi->dev, "FRF = %u", freq_band);

	ret = sx1276_write_single(spi, REG_FRF_MSB, freq_band >> 16);
	if (!ret)
		ret = sx1276_write_single(spi, REG_FRF_MID, freq_band >> 8);
	if (!ret)
		ret = sx1276_write_single(spi, REG_FRF_LSB, freq_band);
	if (ret) {
		dev_err(&spi->dev, "failed writing frequency (%d)", ret);
		return ret;
	}

	netdev = alloc_loradev(sizeof(struct sx1276_priv));
	if (!netdev)
		return -ENOMEM;

	netdev->netdev_ops = &sx1276_netdev_ops;
	spi_set_drvdata(spi, netdev);
	SET_NETDEV_DEV(netdev, &spi->dev);

	ret = register_loradev(netdev);
	if (ret) {
		free_loradev(netdev);
		return ret;
	}

	dev_info(&spi->dev, "SX1276 module probed (SX%d)", model);

	return 0;
}

static int sx1276_remove(struct spi_device *spi)
{
	struct net_device *netdev = spi_get_drvdata(spi);

	unregister_loradev(netdev);
	free_loradev(netdev);

	dev_info(&spi->dev, "SX1276 module removed");

	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id sx1276_dt_ids[] = {
	{ .compatible = "semtech,sx1272" },
	{ .compatible = "semtech,sx1276" },
	{}
};
MODULE_DEVICE_TABLE(of, sx1276_dt_ids);
#endif

static struct spi_driver sx1276_spi_driver = {
	.driver = {
		.name = "sx1276",
		.of_match_table = of_match_ptr(sx1276_dt_ids),
	},
	.probe = sx1276_probe,
	.remove = sx1276_remove,
};

module_spi_driver(sx1276_spi_driver);

MODULE_DESCRIPTION("SX1276 SPI driver");
MODULE_AUTHOR("Andreas Färber <afaerber@suse.de>");
MODULE_LICENSE("GPL");
