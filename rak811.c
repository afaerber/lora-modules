// SPDX-License-Identifier: GPL-2.0+
/*
 * RAK Wireless RAK811
 *
 * Copyright (c) 2017-2018 Andreas Färber
 */

#include <linux/module.h>
#include <linux/netdevice.h>
#include <linux/of.h>
#include <linux/serdev.h>

#include "af_lora.h"
#include "lora.h"

struct rak811_device {
	struct serdev_device *serdev;
};

static int rak811_receive_buf(struct serdev_device *sdev, const u8 *data, size_t count)
{
	//struct rak811_device *rakdev = serdev_device_get_drvdata(sdev);
	size_t i = 0;

	dev_dbg(&sdev->dev, "Receive (%d)\n", (int)count);

	for (i = 0; i < count; i++) {
		dev_dbg(&sdev->dev, "Receive: 0x%02x\n", (int)data[i]);
	}

	return 0;
}

static const struct serdev_device_ops rak811_serdev_client_ops = {
	.receive_buf = rak811_receive_buf,
};

static int rak811_probe(struct serdev_device *sdev)
{
	struct rak811_device *rakdev;
	int ret;

	dev_info(&sdev->dev, "Probing");

	rakdev = devm_kzalloc(&sdev->dev, sizeof(struct rak811_device), GFP_KERNEL);
	if (!rakdev)
		return -ENOMEM;

	rakdev->serdev = sdev;
	serdev_device_set_drvdata(sdev, rakdev);

	ret = serdev_device_open(sdev);
	if (ret) {
		dev_err(&sdev->dev, "Failed to open (%d)", ret);
		return ret;
	}

	serdev_device_set_baudrate(sdev, 115200);
	serdev_device_set_flow_control(sdev, false);
	serdev_device_set_client_ops(sdev, &rak811_serdev_client_ops);

	dev_info(&sdev->dev, "Done.");

	return 0;
}

static void rak811_remove(struct serdev_device *sdev)
{
	serdev_device_close(sdev);

	dev_info(&sdev->dev, "Removed\n");
}

static const struct of_device_id rak811_of_match[] = {
	{ .compatible = "rakwireless,rak811" },
	{}
};
MODULE_DEVICE_TABLE(of, rak811_of_match);

static struct serdev_device_driver rak811_serdev_driver = {
	.probe = rak811_probe,
	.remove = rak811_remove,
	.driver = {
		.name = "rak811",
		.of_match_table = rak811_of_match,
	},
};

static int __init rak811_init(void)
{
	int ret;

	ret = serdev_device_driver_register(&rak811_serdev_driver);
	if (ret)
		return ret;

	return 0;
}

static void __exit rak811_exit(void)
{
	serdev_device_driver_unregister(&rak811_serdev_driver);
}

module_init(rak811_init);
module_exit(rak811_exit);

MODULE_DESCRIPTION("RAK811 serdev driver");
MODULE_AUTHOR("Andreas Färber <afaerber@suse.de>");
MODULE_LICENSE("GPL");
