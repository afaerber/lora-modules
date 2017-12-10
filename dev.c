// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (c) 2017 Andreas Färber
 */

#include <linux/module.h>
#include <linux/netdevice.h>
#include <net/rtnetlink.h>

#include "af_lora.h"
#include "lora.h"

struct lora_skb_priv {
	int ifindex;
};

static inline struct lora_skb_priv *lora_skb_prv(struct sk_buff *skb)
{
	return (struct lora_skb_priv *)(skb->head);
}

static inline void lora_skb_reserve(struct sk_buff *skb)
{
	skb_reserve(skb, sizeof(struct lora_skb_priv));
}

struct sk_buff *alloc_lora_skb(struct net_device *dev, u8 **data)
{
	struct sk_buff *skb;

	skb = netdev_alloc_skb(dev, sizeof(struct lora_skb_priv));
	if (unlikely(!skb))
		return NULL;

	skb->protocol = htons(ETH_P_LORA);
	skb->pkt_type = PACKET_BROADCAST;
	skb->ip_summed = CHECKSUM_UNNECESSARY;

	skb_reset_mac_header(skb);
	skb_reset_network_header(skb);
	skb_reset_transport_header(skb);

	lora_skb_reserve(skb);
	lora_skb_prv(skb)->ifindex = dev->ifindex;

	return skb;
}
EXPORT_SYMBOL_GPL(alloc_lora_skb);

int open_loradev(struct net_device *dev)
{
	if (!netif_carrier_ok(dev))
		netif_carrier_on(dev);

	return 0;
}
EXPORT_SYMBOL_GPL(open_loradev);

void close_loradev(struct net_device *dev)
{
}
EXPORT_SYMBOL_GPL(close_loradev);

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
