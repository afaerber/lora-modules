/*
 * Microchip RN2483
 *
 * Copyright (c) 2017 Andreas Färber
 *
 * SPDX-License-Identifier: GPL-2.0+
 */

#include <linux/delay.h>
#include <linux/gpio/consumer.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/serdev.h>

struct rn2483_device {
	struct serdev_device *serdev;
	struct gpio_desc *reset_gpio;
	bool saw_cr;
	void *buf;
	size_t buflen;
	struct completion line_comp;
};

static void rn2483_receive_line(struct serdev_device *serdev, const char *sz, size_t len)
{
	struct rn2483_device *rndev = serdev_device_get_drvdata(serdev);

	dev_info(&serdev->dev, "Received line '%s' (%d)", sz, (int)len);
	complete(&rndev->line_comp);
}

static int rn2483_receive_buf(struct serdev_device *serdev, const u8 *data, size_t count)
{
	struct rn2483_device *rndev = serdev_device_get_drvdata(serdev);
	size_t i;

	dev_info(&serdev->dev, "Receive (%d)", (int)count);
	if (!rndev->buf) {
		rndev->buf = devm_kmalloc(&serdev->dev, count, GFP_KERNEL);
		if (!rndev->buf)
			return 0;
		rndev->buflen = 0;
	} else {
		void *tmp = devm_kmalloc(&serdev->dev, rndev->buflen + count, GFP_KERNEL);
		if (!tmp)
			return 0;
		memcpy(tmp, rndev->buf, rndev->buflen);
		devm_kfree(&serdev->dev, rndev->buf);
		rndev->buf = tmp;
	}

	for (i = 0; i < count; i++) {
		if (data[i] == '\r') {
			dev_info(&serdev->dev, "CR @ %d", (int)i);
			rndev->saw_cr = true;
		} else if (data[i] == '\n' && rndev->saw_cr) {
			dev_info(&serdev->dev, "LF @ %d", (int)i);
			if (i > 1)
				memcpy(rndev->buf + rndev->buflen, data, i - 1);
			((char *)rndev->buf)[rndev->buflen + i - 1] = 0;
			rn2483_receive_line(serdev, rndev->buf, rndev->buflen + i - 1);
			rndev->saw_cr = false;
			devm_kfree(&serdev->dev, rndev->buf);
			rndev->buf = NULL;
			rndev->buflen = 0;
			return i + 1;
		} else
			rndev->saw_cr = false;
	}

	memcpy(rndev->buf + rndev->buflen, data, count);
	rndev->buflen += count;
	return count;
}

static const struct serdev_device_ops rn2483_serdev_client_ops = {
	.receive_buf = rn2483_receive_buf,
};

static int rn2483_probe(struct serdev_device *sdev)
{
	struct rn2483_device *rndev;
	unsigned long timeout;
	int ret;

	dev_info(&sdev->dev, "Probed");

	rndev = devm_kzalloc(&sdev->dev, sizeof(struct rn2483_device), GFP_KERNEL);
	if (!rndev)
		return -ENOMEM;

	rndev->serdev = sdev;
	init_completion(&rndev->line_comp);
	serdev_device_set_drvdata(sdev, rndev);

	rndev->reset_gpio = devm_gpiod_get_optional(&sdev->dev, "reset", GPIOD_OUT_LOW);
	if (IS_ERR(rndev->reset_gpio))
		return PTR_ERR(rndev->reset_gpio);

	ret = serdev_device_open(sdev);
	if (ret) {
		dev_err(&sdev->dev, "Failed to open (%d)", ret);
		return ret;
	}
	else dev_info(&sdev->dev, "Opened");

	if (!sdev->ctrl) {
		dev_err(&sdev->dev, "!ctrl");
		return -EINVAL;
	}

	serdev_device_set_baudrate(sdev, 57600);
	serdev_device_set_flow_control(sdev, false);

	dev_info(&sdev->dev, "Enforcing reset");
	gpiod_set_value_cansleep(rndev->reset_gpio, 0);
	msleep(5);
	serdev_device_set_client_ops(sdev, &rn2483_serdev_client_ops);
	dev_info(&sdev->dev, "Leaving reset");
	reinit_completion(&rndev->line_comp);
	gpiod_set_value_cansleep(rndev->reset_gpio, 1);
	msleep(100);

	timeout = wait_for_completion_timeout(&rndev->line_comp, HZ);
	if (!timeout) {
		dev_err(&sdev->dev, "Timeout");
		ret = -ETIMEDOUT;
		goto err_timeout;
	}

	dev_info(&sdev->dev, "Done");

	return 0;
err_timeout:
	gpiod_set_value_cansleep(rndev->reset_gpio, 0);
	return ret;
}

static void rn2483_remove(struct serdev_device *sdev)
{
	struct rn2483_device *rndev = serdev_device_get_drvdata(sdev);

	gpiod_set_value_cansleep(rndev->reset_gpio, 0);

	serdev_device_close(sdev);

	dev_info(&sdev->dev, "Removed");
}

static const struct of_device_id rn2483_of_match[] = {
	{ .compatible = "microchip,rn2483" },
	{}
};
MODULE_DEVICE_TABLE(of, rn2483_of_match);

static struct serdev_device_driver rn2483_serdev_driver = {
	.probe = rn2483_probe,
	.remove = rn2483_remove,
	.driver = {
		.name = "rn2483",
		.of_match_table = rn2483_of_match,
	},
};

static int __init rn2483_init(void)
{
	int ret;

	ret = serdev_device_driver_register(&rn2483_serdev_driver);
	if (ret)
		return ret;

	return 0;
}

static void __exit rn2483_exit(void)
{
	serdev_device_driver_unregister(&rn2483_serdev_driver);
}

module_init(rn2483_init);
module_exit(rn2483_exit);

MODULE_DESCRIPTION("RN2483 serdev driver");
MODULE_AUTHOR("Andreas Färber <afaerber@suse.de>");
MODULE_LICENSE("GPL");
