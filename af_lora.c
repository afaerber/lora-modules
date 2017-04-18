/*
 * Copyright (c) 2017 Andreas Färber
 *
 * SPDX-License-Identifier: GPL-2.0+
 */

#include <linux/module.h>
#include <linux/net.h>
#include <linux/socket.h>
#include <net/sock.h>

#include "af_lora.h"

struct raw_sock {
	struct sock sk;
	int ifindex;
};

static inline struct raw_sock *raw_sk(const struct sock *sk)
{
	return (struct raw_sock *)sk;
}

static int raw_getname(struct socket *sock, struct sockaddr *uaddr, int *len, int peer)
{
	struct sockaddr_lora *addr = (struct sockaddr_lora *)uaddr;
	struct sock *sk = sock->sk;
	struct raw_sock *raw = raw_sk(sk);

	if (peer)
		return -EOPNOTSUPP;

	memset(addr, 0, sizeof(*addr));
	addr->lora_family = AF_LORA;
	addr->lora_ifindex = raw->ifindex;

	*len = sizeof(*addr);

	return 0;
}

static int raw_release(struct socket *sock)
{
	struct sock *sk = sock->sk;
	struct raw_sock *raw;

	if (!sk)
		return 0;

	raw = raw_sk(sk);
	lock_sock(sk);
	raw->ifindex = 0;
	sock_orphan(sk);
	sock->sk = NULL;
	release_sock(sk);
	sock_put(sk);

	return 0;
}

static const struct proto_ops raw_ops = {
	.family		= PF_LORA,
	.release	= raw_release,
	.bind		= sock_no_bind,
	.connect	= sock_no_connect,
	.socketpair	= sock_no_socketpair,
	.accept		= sock_no_accept,
	.getname	= raw_getname,
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

static int raw_init(struct sock *sk)
{
	struct raw_sock *raw = raw_sk(sk);

	raw->ifindex = 0;

	return 0;
}

static struct proto raw_proto __read_mostly = {
	.name = "LORA_RAW",
	.owner = THIS_MODULE,
	.obj_size = sizeof(struct raw_sock),
	.init = raw_init,
};

static int lora_create(struct net *net, struct socket *sock, int protocol,
		       int kern)
{
	struct sock *sk;

	sock->state = SS_UNCONNECTED;

	if (protocol < 0 || protocol >= LORA_NPROTO)
		return -EINVAL;

	if (!net_eq(net, &init_net))
		return -EAFNOSUPPORT;

	sock->ops = &raw_ops;

	sk = sk_alloc(net, PF_LORA, GFP_KERNEL, &raw_proto, kern);
	if (!sk)
		return -ENOMEM;

	sock_init_data(sock, sk);

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
	pr_info("lora: init");

	sock_register(&lora_net_proto_family);

	return 0;
}

static __exit void lora_exit(void)
{
	pr_info("lora: exit");

	sock_unregister(PF_LORA);
}

module_init(lora_init);
module_exit(lora_exit);

MODULE_DESCRIPTION("LoRa PF_LORA core");
MODULE_AUTHOR("Andreas Färber <afaerber@suse.de>");
MODULE_LICENSE("GPL");
MODULE_ALIAS_NETPROTO(PF_LORA);
