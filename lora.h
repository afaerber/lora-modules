/*
 * Copyright (c) 2017 Andreas Färber
 *
 * SPDX-License-Identifier: GPL-2.0+ OR MIT
 */
#ifndef LORA_H
#define LORA_H

struct net_device *alloc_loradev(int sizeof_priv);
void free_loradev(struct net_device *dev);
int register_loradev(struct net_device *dev);
void unregister_loradev(struct net_device *dev);

struct lora_priv {
	struct net_device *dev;
};

#endif
