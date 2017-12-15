// SPDX-License-Identifier: GPL-2.0+
/*
 * IMST WiMOD
 *
 * Copyright (c) 2017 Andreas Färber
 */

#include <linux/crc-ccitt.h>
#include <linux/module.h>
#include <linux/netdevice.h>
#include <linux/of.h>
#include <linux/serdev.h>

#include "af_lora.h"
#include "lora.h"

struct wimod_device {
	struct serdev_device *serdev;
};

#define SLIP_END	0300
#define SLIP_ESC	0333
#define SLIP_ESC_END	0334
#define SLIP_ESC_ESC	0335

static int slip_send_end(struct serdev_device *sdev)
{
	u8 val = SLIP_END;

	return serdev_device_write_buf(sdev, &val, 1);
}

static int slip_send_data(struct serdev_device *sdev, const u8 *buf, int len)
{
	int last_idx = -1;
	int i;
	u8 esc[2] = { SLIP_ESC, };
	int ret;

	for (i = 0; i < len; i++) {
		if (buf[i] != SLIP_END && buf[i] != SLIP_ESC)
			continue;

		ret = serdev_device_write_buf(sdev,
			&buf[last_idx + 1], i - (last_idx + 1));

		switch (buf[i]) {
		case SLIP_END:
			esc[1] = SLIP_ESC_END;
			break;
		case SLIP_ESC:
			esc[1] = SLIP_ESC_ESC;
			break;
		}
		ret = serdev_device_write_buf(sdev, esc, 2);

		last_idx = i;
	}

	ret = serdev_device_write_buf(sdev,
		&buf[last_idx + 1], len - (last_idx + 1));

	return ret;
}

#define DEVMGMT_ID	0x01

#define DEVMGMT_MSG_PING_REQ	0x01
#define DEVMGMT_MSG_PING_RSP	0x02

static int wimod_hci_send(struct serdev_device *sdev,
	u8 dst_id, u8 msg_id, u8 *payload, int payload_len)
{
	u16 crc = 0xffff;
	int ret;

	crc = crc_ccitt_byte(crc, dst_id);
	crc = crc_ccitt_byte(crc, msg_id);
	crc = crc_ccitt(crc, payload, payload_len);
	crc = ~crc;

	ret = slip_send_end(sdev);
	ret = slip_send_data(sdev, &dst_id, 1);
	ret = slip_send_data(sdev, &msg_id, 1);
	ret = slip_send_data(sdev, payload, payload_len);
	cpu_to_le16s(crc);
	ret = slip_send_data(sdev, (u8 *)&crc, 2);
	ret = slip_send_end(sdev);

	return ret;
}

static int wimod_receive_buf(struct serdev_device *sdev, const u8 *data, size_t count)
{
	struct wimod_device *wmdev = serdev_device_get_drvdata(sdev);

	dev_dbg(&sdev->dev, "Receive (%d)", (int)count);

	return count;
}

static const struct serdev_device_ops wimod_serdev_client_ops = {
	.receive_buf = wimod_receive_buf,
};

static int wimod_probe(struct serdev_device *sdev)
{
	struct wimod_device *wmdev;
	int ret;

	dev_info(&sdev->dev, "Probing");

	wmdev = devm_kzalloc(&sdev->dev, sizeof(struct wimod_device), GFP_KERNEL);
	if (!wmdev)
		return -ENOMEM;

	wmdev->serdev = sdev;
	serdev_device_set_drvdata(sdev, wmdev);

	ret = serdev_device_open(sdev);
	if (ret) {
		dev_err(&sdev->dev, "Failed to open (%d)", ret);
		return ret;
	}

	serdev_device_set_baudrate(sdev, 115200);
	serdev_device_set_flow_control(sdev, false);
	serdev_device_set_client_ops(sdev, &wimod_serdev_client_ops);

	ret = wimod_hci_send(sdev, DEVMGMT_ID, DEVMGMT_MSG_PING_REQ, NULL, 0);

	dev_info(&sdev->dev, "Done.");

	return 0;
}

static void wimod_remove(struct serdev_device *sdev)
{
	serdev_device_close(sdev);

	dev_info(&sdev->dev, "Removed\n");
}

static const struct of_device_id wimod_of_match[] = {
	{ .compatible = "imst,wimod-hci" },
	{}
};
MODULE_DEVICE_TABLE(of, wimod_of_match);

static struct serdev_device_driver wimod_serdev_driver = {
	.probe = wimod_probe,
	.remove = wimod_remove,
	.driver = {
		.name = "wimod",
		.of_match_table = wimod_of_match,
	},
};

static int __init wimod_init(void)
{
	int ret;

	ret = serdev_device_driver_register(&wimod_serdev_driver);
	if (ret)
		return ret;

	return 0;
}

static void __exit wimod_exit(void)
{
	serdev_device_driver_unregister(&wimod_serdev_driver);
}

module_init(wimod_init);
module_exit(wimod_exit);

MODULE_DESCRIPTION("WiMOD serdev driver");
MODULE_AUTHOR("Andreas Färber <afaerber@suse.de>");
MODULE_LICENSE("GPL");
