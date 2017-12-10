// SPDX-License-Identifier: GPL-2.0+ OR MIT
/*
 * Copyright (c) 2017 Andreas Färber
 */
#ifndef LORA_H
#define LORA_H

typedef u8 lora_eui[8];

#define PRIxLORAEUI "%02x%02x%02x%02x%02x%02x%02x%02x"
#define PRIXLORAEUI "%02X%02X%02X%02X%02X%02X%02X%02X"
#define LORA_EUI(x) x[0], x[1], x[2], x[3], x[4], x[5], x[6], x[7]

static inline int lora_strtoeui(const char *str, lora_eui *val)
{
        char buf[3];
        int i, ret;

        for (i = 0; i < 8; i++) {
                strncpy(buf, str + i * 2, 2);
                buf[2] = 0;
                ret = kstrtou8(buf, 16, &(*val)[i]);
                if (ret)
                        return ret;
        }
        return 0;
}

extern struct proto dgram_proto;
extern const struct proto_ops dgram_proto_ops;

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

struct sk_buff *alloc_lora_skb(struct net_device *dev, u8 **data);

int lora_send(struct sk_buff *skb);

struct net_device *alloc_loradev(int sizeof_priv);
void free_loradev(struct net_device *dev);
int register_loradev(struct net_device *dev);
void unregister_loradev(struct net_device *dev);
int open_loradev(struct net_device *dev);
void close_loradev(struct net_device *dev);

struct lora_priv {
	struct net_device *dev;
};

#endif
