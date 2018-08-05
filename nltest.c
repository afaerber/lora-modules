#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <linux/socket.h>
#include <sys/ioctl.h>
#include <net/if.h>

#include <netlink/netlink.h>
#include <netlink/genl/genl.h>
#include <netlink/genl/ctrl.h>

#include "include/linux/lora.h"
#include "include/linux/nllora.h"

static struct nla_policy my_policy[NLLORA_ATTR_MAX + 1] = {
	[NLLORA_ATTR_IFINDEX] = { .type = NLA_U32 },
};

static int seq_check(struct nl_msg *msg, void *arg)
{
	return NL_OK;
}

static int print_msg(struct nl_msg *msg, void *arg)
{
	struct nlattr *attr[NLLORA_ATTR_MAX + 1];

	genlmsg_parse(nlmsg_hdr(msg), 0, attr, NLLORA_ATTR_MAX, my_policy);

	printf("received!\n");

	nl_msg_dump(msg, stdout);

	return NL_OK;
}

int main(void)
{
	struct ifreq ifr;
	int skt;
	struct nl_sock *sk;
	struct nl_msg *msg;
	struct nl_cb *cb;
	void *ptr;
	int family_id, ret;

	skt = socket(PF_LORA, SOCK_DGRAM, 1);
	if (skt == -1) {
		int err = errno;
		fprintf(stderr, "socket failed: %s\n", strerror(err));
		return 1;
	}

	strcpy(ifr.ifr_name, "lora0");
	ret = ioctl(skt, SIOCGIFINDEX, &ifr);
	if (ret == -1) {
		int err = errno;
		fprintf(stderr, "ioctl failed: %s\n", strerror(err));
		return 1;
	}
	printf("ifindex %d\n", ifr.ifr_ifindex);

	sk = nl_socket_alloc();
	if (sk == NULL) {
		fprintf(stderr, "nl_socket_alloc\n");
		return 1;
	}

	ret = genl_connect(sk);
	if (ret < 0) {
		fprintf(stderr, "nl_socket_alloc\n");
		nl_socket_free(sk);
		return 1;
	}

	family_id = genl_ctrl_resolve(sk, NLLORA_GENL_NAME);
	if (family_id < 0) {
		fprintf(stderr, "genl_ctrl_resolve\n");
		nl_socket_free(sk);
		return 1;
	}

	msg = nlmsg_alloc();
	if (msg == NULL) {
		fprintf(stderr, "nlmsg_alloc\n");
		nl_socket_free(sk);
		return 1;
	}

	ptr = genlmsg_put(msg, NL_AUTO_PID, NL_AUTO_SEQ, family_id, 0, NLM_F_REQUEST, NLLORA_CMD_FOO, 0);
	if (ptr == NULL) {
		fprintf(stderr, "genlmsg_put\n");
		nlmsg_free(msg);
		nl_socket_free(sk);
		return 1;
	}

	ret = nla_put_u32(msg, NLLORA_ATTR_IFINDEX, ifr.ifr_ifindex);
	if (ret < 0) {
		fprintf(stderr, "nla_put_u32\n");
		nlmsg_free(msg);
		nl_socket_free(sk);
		return 1;
	}

	ret = nl_send_auto(sk, msg);
	if (ret < 0) {
		fprintf(stderr, "nl_send_auto\n");
		nlmsg_free(msg);
		nl_socket_free(sk);
		return 1;
	}

	nl_msg_dump(msg, stdout);

	nlmsg_free(msg);

	cb = nl_cb_alloc(NL_CB_DEFAULT);
	if (cb == NULL) {
		fprintf(stderr, "nl_cb_alloc\n");
		nl_socket_free(sk);
		return 1;
	}

	nl_cb_set(cb, NL_CB_SEQ_CHECK, NL_CB_CUSTOM, seq_check, NULL);
	nl_cb_set(cb, NL_CB_VALID, NL_CB_CUSTOM, print_msg, NULL);

	ret = nl_recvmsgs(sk, cb);

	nl_cb_put(cb);

	nl_socket_free(sk);

	return 0;
}
