// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (c) 2017 Andreas Färber
 */

#include <linux/module.h>
#include <linux/net.h>
#include <linux/socket.h>
#include <net/sock.h>

#include "af_lora.h"
#include "lora.h"

int lora_send(struct sk_buff *skb)
{
	int ret;

	pr_debug("lora: %s\n", __func__);

	skb->protocol = htons(ETH_P_LORA);

	if (unlikely(skb->len > skb->dev->mtu)) {
		ret = -EMSGSIZE;
		goto err_msg;
	}

	if (unlikely(skb->dev->type != ARPHRD_LORA)) {
		ret = -EPERM;
		goto err_msg;
	}

	skb->ip_summed = CHECKSUM_UNNECESSARY;

	skb_reset_mac_header(skb);
	skb_reset_network_header(skb);
	skb_reset_transport_header(skb);

	if (false) {
		skb->pkt_type = PACKET_LOOPBACK;
	} else
		skb->pkt_type = PACKET_HOST;

	ret = dev_queue_xmit(skb);
	if (ret > 0)
		ret = net_xmit_errno(ret);
	if (ret)
		return ret;

	return 0;

err_msg:
	kfree_skb(skb);
	return ret;
}
EXPORT_SYMBOL(lora_send);

static void lora_sock_destruct(struct sock *sk)
{
	pr_debug("lora: %s\n", __func__);

	skb_queue_purge(&sk->sk_receive_queue);
}

static int lora_create(struct net *net, struct socket *sock, int protocol,
		       int kern)
{
	struct sock *sk;

	pr_debug("lora: %s\n", __func__);

	sock->state = SS_UNCONNECTED;

	if (protocol < 0 || protocol > LORA_NPROTO)
		return -EINVAL;

	/*if (!net_eq(net, &init_net))
		return -EAFNOSUPPORT;*/

	if (sock->type != SOCK_DGRAM)
		return -ESOCKTNOSUPPORT;

	sock->ops = &dgram_proto_ops;

	sk = sk_alloc(net, PF_LORA, GFP_KERNEL, &dgram_proto, kern);
	if (!sk)
		return -ENOMEM;

	sock_init_data(sock, sk);
	sk->sk_family = PF_LORA;
	sk->sk_destruct = lora_sock_destruct;

	if (sk->sk_prot->init) {
		int ret = sk->sk_prot->init(sk);
		if (ret) {
			sock_orphan(sk);
			sock_put(sk);
			return ret;
		}
	}

	return 0;
}

static const struct net_proto_family lora_net_proto_family = {
	.family = PF_LORA,
	.owner = THIS_MODULE,
	.create = lora_create,
};

static __init int lora_init(void)
{
	int ret;

	pr_debug("lora: init\n");

	ret = proto_register(&dgram_proto, 1);
	if (ret)
		goto err_dgram;

	ret = sock_register(&lora_net_proto_family);
	if (ret)
		goto err_sock;

	return 0;

err_sock:
	proto_unregister(&dgram_proto);
err_dgram:
	return ret;
}

static __exit void lora_exit(void)
{
	pr_debug("lora: exit\n");

	sock_unregister(PF_LORA);
	proto_unregister(&dgram_proto);
}

module_init(lora_init);
module_exit(lora_exit);

MODULE_DESCRIPTION("LoRa PF_LORA core");
MODULE_AUTHOR("Andreas Färber <afaerber@suse.de>");
MODULE_LICENSE("GPL");
MODULE_ALIAS_NETPROTO(PF_LORA);
