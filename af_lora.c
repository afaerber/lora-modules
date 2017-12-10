// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (c) 2017 Andreas Färber
 */

#include <linux/module.h>
#include <linux/net.h>
#include <linux/socket.h>
#include <net/sock.h>

#include "af_lora.h"

struct dgram_sock {
	struct sock sk;
	int ifindex;
	bool bound;
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

static int dgram_getname(struct socket *sock, struct sockaddr *uaddr, int *len, int peer)
{
	struct sockaddr_lora *addr = (struct sockaddr_lora *)uaddr;
	struct sock *sk = sock->sk;
	struct dgram_sock *dgram = dgram_sk(sk);

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

	if (!sk)
		return 0;

	dgram = dgram_sk(sk);

	lock_sock(sk);

	dgram->ifindex = 0;
	dgram->bound = false;

	sock_orphan(sk);
	sock->sk = NULL;

	release_sock(sk);
	sock_put(sk);

	return 0;
}

static const struct proto_ops dgram_proto_ops = {
	.family		= PF_LORA,
	.release	= dgram_release,
	.bind		= dgram_bind,
	.connect	= sock_no_connect,
	.socketpair	= sock_no_socketpair,
	.accept		= sock_no_accept,
	.getname	= dgram_getname,
	.poll		= datagram_poll,
	.ioctl		= sock_no_ioctl,
	.listen		= sock_no_listen,
	.shutdown	= sock_no_shutdown,
	.setsockopt	= sock_no_setsockopt,
	.getsockopt	= sock_no_getsockopt,
	.sendmsg	= sock_no_sendmsg,
	.recvmsg	= sock_no_recvmsg,
	.mmap		= sock_no_mmap,
	.sendpage	= sock_no_sendpage,
};

static int dgram_init(struct sock *sk)
{
	struct dgram_sock *dgram = dgram_sk(sk);

	dgram->bound = false;
	dgram->ifindex = 0;

	return 0;
}

static struct proto dgram_proto __read_mostly = {
	.name = "LoRa",
	.owner = THIS_MODULE,
	.obj_size = sizeof(struct dgram_sock),
	.init = dgram_init,
};

static void lora_sock_destruct(struct sock *sk)
{
	skb_queue_purge(&sk->sk_receive_queue);
}

static int lora_create(struct net *net, struct socket *sock, int protocol,
		       int kern)
{
	struct sock *sk;

	sock->state = SS_UNCONNECTED;

	if (protocol < 0 || protocol >= LORA_NPROTO)
		return -EINVAL;

	if (!net_eq(net, &init_net))
		return -EAFNOSUPPORT;

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

	pr_info("lora: init");

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
	pr_info("lora: exit");

	sock_unregister(PF_LORA);
	proto_unregister(&dgram_proto);
}

module_init(lora_init);
module_exit(lora_exit);

MODULE_DESCRIPTION("LoRa PF_LORA core");
MODULE_AUTHOR("Andreas Färber <afaerber@suse.de>");
MODULE_LICENSE("GPL");
MODULE_ALIAS_NETPROTO(PF_LORA);
