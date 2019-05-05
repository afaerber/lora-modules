#include <errno.h>
#include <stdio.h>
#include <stdint.h>
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
#include "include/linux/nlfsk.h"

static struct nla_policy my_lora_policy[NLLORA_ATTR_MAX + 1] = {
	[NLLORA_ATTR_IFINDEX]	= { .type = NLA_U32 },
	[NLLORA_ATTR_FREQ]	= { .type = NLA_U32 },
	[NLLORA_ATTR_TX_POWER]	= { .type = NLA_S32 },
};

static struct nla_policy my_fsk_policy[NLFSK_ATTR_MAX + 1] = {
	[NLFSK_ATTR_IFINDEX]	= { .type = NLA_U32 },
	[NLFSK_ATTR_FREQ]	= { .type = NLA_U32 },
	[NLFSK_ATTR_TX_POWER]	= { .type = NLA_S32 },
};

static int seq_check(struct nl_msg *msg, void *arg)
{
	return NL_OK;
}

static int nllora_get_freq_val(struct nl_msg *msg, void *arg)
{
	struct nlattr *attrs[NLLORA_ATTR_MAX + 1];
	uint32_t *freq = arg;

	genlmsg_parse(nlmsg_hdr(msg), 0, attrs, NLLORA_ATTR_MAX, my_lora_policy);

	if (!attrs[NLLORA_ATTR_FREQ])
		return NL_SKIP;

	*freq = nla_get_u32(attrs[NLLORA_ATTR_FREQ]);

	return NL_OK;
}

static int nllora_get_freq(struct nl_sock *sk, int family_id,
	int ifindex, uint32_t *val)
{
	struct nl_msg *msg;
	struct nl_cb *cb;
	void *ptr;
	int ret;

	msg = nlmsg_alloc();
	if (msg == NULL) {
		fprintf(stderr, "nlmsg_alloc\n");
		return -ENOMEM;
	}

	ptr = genlmsg_put(msg, NL_AUTO_PID, NL_AUTO_SEQ, family_id, 0, NLM_F_REQUEST, NLLORA_CMD_GET_FREQ, 0);
	if (ptr == NULL) {
		fprintf(stderr, "genlmsg_put\n");
		nlmsg_free(msg);
		return -ENOMEM;
	}

	ret = nla_put_u32(msg, NLLORA_ATTR_IFINDEX, ifindex);
	if (ret < 0) {
		fprintf(stderr, "nla_put_u32\n");
		nlmsg_free(msg);
		return ret;
	}

	ret = nl_send_auto(sk, msg);
	if (ret < 0) {
		fprintf(stderr, "nl_send_auto\n");
		nlmsg_free(msg);
		return ret;
	}

	nlmsg_free(msg);

	cb = nl_cb_alloc(NL_CB_DEFAULT);
	if (cb == NULL) {
		fprintf(stderr, "nl_cb_alloc\n");
		return -ENOMEM;
	}

	nl_cb_set(cb, NL_CB_SEQ_CHECK, NL_CB_CUSTOM, seq_check, NULL);
	nl_cb_set(cb, NL_CB_VALID, NL_CB_CUSTOM, nllora_get_freq_val, val);

	ret = nl_recvmsgs(sk, cb);

	nl_cb_put(cb);

	return ret;
}

static int nllora_set_freq(struct nl_sock *sk, int family_id,
	int ifindex, uint32_t val)
{
	struct nl_msg *msg;
	struct nl_cb *cb;
	void *ptr;
	int ret;

	msg = nlmsg_alloc();
	if (msg == NULL) {
		fprintf(stderr, "nlmsg_alloc\n");
		return -ENOMEM;
	}

	ptr = genlmsg_put(msg, NL_AUTO_PID, NL_AUTO_SEQ, family_id, 0, NLM_F_REQUEST, NLLORA_CMD_SET_FREQ, 0);
	if (ptr == NULL) {
		fprintf(stderr, "genlmsg_put\n");
		nlmsg_free(msg);
		return -ENOMEM;
	}

	ret = nla_put_u32(msg, NLLORA_ATTR_IFINDEX, ifindex);
	if (ret < 0) {
		fprintf(stderr, "nla_put_u32\n");
		nlmsg_free(msg);
		return ret;
	}

	ret = nla_put_u32(msg, NLLORA_ATTR_FREQ, val);
	if (ret < 0) {
		fprintf(stderr, "nla_put_u32 2\n");
		nlmsg_free(msg);
		return ret;
	}

	ret = nl_send_auto(sk, msg);
	if (ret < 0) {
		fprintf(stderr, "nl_send_auto\n");
		nlmsg_free(msg);
		return ret;
	}

	nlmsg_free(msg);

	cb = nl_cb_alloc(NL_CB_DEFAULT);
	if (cb == NULL) {
		fprintf(stderr, "nl_cb_alloc\n");
		return -ENOMEM;
	}

	nl_cb_set(cb, NL_CB_SEQ_CHECK, NL_CB_CUSTOM, seq_check, NULL);

	ret = nl_recvmsgs(sk, cb);

	nl_cb_put(cb);

	return ret;
}

static int nllora_get_tx_power_val(struct nl_msg *msg, void *arg)
{
	struct nlattr *attrs[NLLORA_ATTR_MAX + 1];
	uint32_t *freq = arg;

	genlmsg_parse(nlmsg_hdr(msg), 0, attrs, NLLORA_ATTR_MAX, my_lora_policy);

	if (!attrs[NLLORA_ATTR_TX_POWER])
		return NL_SKIP;

	*freq = nla_get_u32(attrs[NLLORA_ATTR_TX_POWER]);

	return NL_OK;
}

static int nllora_get_tx_power(struct nl_sock *sk, int family_id,
	int ifindex, int32_t *val)
{
	struct nl_msg *msg;
	struct nl_cb *cb;
	void *ptr;
	int ret;

	msg = nlmsg_alloc();
	if (msg == NULL) {
		fprintf(stderr, "nlmsg_alloc\n");
		return -ENOMEM;
	}

	ptr = genlmsg_put(msg, NL_AUTO_PID, NL_AUTO_SEQ, family_id, 0, NLM_F_REQUEST, NLLORA_CMD_GET_TX_POWER, 0);
	if (ptr == NULL) {
		fprintf(stderr, "genlmsg_put\n");
		nlmsg_free(msg);
		return -ENOMEM;
	}

	ret = nla_put_u32(msg, NLLORA_ATTR_IFINDEX, ifindex);
	if (ret < 0) {
		fprintf(stderr, "nla_put_u32\n");
		nlmsg_free(msg);
		return ret;
	}

	ret = nl_send_auto(sk, msg);
	if (ret < 0) {
		fprintf(stderr, "nl_send_auto\n");
		nlmsg_free(msg);
		return ret;
	}

	nlmsg_free(msg);

	cb = nl_cb_alloc(NL_CB_DEFAULT);
	if (cb == NULL) {
		fprintf(stderr, "nl_cb_alloc\n");
		return -ENOMEM;
	}

	nl_cb_set(cb, NL_CB_SEQ_CHECK, NL_CB_CUSTOM, seq_check, NULL);
	nl_cb_set(cb, NL_CB_VALID, NL_CB_CUSTOM, nllora_get_tx_power_val, val);

	ret = nl_recvmsgs(sk, cb);

	nl_cb_put(cb);

	return ret;
}

static int nllora_set_tx_power(struct nl_sock *sk, int family_id,
	int ifindex, int32_t val)
{
	struct nl_msg *msg;
	struct nl_cb *cb;
	void *ptr;
	int ret;

	msg = nlmsg_alloc();
	if (msg == NULL) {
		fprintf(stderr, "nlmsg_alloc\n");
		return -ENOMEM;
	}

	ptr = genlmsg_put(msg, NL_AUTO_PID, NL_AUTO_SEQ, family_id, 0, NLM_F_REQUEST, NLLORA_CMD_SET_TX_POWER, 0);
	if (ptr == NULL) {
		fprintf(stderr, "genlmsg_put\n");
		nlmsg_free(msg);
		return -ENOMEM;
	}

	ret = nla_put_u32(msg, NLLORA_ATTR_IFINDEX, ifindex);
	if (ret < 0) {
		fprintf(stderr, "nla_put_u32\n");
		nlmsg_free(msg);
		return ret;
	}

	ret = nla_put_s32(msg, NLLORA_ATTR_TX_POWER, val);
	if (ret < 0) {
		fprintf(stderr, "nla_put_s32\n");
		nlmsg_free(msg);
		return ret;
	}

	ret = nl_send_auto(sk, msg);
	if (ret < 0) {
		fprintf(stderr, "nl_send_auto\n");
		nlmsg_free(msg);
		return ret;
	}

	nlmsg_free(msg);

	cb = nl_cb_alloc(NL_CB_DEFAULT);
	if (cb == NULL) {
		fprintf(stderr, "nl_cb_alloc\n");
		return -ENOMEM;
	}

	nl_cb_set(cb, NL_CB_SEQ_CHECK, NL_CB_CUSTOM, seq_check, NULL);

	ret = nl_recvmsgs(sk, cb);

	nl_cb_put(cb);

	return ret;
}

static int nlfsk_get_freq_val(struct nl_msg *msg, void *arg)
{
	struct nlattr *attrs[NLFSK_ATTR_MAX + 1];
	uint32_t *freq = arg;

	genlmsg_parse(nlmsg_hdr(msg), 0, attrs, NLFSK_ATTR_MAX, my_fsk_policy);

	if (!attrs[NLFSK_ATTR_FREQ])
		return NL_SKIP;

	*freq = nla_get_u32(attrs[NLFSK_ATTR_FREQ]);

	return NL_OK;
}

static int nlfsk_get_freq(struct nl_sock *sk, int family_id,
	int ifindex, uint32_t *val)
{
	struct nl_msg *msg;
	struct nl_cb *cb;
	void *ptr;
	int ret;

	msg = nlmsg_alloc();
	if (msg == NULL) {
		fprintf(stderr, "nlmsg_alloc\n");
		return -ENOMEM;
	}

	ptr = genlmsg_put(msg, NL_AUTO_PID, NL_AUTO_SEQ, family_id, 0, NLM_F_REQUEST, NLFSK_CMD_GET_FREQ, 0);
	if (ptr == NULL) {
		fprintf(stderr, "genlmsg_put\n");
		nlmsg_free(msg);
		return -ENOMEM;
	}

	ret = nla_put_u32(msg, NLFSK_ATTR_IFINDEX, ifindex);
	if (ret < 0) {
		fprintf(stderr, "nla_put_u32\n");
		nlmsg_free(msg);
		return ret;
	}

	ret = nl_send_auto(sk, msg);
	if (ret < 0) {
		fprintf(stderr, "nl_send_auto\n");
		nlmsg_free(msg);
		return ret;
	}

	nlmsg_free(msg);

	cb = nl_cb_alloc(NL_CB_DEFAULT);
	if (cb == NULL) {
		fprintf(stderr, "nl_cb_alloc\n");
		return -ENOMEM;
	}

	nl_cb_set(cb, NL_CB_SEQ_CHECK, NL_CB_CUSTOM, seq_check, NULL);
	nl_cb_set(cb, NL_CB_VALID, NL_CB_CUSTOM, nlfsk_get_freq_val, val);

	ret = nl_recvmsgs(sk, cb);

	nl_cb_put(cb);

	return ret;
}

static int nlfsk_set_freq(struct nl_sock *sk, int family_id,
	int ifindex, uint32_t val)
{
	struct nl_msg *msg;
	struct nl_cb *cb;
	void *ptr;
	int ret;

	msg = nlmsg_alloc();
	if (msg == NULL) {
		fprintf(stderr, "nlmsg_alloc\n");
		return -ENOMEM;
	}

	ptr = genlmsg_put(msg, NL_AUTO_PID, NL_AUTO_SEQ, family_id, 0, NLM_F_REQUEST, NLFSK_CMD_SET_FREQ, 0);
	if (ptr == NULL) {
		fprintf(stderr, "genlmsg_put\n");
		nlmsg_free(msg);
		return -ENOMEM;
	}

	ret = nla_put_u32(msg, NLFSK_ATTR_IFINDEX, ifindex);
	if (ret < 0) {
		fprintf(stderr, "nla_put_u32\n");
		nlmsg_free(msg);
		return ret;
	}

	ret = nla_put_u32(msg, NLFSK_ATTR_FREQ, val);
	if (ret < 0) {
		fprintf(stderr, "nla_put_u32 2\n");
		nlmsg_free(msg);
		return ret;
	}

	ret = nl_send_auto(sk, msg);
	if (ret < 0) {
		fprintf(stderr, "nl_send_auto\n");
		nlmsg_free(msg);
		return ret;
	}

	nlmsg_free(msg);

	cb = nl_cb_alloc(NL_CB_DEFAULT);
	if (cb == NULL) {
		fprintf(stderr, "nl_cb_alloc\n");
		return -ENOMEM;
	}

	nl_cb_set(cb, NL_CB_SEQ_CHECK, NL_CB_CUSTOM, seq_check, NULL);

	ret = nl_recvmsgs(sk, cb);

	nl_cb_put(cb);

	return ret;
}

static int nlfsk_get_freq_dev(struct nl_sock *sk, int family_id,
	int ifindex, uint32_t *val)
{
	struct nl_msg *msg;
	struct nl_cb *cb;
	void *ptr;
	int ret;

	msg = nlmsg_alloc();
	if (msg == NULL) {
		fprintf(stderr, "nlmsg_alloc\n");
		return -ENOMEM;
	}

	ptr = genlmsg_put(msg, NL_AUTO_PID, NL_AUTO_SEQ, family_id, 0, NLM_F_REQUEST, NLFSK_CMD_GET_FREQ_DEV, 0);
	if (ptr == NULL) {
		fprintf(stderr, "genlmsg_put\n");
		nlmsg_free(msg);
		return -ENOMEM;
	}

	ret = nla_put_u32(msg, NLFSK_ATTR_IFINDEX, ifindex);
	if (ret < 0) {
		fprintf(stderr, "nla_put_u32\n");
		nlmsg_free(msg);
		return ret;
	}

	ret = nl_send_auto(sk, msg);
	if (ret < 0) {
		fprintf(stderr, "nl_send_auto\n");
		nlmsg_free(msg);
		return ret;
	}

	nlmsg_free(msg);

	cb = nl_cb_alloc(NL_CB_DEFAULT);
	if (cb == NULL) {
		fprintf(stderr, "nl_cb_alloc\n");
		return -ENOMEM;
	}

	nl_cb_set(cb, NL_CB_SEQ_CHECK, NL_CB_CUSTOM, seq_check, NULL);
	nl_cb_set(cb, NL_CB_VALID, NL_CB_CUSTOM, nlfsk_get_freq_val, val);

	ret = nl_recvmsgs(sk, cb);

	nl_cb_put(cb);

	return ret;
}

static int nlfsk_set_freq_dev(struct nl_sock *sk, int family_id,
	int ifindex, uint32_t val)
{
	struct nl_msg *msg;
	struct nl_cb *cb;
	void *ptr;
	int ret;

	msg = nlmsg_alloc();
	if (msg == NULL) {
		fprintf(stderr, "nlmsg_alloc\n");
		return -ENOMEM;
	}

	ptr = genlmsg_put(msg, NL_AUTO_PID, NL_AUTO_SEQ, family_id, 0, NLM_F_REQUEST, NLFSK_CMD_SET_FREQ_DEV, 0);
	if (ptr == NULL) {
		fprintf(stderr, "genlmsg_put\n");
		nlmsg_free(msg);
		return -ENOMEM;
	}

	ret = nla_put_u32(msg, NLFSK_ATTR_IFINDEX, ifindex);
	if (ret < 0) {
		fprintf(stderr, "nla_put_u32\n");
		nlmsg_free(msg);
		return ret;
	}

	ret = nla_put_u32(msg, NLFSK_ATTR_FREQ, val);
	if (ret < 0) {
		fprintf(stderr, "nla_put_u32 2\n");
		nlmsg_free(msg);
		return ret;
	}

	ret = nl_send_auto(sk, msg);
	if (ret < 0) {
		fprintf(stderr, "nl_send_auto\n");
		nlmsg_free(msg);
		return ret;
	}

	nlmsg_free(msg);

	cb = nl_cb_alloc(NL_CB_DEFAULT);
	if (cb == NULL) {
		fprintf(stderr, "nl_cb_alloc\n");
		return -ENOMEM;
	}

	nl_cb_set(cb, NL_CB_SEQ_CHECK, NL_CB_CUSTOM, seq_check, NULL);

	ret = nl_recvmsgs(sk, cb);

	nl_cb_put(cb);

	return ret;
}

static int nlfsk_get_tx_power_val(struct nl_msg *msg, void *arg)
{
	struct nlattr *attrs[NLFSK_ATTR_MAX + 1];
	uint32_t *tx_power = arg;

	genlmsg_parse(nlmsg_hdr(msg), 0, attrs, NLFSK_ATTR_MAX, my_fsk_policy);

	if (!attrs[NLFSK_ATTR_TX_POWER])
		return NL_SKIP;

	*tx_power = nla_get_u32(attrs[NLFSK_ATTR_TX_POWER]);

	return NL_OK;
}

static int nlfsk_get_tx_power(struct nl_sock *sk, int family_id,
	int ifindex, int32_t *val)
{
	struct nl_msg *msg;
	struct nl_cb *cb;
	void *ptr;
	int ret;

	msg = nlmsg_alloc();
	if (msg == NULL) {
		fprintf(stderr, "nlmsg_alloc\n");
		return -ENOMEM;
	}

	ptr = genlmsg_put(msg, NL_AUTO_PID, NL_AUTO_SEQ, family_id, 0, NLM_F_REQUEST, NLFSK_CMD_GET_TX_POWER, 0);
	if (ptr == NULL) {
		fprintf(stderr, "genlmsg_put\n");
		nlmsg_free(msg);
		return -ENOMEM;
	}

	ret = nla_put_u32(msg, NLFSK_ATTR_IFINDEX, ifindex);
	if (ret < 0) {
		fprintf(stderr, "nla_put_u32\n");
		nlmsg_free(msg);
		return ret;
	}

	ret = nl_send_auto(sk, msg);
	if (ret < 0) {
		fprintf(stderr, "nl_send_auto\n");
		nlmsg_free(msg);
		return ret;
	}

	nlmsg_free(msg);

	cb = nl_cb_alloc(NL_CB_DEFAULT);
	if (cb == NULL) {
		fprintf(stderr, "nl_cb_alloc\n");
		return -ENOMEM;
	}

	nl_cb_set(cb, NL_CB_SEQ_CHECK, NL_CB_CUSTOM, seq_check, NULL);
	nl_cb_set(cb, NL_CB_VALID, NL_CB_CUSTOM, nlfsk_get_tx_power_val, val);

	ret = nl_recvmsgs(sk, cb);

	nl_cb_put(cb);

	return ret;
}

static int nlfsk_set_tx_power(struct nl_sock *sk, int family_id,
	int ifindex, int32_t val)
{
	struct nl_msg *msg;
	struct nl_cb *cb;
	void *ptr;
	int ret;

	msg = nlmsg_alloc();
	if (msg == NULL) {
		fprintf(stderr, "nlmsg_alloc\n");
		return -ENOMEM;
	}

	ptr = genlmsg_put(msg, NL_AUTO_PID, NL_AUTO_SEQ, family_id, 0, NLM_F_REQUEST, NLFSK_CMD_SET_TX_POWER, 0);
	if (ptr == NULL) {
		fprintf(stderr, "genlmsg_put\n");
		nlmsg_free(msg);
		return -ENOMEM;
	}

	ret = nla_put_u32(msg, NLFSK_ATTR_IFINDEX, ifindex);
	if (ret < 0) {
		fprintf(stderr, "nla_put_u32\n");
		nlmsg_free(msg);
		return ret;
	}

	ret = nla_put_s32(msg, NLFSK_ATTR_TX_POWER, val);
	if (ret < 0) {
		fprintf(stderr, "nla_put_s32\n");
		nlmsg_free(msg);
		return ret;
	}

	ret = nl_send_auto(sk, msg);
	if (ret < 0) {
		fprintf(stderr, "nl_send_auto\n");
		nlmsg_free(msg);
		return ret;
	}

	nlmsg_free(msg);

	cb = nl_cb_alloc(NL_CB_DEFAULT);
	if (cb == NULL) {
		fprintf(stderr, "nl_cb_alloc\n");
		return -ENOMEM;
	}

	nl_cb_set(cb, NL_CB_SEQ_CHECK, NL_CB_CUSTOM, seq_check, NULL);

	ret = nl_recvmsgs(sk, cb);

	nl_cb_put(cb);

	return ret;
}

static int get_ifindex(const char *ifname, const char *mode, int *ifindex)
{
	struct ifreq ifr;
	int skt, ret;

	if (strcmp(mode, "lora") == 0) {
		//skt = socket(PF_LORA, SOCK_DGRAM, 1);
		skt = socket(PF_PACKET, SOCK_DGRAM, htons(ETH_P_LORA));
	} else if (strcmp(mode, "fsk") == 0) {
		skt = socket(PF_PACKET, SOCK_DGRAM, htons(ETH_P_FSK));
	} else return -EINVAL;
	if (skt == -1) {
		int err = errno;
		fprintf(stderr, "socket failed: %s (%d)\n", strerror(err), err);
		return -err;
	}

	strcpy(ifr.ifr_name, ifname);
	ret = ioctl(skt, SIOCGIFINDEX, &ifr);
	if (ret == -1) {
		int err = errno;
		fprintf(stderr, "ioctl failed: %s (%d)\n", strerror(err), err);
		close(skt);
		return -err;
	}
	close(skt);

	*ifindex = ifr.ifr_ifindex;
	return 0;
}

static int handle_lora(struct nl_sock *sk, int family_id, int ifindex, const char *cmd,
	int argc, char **args)
{
	uint32_t freq;
	int32_t tx_power;
	char *endptr;
	int ret;

	if (strcmp(cmd, "freq") == 0) {
		if (argc == 0) {
			ret = nllora_get_freq(sk, family_id, ifindex, &freq);
			if (ret) {
				fprintf(stderr, "nllora_get_freq\n");
				return 1;
			}
			printf("frequency: %u\n", freq);
		} else if (argc == 1) {
			freq = strtoul(args[0], &endptr, 0);
			if (endptr == args[0]) {
				fprintf(stderr, "invalid argument\n");
				return 1;
			}
			ret = nllora_set_freq(sk, family_id, ifindex, freq);
			if (ret) {
				fprintf(stderr, "nllora_set_freq\n");
				return 1;
			}
		} else return -EINVAL;
	} else if (strcmp(cmd, "tx_power") == 0) {
		if (argc == 0) {
			ret = nllora_get_tx_power(sk, family_id, ifindex, &tx_power);
			if (ret) {
				fprintf(stderr, "nllora_get_tx_power\n");
				return 1;
			}
			printf("tx power: %d\n", tx_power);
		} else if (argc == 1) {
			tx_power = strtol(args[0], &endptr, 0);
			if (endptr == args[0]) {
				fprintf(stderr, "invalid argument\n");
				return 1;
			}
			ret = nllora_set_tx_power(sk, family_id, ifindex, tx_power);
			if (ret) {
				fprintf(stderr, "nllora_set_tx_power\n");
				return 1;
			}
		} else return -EINVAL;
	} else return -EINVAL;
	return 0;
}

static int handle_fsk(struct nl_sock *sk, int family_id, int ifindex, const char *cmd,
	int argc, char **args)
{
	uint32_t freq;
	int32_t tx_power;
	char *endptr;
	int ret;

	if (strcmp(cmd, "freq") == 0) {
		if (argc == 0) {
			ret = nlfsk_get_freq(sk, family_id, ifindex, &freq);
			if (ret) {
				fprintf(stderr, "nlfsk_get_freq\n");
				return 1;
			}
			printf("frequency: %u\n", freq);
		} else if (argc == 1) {
			freq = strtoul(args[0], &endptr, 0);
			if (endptr == args[0]) {
				fprintf(stderr, "invalid argument\n");
				return 1;
			}
			ret = nlfsk_set_freq(sk, family_id, ifindex, freq);
			if (ret) {
				fprintf(stderr, "nlfsk_set_freq\n");
				return 1;
			}
		} else return -EINVAL;
	} else if (strcmp(cmd, "freq_dev") == 0) {
		if (argc == 0) {
			ret = nlfsk_get_freq_dev(sk, family_id, ifindex, &freq);
			if (ret) {
				fprintf(stderr, "nlfsk_get_freq_dev\n");
				return 1;
			}
			printf("frequency deviation: %u\n", freq);
		} else if (argc == 1) {
			freq = strtoul(args[0], &endptr, 0);
			if (endptr == args[0]) {
				fprintf(stderr, "invalid argument\n");
				return 1;
			}
			ret = nlfsk_set_freq_dev(sk, family_id, ifindex, freq);
			if (ret) {
				fprintf(stderr, "nlfsk_set_freq_dev\n");
				return 1;
			}
		} else return -EINVAL;
	} else if (strcmp(cmd, "tx_power") == 0) {
		if (argc == 0) {
			ret = nlfsk_get_tx_power(sk, family_id, ifindex, &tx_power);
			if (ret) {
				fprintf(stderr, "nlfsk_get_tx_power\n");
				return 1;
			}
			printf("tx power: %d\n", tx_power);
		} else if (argc == 1) {
			tx_power = strtol(args[0], &endptr, 0);
			if (endptr == args[0]) {
				fprintf(stderr, "invalid argument\n");
				return 1;
			}
			ret = nlfsk_set_tx_power(sk, family_id, ifindex, tx_power);
			if (ret) {
				fprintf(stderr, "nlfsk_set_tx_power\n");
				return 1;
			}
		} else return -EINVAL;
	} else return -EINVAL;
	return 0;
}

static int usage(const char *argv0)
{
	fprintf(stderr, "Usage: %s lora0 lora|fsk op\n", argv0);
	return 2;
}

int main(int argc, char **argv)
{
	struct nl_sock *sk;
	const char *name;
	int ifindex, family_id, ret;

	if (argc < 1 + 3) {
		return usage(argv[0]);
	}

	if (strcmp(argv[2], "lora") != 0 &&
	    strcmp(argv[2], "fsk") != 0)
		return usage(argv[0]);

	ret = get_ifindex(argv[1], argv[2], &ifindex);
	if (ret < 0)
		return 1;
	//printf("ifindex %d\n", ifindex);

	sk = nl_socket_alloc();
	if (sk == NULL) {
		fprintf(stderr, "nl_socket_alloc\n");
		return 1;
	}

	ret = genl_connect(sk);
	if (ret < 0) {
		fprintf(stderr, "genl_connect\n");
		nl_socket_free(sk);
		return 1;
	}

	if (strcmp(argv[2], "lora") == 0) {
		name = NLLORA_GENL_NAME;
	} else if (strcmp(argv[2], "fsk") == 0) {
		name = NLFSK_GENL_NAME;
	} else return 1;

	family_id = genl_ctrl_resolve(sk, name);
	if (family_id < 0) {
		fprintf(stderr, "genl_ctrl_resolve\n");
		nl_socket_free(sk);
		return 1;
	}

	if (strcmp(argv[2], "lora") == 0) {
		ret = handle_lora(sk, family_id, ifindex, argv[3], argc - 4, &argv[4]);
	} else if (strcmp(argv[2], "fsk") == 0) {
		ret = handle_fsk(sk, family_id, ifindex, argv[3], argc - 4, &argv[4]);
	} else return 1;
	if (ret) {
		nl_socket_free(sk);
		return 1;
	}

	nl_socket_free(sk);

	return 0;
}
