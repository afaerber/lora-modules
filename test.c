#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
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
	if (skt == -1) {
		printf("socket failed: errno %d\n", errno);
		return 1;
	}
	printf("socket %d\n", skt);

	struct ifreq ifr;
	strcpy(ifr.ifr_name, "lora0");
	int ret = ioctl(skt, SIOCGIFINDEX, &ifr);
	if (ret == -1) {
		printf("ioctl failed: errno %d\n", errno);
		return 1;
	}
	printf("ifindex %d\n", ifr.ifr_ifindex);

	struct sockaddr_lora addr;
	addr.lora_family = AF_LORA;
	addr.lora_ifindex = ifr.ifr_ifindex;
	ret = bind(skt, (struct sockaddr *)&addr, sizeof(addr));
	if (ret == -1) {
		printf("bind failed: errno %d\n", errno);
		return 1;
	}

	char buf[0];
	int bytes_sent = write(skt, buf, 0);
	if (bytes_sent == -1) {
		printf("write failed: errno %d\n", errno);
		return 1;
	}

	return 0;
}
