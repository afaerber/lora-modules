/*
 * Copyright (c) 2017 Andreas Färber
 *
 * SPDX-License-Identifier: GPL-2.0+
 */

#include <linux/module.h>
#include <linux/netdevice.h>
#include <net/rtnetlink.h>

#include "af_lora.h"
#include "lora.h"

static void lora_setup(struct net_device *dev)
{
	dev->type = ARPHRD_LORA;
	dev->mtu = LORA_MTU;
	dev->hard_header_len = 0;
	dev->addr_len = 0;
	dev->tx_queue_len = 0;

	dev->flags = IFF_NOARP;
	dev->features = 0;
}

struct net_device *alloc_loradev(int sizeof_priv)
{
	struct net_device *dev;
	struct lora_priv *priv;

	dev = alloc_netdev(sizeof_priv, "lora%d", NET_NAME_UNKNOWN, lora_setup);
	if (!dev)
		return NULL;

	priv = netdev_priv(dev);
	priv->dev = dev;

	return dev;
}
EXPORT_SYMBOL_GPL(alloc_loradev);

void free_loradev(struct net_device *dev)
{
	free_netdev(dev);
}
EXPORT_SYMBOL_GPL(free_loradev);

static struct rtnl_link_ops lora_link_ops __read_mostly = {
	.kind = "lora",
	.setup = lora_setup,
};

int register_loradev(struct net_device *dev)
{
	dev->rtnl_link_ops = &lora_link_ops;
	return register_netdev(dev);
}
EXPORT_SYMBOL_GPL(register_loradev);

void unregister_loradev(struct net_device *dev)
{
	unregister_netdev(dev);
}
EXPORT_SYMBOL_GPL(unregister_loradev);

static int __init lora_dev_init(void)
{
	printk("lora-dev: init\n");

	return rtnl_link_register(&lora_link_ops);
}

static void __exit lora_dev_exit(void)
{
	printk("lora-dev: exit\n");

	rtnl_link_unregister(&lora_link_ops);
}

module_init(lora_dev_init);
module_exit(lora_dev_exit);

MODULE_DESCRIPTION("LoRa device driver interface");
MODULE_ALIAS_RTNL_LINK("lora");
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Andreas Färber");
