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

struct dgram_sock {
	struct sock sk;
	int ifindex;
	bool bound;
	struct notifier_block notifier;
};

static inline struct dgram_sock *dgram_sk(const struct sock *sk)
{
	return (struct dgram_sock *)sk;
}

static int dgram_bind(struct socket *sock, struct sockaddr *uaddr, int len)
{
	struct sockaddr_lora *addr = (struct sockaddr_lora *)uaddr;
	struct sock *sk = sock->sk;
	struct dgram_sock *dgram = dgram_sk(sk);
	int ifindex;
	int ret = 0;
	bool notify_enetdown = false;

	pr_debug("lora: %s\n", __func__);

	if (len < sizeof(*addr))
		return -EINVAL;

	lock_sock(sk);

	if (dgram->bound && addr->lora_ifindex == dgram->ifindex)
		goto out;

	if (addr->lora_ifindex) {
		struct net_device *netdev;

		netdev = dev_get_by_index(sock_net(sk), addr->lora_ifindex);
		if (!netdev) {
			ret = -ENODEV;
			goto out;
		}
		if (netdev->type != ARPHRD_LORA) {
			dev_put(netdev);
			ret = -ENODEV;
			goto out;
		}
		if (!(netdev->flags & IFF_UP))
			notify_enetdown = true;

		ifindex = netdev->ifindex;

		dev_put(netdev);
	} else
		ifindex = 0;

	dgram->ifindex = ifindex;
	dgram->bound = true;

out:
	release_sock(sk);

	if (notify_enetdown) {
		sk->sk_err = ENETDOWN;
		if (!sock_flag(sk, SOCK_DEAD))
			sk->sk_error_report(sk);
	}

	return ret;
}

static int dgram_sendmsg(struct socket *sock, struct msghdr *msg, size_t size)
{
	struct sock *sk = sock->sk;
	struct dgram_sock *dgram = dgram_sk(sk);
	struct sk_buff *skb;
	struct net_device *netdev;
	int ifindex;
	int ret;

	pr_debug("lora: %s\n", __func__);

	if (msg->msg_name) {
		DECLARE_SOCKADDR(struct sockaddr_lora *, addr, msg->msg_name);

		if (msg->msg_namelen < sizeof(*addr))
			return -EINVAL;

		if (addr->lora_family != AF_LORA)
			return -EINVAL;

		ifindex = addr->lora_ifindex;
	} else
		ifindex = dgram->ifindex;

	netdev = dev_get_by_index(sock_net(sk), ifindex);
	if (!netdev)
		return -ENXIO;

	skb = sock_alloc_send_skb(sk, size + sizeof(struct lora_skb_priv),
				  msg->msg_flags & MSG_DONTWAIT, &ret);
	if (!skb)
		goto err_sock_alloc_send_skb;

	lora_skb_reserve(skb);
	lora_skb_prv(skb)->ifindex = netdev->ifindex;

	ret = memcpy_from_msg(skb_put(skb, size), msg, size);
	if (ret < 0)
		goto err_memcpy_from_msg;

	sock_tx_timestamp(sk,
#if defined(CONFIG_ARM) || defined(CONFIG_ARM64)
			  sk->sk_tsflags,
#endif
			  &skb_shinfo(skb)->tx_flags);

	skb->dev = netdev;
	skb->sk = sk;
	skb->priority = sk->sk_priority;

	ret = lora_send(skb);

	dev_put(netdev);

	if (ret)
		return ret;

	return size;

err_memcpy_from_msg:
	kfree_skb(skb);
err_sock_alloc_send_skb:
	dev_put(netdev);
	return ret;
}

static int dgram_ioctl(struct socket *sock, unsigned int cmd, unsigned long arg)
{
	pr_debug("lora: %s\n", __func__);

	return -ENOIOCTLCMD;
}

static int dgram_getname(struct socket *sock, struct sockaddr *uaddr, int *len, int peer)
{
	struct sockaddr_lora *addr = (struct sockaddr_lora *)uaddr;
	struct sock *sk = sock->sk;
	struct dgram_sock *dgram = dgram_sk(sk);

	pr_debug("lora: %s\n", __func__);

	if (peer)
		return -EOPNOTSUPP;

	memset(addr, 0, sizeof(*addr));
	addr->lora_family = AF_LORA;
	addr->lora_ifindex = dgram->ifindex;

	*len = sizeof(*addr);

	return 0;
}

static int dgram_release(struct socket *sock)
{
	struct sock *sk = sock->sk;
	struct dgram_sock *dgram;

	pr_debug("lora: %s\n", __func__);

	if (!sk)
		return 0;

	dgram = dgram_sk(sk);

	unregister_netdevice_notifier(&dgram->notifier);

	lock_sock(sk);

	dgram->ifindex = 0;
	dgram->bound = false;

	sock_orphan(sk);
	sock->sk = NULL;

	release_sock(sk);
	sock_put(sk);

	return 0;
}

const struct proto_ops dgram_proto_ops = {
	.family		= PF_LORA,
	.release	= dgram_release,
	.bind		= dgram_bind,
	.connect	= sock_no_connect,
	.socketpair	= sock_no_socketpair,
	.accept		= sock_no_accept,
	.getname	= dgram_getname,
	.poll		= datagram_poll,
	.ioctl		= dgram_ioctl,
	.listen		= sock_no_listen,
	.shutdown	= sock_no_shutdown,
	.setsockopt	= sock_no_setsockopt,
	.getsockopt	= sock_no_getsockopt,
	.sendmsg	= dgram_sendmsg,
	.recvmsg	= sock_no_recvmsg,
	.mmap		= sock_no_mmap,
	.sendpage	= sock_no_sendpage,
};

static int dgram_notifier(struct notifier_block *nb, unsigned long msg, void *ptr)
{
	struct net_device *netdev = netdev_notifier_info_to_dev(ptr);
	struct dgram_sock *dgram = container_of(nb, struct dgram_sock, notifier);
	struct sock *sk = &dgram->sk;

	pr_debug("lora: %s\n", __func__);

	if (!net_eq(dev_net(netdev), sock_net(sk)))
		return NOTIFY_DONE;

	if (netdev->type != ARPHRD_LORA)
		return NOTIFY_DONE;

	if (dgram->ifindex != netdev->ifindex)
		return NOTIFY_DONE;

	switch (msg) {
	case NETDEV_UNREGISTER:
		lock_sock(sk);

		dgram->ifindex = 0;
		dgram->bound = false;

		release_sock(sk);

		sk->sk_err = ENODEV;
		if (!sock_flag(sk, SOCK_DEAD))
			sk->sk_error_report(sk);
		break;

	case NETDEV_DOWN:
		sk->sk_err = ENETDOWN;
		if (!sock_flag(sk, SOCK_DEAD))
			sk->sk_error_report(sk);
		break;
	}

	return NOTIFY_DONE;
}

static int dgram_init(struct sock *sk)
{
	struct dgram_sock *dgram = dgram_sk(sk);

	pr_debug("lora: %s\n", __func__);

	dgram->bound = false;
	dgram->ifindex = 0;

	dgram->notifier.notifier_call = dgram_notifier;
	register_netdevice_notifier(&dgram->notifier);

	return 0;
}

struct proto dgram_proto __read_mostly = {
	.name = "LoRa",
	.owner = THIS_MODULE,
	.obj_size = sizeof(struct dgram_sock),
	.init = dgram_init,
};
