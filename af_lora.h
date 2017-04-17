/*
 * Copyright (c) 2017 Andreas Färber
 *
 * SPDX-License-Identifier: GPL-2.0+
 */
#ifndef AF_LORA_H
#define AF_LORA_H

#define AF_LORA 28 /* XXX some available value lower than AF_MAX */

#define PF_LORA AF_LORA

#define ARPHRD_LORA 519 /* XXX some unused value for "non ARP hardware" -- include/uapi/linux/if_arp.h */

#define LORA_NPROTO 1

#define LORA_MTU 36 /* XXX smallest maximum payload size? */

#endif
