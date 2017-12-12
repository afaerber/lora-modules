// SPDX-License-Identifier: GPL-2.0+
/*
 * Semtech SX1276 LoRa transceiver
 *
 * Copyright (c) 2016-2017 Andreas Färber
 */

#include <linux/debugfs.h>
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
#define REG_PA_CONFIG			0x09
#define LORA_REG_FIFO_ADDR_PTR		0x0d
#define LORA_REG_FIFO_TX_BASE_ADDR	0x0e
#define LORA_REG_IRQ_FLAGS_MASK		0x11
#define LORA_REG_IRQ_FLAGS		0x12
#define LORA_REG_PAYLOAD_LENGTH		0x22
#define LORA_REG_SYNC_WORD		0x39
#define REG_DIO_MAPPING1		0x40
#define REG_DIO_MAPPING2		0x41
#define REG_VERSION			0x42
#define REG_PA_DAC			0x4d

#define REG_OPMODE_LONG_RANGE_MODE		BIT(7)
#define REG_OPMODE_LOW_FREQUENCY_MODE_ON	BIT(3)
#define REG_OPMODE_MODE_MASK			GENMASK(2, 0)
#define REG_OPMODE_MODE_SLEEP			(0x0 << 0)
#define REG_OPMODE_MODE_STDBY			(0x1 << 0)
#define REG_OPMODE_MODE_TX			(0x3 << 0)
#define REG_OPMODE_MODE_RXCONTINUOUS		(0x5 << 0)
#define REG_OPMODE_MODE_RXSINGLE		(0x6 << 0)

#define REG_PA_CONFIG_PA_SELECT			BIT(7)

#define LORA_REG_IRQ_FLAGS_TX_DONE		BIT(3)

#define REG_DIO_MAPPING1_DIO0_MASK	GENMASK(7, 6)

struct sx1276_priv {
	struct lora_priv lora;
	struct spi_device *spi;

	size_t fifosize;
	int dio_gpio[6];

	struct mutex spi_lock;

	struct sk_buff *tx_skb;
	int tx_len;

	struct workqueue_struct *wq;
	struct work_struct tx_work;

	struct dentry *debugfs;
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
	struct sx1276_priv *priv = netdev_priv(netdev);

	netdev_dbg(netdev, "%s\n", __func__);

	if (priv->tx_skb || priv->tx_len) {
		netdev_warn(netdev, "TX busy\n");
		return NETDEV_TX_BUSY;
	}

	if (skb->protocol != htons(ETH_P_LORA)) {
		kfree_skb(skb);
		netdev->stats.tx_dropped++;
		return NETDEV_TX_OK;
	}

	netif_stop_queue(netdev);
	priv->tx_skb = skb;
	queue_work(priv->wq, &priv->tx_work);

	return NETDEV_TX_OK;
}

static int sx1276_tx(struct spi_device *spi, void *data, int data_len)
{
	u8 addr, val;
	int ret;

	dev_dbg(&spi->dev, "%s\n", __func__);

	ret = sx1276_read_single(spi, REG_OPMODE, &val);
	if (ret) {
		dev_err(&spi->dev, "Failed to read RegOpMode (%d)\n", ret);
		return ret;
	}
	dev_dbg(&spi->dev, "RegOpMode = 0x%02x\n", val);
	if (!(val & REG_OPMODE_LONG_RANGE_MODE))
		dev_err(&spi->dev, "LongRange Mode not active!\n");
	if ((val & REG_OPMODE_MODE_MASK) == REG_OPMODE_MODE_SLEEP)
		dev_err(&spi->dev, "Cannot access FIFO in Sleep Mode!\n");

	ret = sx1276_read_single(spi, LORA_REG_FIFO_TX_BASE_ADDR, &addr);
	if (ret) {
		dev_err(&spi->dev, "Failed to read RegFifoTxBaseAddr (%d)\n", ret);
		return ret;
	}
	dev_dbg(&spi->dev, "RegFifoTxBaseAddr = 0x%02x\n", addr);

	ret = sx1276_write_single(spi, LORA_REG_FIFO_ADDR_PTR, addr);
	if (ret) {
		dev_err(&spi->dev, "Failed to write RegFifoAddrPtr (%d)\n", ret);
		return ret;
	}

	ret = sx1276_write_single(spi, LORA_REG_PAYLOAD_LENGTH, data_len);
	if (ret) {
		dev_err(&spi->dev, "Failed to write RegPayloadLength (%d)\n", ret);
		return ret;
	}

	ret = sx1276_write_fifo(spi, data_len, data);
	if (ret) {
		dev_err(&spi->dev, "Failed to write into FIFO (%d)\n", ret);
		return ret;
	}

	ret = sx1276_read_single(spi, LORA_REG_IRQ_FLAGS_MASK, &val);
	if (ret) {
		dev_err(&spi->dev, "Failed to read RegIrqFlagsMask (%d)\n", ret);
		return ret;
	}
	dev_dbg(&spi->dev, "RegIrqFlagsMask = 0x%02x\n", val);

	ret = sx1276_read_single(spi, LORA_REG_IRQ_FLAGS, &val);
	if (ret) {
		dev_err(&spi->dev, "Failed to read RegIrqFlags (%d)\n", ret);
		return ret;
	}
	dev_dbg(&spi->dev, "RegIrqFlags = 0x%02x\n", val);

	ret = sx1276_read_single(spi, REG_DIO_MAPPING1, &val);
	if (ret) {
		dev_err(&spi->dev, "Failed to read RegDioMapping1 (%d)\n", ret);
		return ret;
	}

	val &= ~REG_DIO_MAPPING1_DIO0_MASK;
	val |= 0x1 << 6;
	ret = sx1276_write_single(spi, REG_DIO_MAPPING1, val);
	if (ret) {
		dev_err(&spi->dev, "Failed to write RegDioMapping1 (%d)\n", ret);
		return ret;
	}

	ret = sx1276_read_single(spi, REG_OPMODE, &val);
	if (ret) {
		dev_err(&spi->dev, "Failed to read RegOpMode (%d)\n", ret);
		return ret;
	}

	val &= ~REG_OPMODE_MODE_MASK;
	val |= REG_OPMODE_MODE_TX;
	ret = sx1276_write_single(spi, REG_OPMODE, val);
	if (ret) {
		dev_err(&spi->dev, "Failed to write RegOpMode (%d)\n", ret);
		return ret;
	}

	dev_dbg(&spi->dev, "%s: done\n", __func__);

	return 0;
}

static void sx1276_tx_work_handler(struct work_struct *ws)
{
	struct sx1276_priv *priv = container_of(ws, struct sx1276_priv, tx_work);
	struct spi_device *spi = priv->spi;
	struct net_device *netdev = spi_get_drvdata(spi);

	netdev_dbg(netdev, "%s\n", __func__);

	mutex_lock(&priv->spi_lock);

	if (priv->tx_skb) {
		sx1276_tx(spi, priv->tx_skb->data, priv->tx_skb->data_len);
		priv->tx_len = 1 + priv->tx_skb->data_len;
		if (!(netdev->flags & IFF_ECHO) ||
			priv->tx_skb->pkt_type != PACKET_LOOPBACK ||
			priv->tx_skb->protocol != htons(ETH_P_LORA))
			kfree_skb(priv->tx_skb);
		priv->tx_skb = NULL;
	}

	mutex_unlock(&priv->spi_lock);
}

static irqreturn_t sx1276_dio_interrupt(int irq, void *dev_id)
{
	struct net_device *netdev = dev_id;
	struct sx1276_priv *priv = netdev_priv(netdev);
	struct spi_device *spi = priv->spi;
	u8 val;
	int ret;

	netdev_dbg(netdev, "%s\n", __func__);

	mutex_lock(&priv->spi_lock);

	ret = sx1276_read_single(spi, LORA_REG_IRQ_FLAGS, &val);
	if (ret) {
		netdev_warn(netdev, "Failed to read RegIrqFlags (%d)\n", ret);
		val = 0;
	}

	if (val & LORA_REG_IRQ_FLAGS_TX_DONE) {
		netdev_info(netdev, "TX done.\n");
		netdev->stats.tx_packets++;
		netdev->stats.tx_bytes += priv->tx_len - 1;
		priv->tx_len = 0;
		netif_wake_queue(netdev);

		ret = sx1276_write_single(spi, LORA_REG_IRQ_FLAGS, LORA_REG_IRQ_FLAGS_TX_DONE);
		if (ret)
			netdev_warn(netdev, "Failed to write RegIrqFlags (%d)\n", ret);
	}

	mutex_unlock(&priv->spi_lock);

	return IRQ_HANDLED;
}

static int sx1276_loradev_open(struct net_device *netdev)
{
	struct sx1276_priv *priv = netdev_priv(netdev);
	struct spi_device *spi = to_spi_device(netdev->dev.parent);
	u8 val;
	int ret, irq;

	netdev_dbg(netdev, "%s\n", __func__);

	ret = open_loradev(netdev);
	if (ret)
		return ret;

	mutex_lock(&priv->spi_lock);

	ret = sx1276_read_single(spi, REG_OPMODE, &val);
	if (ret) {
		netdev_err(netdev, "Failed to read RegOpMode (%d)\n", ret);
		goto err_opmode;
	}

	val &= ~REG_OPMODE_MODE_MASK;
	val |= REG_OPMODE_MODE_STDBY;
	ret = sx1276_write_single(spi, REG_OPMODE, val);
	if (ret) {
		netdev_err(netdev, "Failed to write RegOpMode (%d)\n", ret);
		goto err_opmode;
	}

	priv->tx_skb = NULL;
	priv->tx_len = 0;

	priv->wq = alloc_workqueue("sx1276_wq", WQ_FREEZABLE | WQ_MEM_RECLAIM, 0);
	INIT_WORK(&priv->tx_work, sx1276_tx_work_handler);

	if (gpio_is_valid(priv->dio_gpio[0])) {
		irq = gpio_to_irq(priv->dio_gpio[0]);
		if (irq <= 0)
			netdev_warn(netdev, "Failed to obtain interrupt for DIO0 (%d)\n", irq);
		else {
			netdev_info(netdev, "Succeeded in obtaining interrupt for DIO0: %d\n", irq);
			ret = request_threaded_irq(irq, NULL, sx1276_dio_interrupt, IRQF_ONESHOT | IRQF_TRIGGER_FALLING, netdev->name, netdev);
			if (ret) {
				netdev_err(netdev, "Failed to request interrupt for DIO0 (%d)\n", ret);
				goto err_irq;
			}
		}
	}

	netif_wake_queue(netdev);

	mutex_unlock(&priv->spi_lock);

	return 0;

err_irq:
	destroy_workqueue(priv->wq);
	priv->wq = NULL;
err_opmode:
	close_loradev(netdev);
	mutex_unlock(&priv->spi_lock);
	return ret;
}

static int sx1276_loradev_stop(struct net_device *netdev)
{
	struct sx1276_priv *priv = netdev_priv(netdev);
	struct spi_device *spi = to_spi_device(netdev->dev.parent);
	u8 val;
	int ret, irq;

	netdev_dbg(netdev, "%s\n", __func__);

	close_loradev(netdev);

	mutex_lock(&priv->spi_lock);

	ret = sx1276_read_single(spi, REG_OPMODE, &val);
	if (ret) {
		netdev_err(netdev, "Failed to read RegOpMode (%d)\n", ret);
		goto err_opmode;
	}

	val &= ~REG_OPMODE_MODE_MASK;
	val |= REG_OPMODE_MODE_SLEEP;
	ret = sx1276_write_single(spi, REG_OPMODE, val);
	if (ret) {
		netdev_err(netdev, "Failed to write RegOpMode (%d)\n", ret);
		goto err_opmode;
	}

	if (gpio_is_valid(priv->dio_gpio[0])) {
		irq = gpio_to_irq(priv->dio_gpio[0]);
		if (irq > 0) {
			netdev_dbg(netdev, "Freeing IRQ %d\n", irq);
			free_irq(irq, netdev);
		}
	}

	destroy_workqueue(priv->wq);
	priv->wq = NULL;

	if (priv->tx_skb || priv->tx_len)
		netdev->stats.tx_errors++;
	if (priv->tx_skb)
		dev_kfree_skb(priv->tx_skb);
	priv->tx_skb = NULL;
	priv->tx_len = 0;

	mutex_unlock(&priv->spi_lock);

	return 0;

err_opmode:
	mutex_unlock(&priv->spi_lock);
	return ret;
}

static const struct net_device_ops sx1276_netdev_ops =  {
	.ndo_open = sx1276_loradev_open,
	.ndo_stop = sx1276_loradev_stop,
	.ndo_start_xmit = sx1276_loradev_start_xmit,
};

static ssize_t sx1276_freq_read(struct file *file, char __user *user_buf,
				 size_t count, loff_t *ppos)
{
	struct net_device *netdev = file->private_data;
	struct sx1276_priv *priv = netdev_priv(netdev);
	struct spi_device *spi = priv->spi;
	ssize_t size;
	char *buf;
	int len = 0;
	int ret;
	u8 msb, mid, lsb;
	unsigned long freq;

	mutex_lock(&priv->spi_lock);

	ret = sx1276_read_single(spi, REG_FRF_MSB, &msb);
	if (!ret)
		ret = sx1276_read_single(spi, REG_FRF_MID, &mid);
	if (!ret)
		ret = sx1276_read_single(spi, REG_FRF_LSB, &lsb);

	mutex_unlock(&priv->spi_lock);

	if (ret)
		return 0;

	freq = 32000000UL / (2 << 19);
	freq *= ((ulong)msb << 16) | ((ulong)mid << 8) | lsb;

	buf = kasprintf(GFP_KERNEL, "%lu\n", freq);
	if (!buf)
		return 0;

	size = simple_read_from_buffer(user_buf, count, ppos, buf, len);
	kfree(buf);

	return size;
}

static const struct file_operations sx1276_freq_fops = {
	.owner = THIS_MODULE,
	.open = simple_open,
	.read = sx1276_freq_read,
};

static ssize_t sx1276_state_read(struct file *file, char __user *user_buf,
				 size_t count, loff_t *ppos)
{
	struct net_device *netdev = file->private_data;
	struct sx1276_priv *priv = netdev_priv(netdev);
	struct spi_device *spi = priv->spi;
	ssize_t size;
	char *buf;
	int len = 0;
	int ret;
	u8 val;
	bool lora_mode = true;
	const int max_len = 4096;

	buf = kzalloc(max_len, GFP_KERNEL);
	if (!buf)
		return 0;

	mutex_lock(&priv->spi_lock);

	ret = sx1276_read_single(spi, REG_OPMODE, &val);
	if (!ret) {
		len += snprintf(buf + len, max_len - len, "RegOpMode = 0x%02x\n", val);
		lora_mode = (val & REG_OPMODE_LONG_RANGE_MODE) != 0;
	}

	ret = sx1276_read_single(spi, REG_PA_CONFIG, &val);
	if (!ret)
		len += snprintf(buf + len, max_len - len, "RegPaConfig = 0x%02x\n", val);

	if (lora_mode) {
		ret = sx1276_read_single(spi, LORA_REG_IRQ_FLAGS_MASK, &val);
		if (!ret)
			len += snprintf(buf + len, max_len - len, "RegIrqFlagsMask = 0x%02x\n", val);

		ret = sx1276_read_single(spi, LORA_REG_IRQ_FLAGS, &val);
		if (!ret)
			len += snprintf(buf + len, max_len - len, "RegIrqFlags = 0x%02x\n", val);

		ret = sx1276_read_single(spi, LORA_REG_SYNC_WORD, &val);
		if (!ret)
			len += snprintf(buf + len, max_len - len, "RegSyncWord = 0x%02x\n", val);
	}

	ret = sx1276_read_single(spi, REG_PA_DAC, &val);
	if (!ret)
		len += snprintf(buf + len, max_len - len, "RegPaDac = 0x%02x\n", val);

	mutex_unlock(&priv->spi_lock);

	size = simple_read_from_buffer(user_buf, count, ppos, buf, len);
	kfree(buf);

	return size;
}

static const struct file_operations sx1276_state_fops = {
	.owner = THIS_MODULE,
	.open = simple_open,
	.read = sx1276_state_read,
};

static int sx1276_probe(struct spi_device *spi)
{
	struct net_device *netdev;
	struct sx1276_priv *priv;
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

	val = REG_OPMODE_LONG_RANGE_MODE | REG_OPMODE_MODE_SLEEP;
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

	ret = sx1276_read_single(spi, REG_PA_CONFIG, &val);
	if (ret) {
		dev_err(&spi->dev, "failed reading RegPaConfig\n");
		return ret;
	}
	if (true)
		val |= REG_PA_CONFIG_PA_SELECT;
	ret = sx1276_write_single(spi, REG_PA_CONFIG, val);
	if (ret) {
		dev_err(&spi->dev, "failed writing RegPaConfig\n");
		return ret;
	}

	netdev = alloc_loradev(sizeof(struct sx1276_priv));
	if (!netdev)
		return -ENOMEM;

	netdev->netdev_ops = &sx1276_netdev_ops;
	netdev->flags |= IFF_ECHO;

	priv = netdev_priv(netdev);
	priv->spi = spi;
	mutex_init(&priv->spi_lock);
	for (i = 0; i < 6; i++)
		priv->dio_gpio[i] = dio[i];

	spi_set_drvdata(spi, netdev);
	SET_NETDEV_DEV(netdev, &spi->dev);

	ret = register_loradev(netdev);
	if (ret) {
		free_loradev(netdev);
		return ret;
	}

	priv->debugfs = debugfs_create_dir(dev_name(&spi->dev), NULL);
	debugfs_create_file("state", S_IRUGO, priv->debugfs, netdev, &sx1276_state_fops);
	debugfs_create_file("frequency", S_IRUGO, priv->debugfs, netdev, &sx1276_freq_fops);

	dev_info(&spi->dev, "SX1276 module probed (SX%d)", model);

	return 0;
}

static int sx1276_remove(struct spi_device *spi)
{
	struct net_device *netdev = spi_get_drvdata(spi);
	struct sx1276_priv *priv = netdev_priv(netdev);

	debugfs_remove_recursive(priv->debugfs);

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
