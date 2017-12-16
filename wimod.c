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

#define WIMOD_HCI_PAYLOAD_MAX	300
#define WIMOD_HCI_PACKET_MAX	(1 + (2 + WIMOD_HCI_PAYLOAD_MAX + 2) * 2 + 1)

struct wimod_device {
	struct serdev_device *serdev;

	u8 rx_buf[WIMOD_HCI_PACKET_MAX];
	int rx_len;
	bool rx_esc;
};

#define SLIP_END	0300
#define SLIP_ESC	0333
#define SLIP_ESC_END	0334
#define SLIP_ESC_ESC	0335

static inline void slip_print_bytes(const u8* buf, int len)
{
	int i;

	for (i = 0; i < len; i++)
		printk("%02x ", buf[i]);
}

static int slip_send_end(struct serdev_device *sdev)
{
	u8 val = SLIP_END;

	slip_print_bytes(&val, 1);

	return serdev_device_write_buf(sdev, &val, 1);
}

static int slip_send_data(struct serdev_device *sdev, const u8 *buf, int len)
{
	int last_idx = -1;
	int i;
	u8 esc[2] = { SLIP_ESC, 0 };
	int ret;

	for (i = 0; i < len; i++) {
		if (buf[i] != SLIP_END &&
		    buf[i] != SLIP_ESC)
			continue;

		slip_print_bytes(&buf[last_idx + 1], i - (last_idx + 1));

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

	slip_print_bytes(&buf[last_idx + 1], len - (last_idx + 1));

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
	if (payload_len > 0)
		crc = crc_ccitt(crc, payload, payload_len);
	crc = ~crc;

	printk(KERN_INFO "sending: ");

	ret = slip_send_end(sdev);
	ret = slip_send_data(sdev, &dst_id, 1);
	ret = slip_send_data(sdev, &msg_id, 1);
	if (payload_len > 0)
		ret = slip_send_data(sdev, payload, payload_len);
	cpu_to_le16s(crc);
	ret = slip_send_data(sdev, (u8 *)&crc, 2);
	ret = slip_send_end(sdev);

	printk("\n");

	return ret;
}

static void wimod_process_packet(struct serdev_device *sdev, const u8 *data, int len)
{
	u16 crc;

	dev_info(&sdev->dev, "Processing incoming packet (%d)\n", len);

	if (len < 4) {
		dev_dbg(&sdev->dev, "Discarding packet of length %d\n", len);
		return;
	}

	crc = ~crc_ccitt(0xffff, data, len);
	if (crc != 0x0f47) {
		dev_dbg(&sdev->dev, "Discarding packet with wrong checksum\n");
		return;
	}
}

static int wimod_receive_buf(struct serdev_device *sdev, const u8 *data, size_t count)
{
	struct wimod_device *wmdev = serdev_device_get_drvdata(sdev);
	size_t i = 0;
	int len = 0;

	dev_dbg(&sdev->dev, "Receive (%d)\n", (int)count);

	while (i < min(count, sizeof(wmdev->rx_buf) - wmdev->rx_len)) {
		if (wmdev->rx_esc) {
			wmdev->rx_esc = false;
			switch (data[i]) {
			case SLIP_ESC_END:
				wmdev->rx_buf[wmdev->rx_len++] = SLIP_END;
				break;
			case SLIP_ESC_ESC:
				wmdev->rx_buf[wmdev->rx_len++] = SLIP_ESC;
				break;
			default:
				dev_warn(&sdev->dev, "Ignoring unknown escape sequence 0300 0%o\n", data[i]);
				break;
			}
			len += i + 1;
			data += i + 1;
			count -= i + 1;
			i = 0;
			continue;
		}
		if (data[i] != SLIP_END &&
		    data[i] != SLIP_ESC) {
			i++;
			continue;
		}
		if (i > 0) {
			memcpy(&wmdev->rx_buf[wmdev->rx_len], data, i);
			wmdev->rx_len += i;
		}
		if (data[i] == SLIP_END && wmdev->rx_len > 0) {
			wimod_process_packet(sdev, wmdev->rx_buf, wmdev->rx_len);
			wmdev->rx_len = 0;
		} else if (data[i] == SLIP_ESC) {
			wmdev->rx_esc = true;
		}
		len += i + 1;
		data += i + 1;
		count -= i + 1;
		i = 0;
	}

	return len;
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
