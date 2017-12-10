#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <linux/socket.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/types.h>

#ifndef AF_LORA
#define AF_LORA 28
#endif

#ifndef PF_LORA
#define PF_LORA AF_LORA
#endif

#include "af_lora.h"

int main(void)
{
	int skt = socket(PF_LORA, SOCK_DGRAM, 0);

	struct ifreq ifr;
	strcpy(ifr.ifr_name, "lora0");
	ioctl(skt, SIOCGIFINDEX, &ifr);

	struct sockaddr_lora addr;
	addr.lora_family = AF_LORA;
	addr.lora_ifindex = ifr.ifr_ifindex;
	bind(skt, (struct sockaddr *)&addr, sizeof(addr));

	char buf[0];
	int bytes_sent = write(skt, buf, 0);

	return 0;
}
