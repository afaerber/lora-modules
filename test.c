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

#include "af_lora.h"

#ifndef AF_LORA
#define AF_LORA 28
#endif

#ifndef PF_LORA
#define PF_LORA AF_LORA
#endif

int main(void)
{
	int skt = socket(PF_LORA, SOCK_DGRAM, 1);
	if (skt == -1) {
		int err = errno;
		printf("socket failed: %s\n", strerror(err));
		return 1;
	}
	printf("socket %d\n", skt);

	struct ifreq ifr;
	strcpy(ifr.ifr_name, "lora0");
	int ret = ioctl(skt, SIOCGIFINDEX, &ifr);
	if (ret == -1) {
		int err = errno;
		printf("ioctl failed: %s\n", strerror(err));
		return 1;
	}
	printf("ifindex %d\n", ifr.ifr_ifindex);

	struct sockaddr_lora addr;
	addr.lora_family = AF_LORA;
	addr.lora_ifindex = ifr.ifr_ifindex;
	ret = bind(skt, (struct sockaddr *)&addr, sizeof(addr));
	if (ret == -1) {
		int err = errno;
		printf("bind failed: %s\n", strerror(err));
		return 1;
	}

	char buf[1];
	buf[0] = 0x42;
	int bytes_sent = write(skt, buf, 1);
	if (bytes_sent == -1) {
		int err = errno;
		printf("write failed: %s\n", strerror(err));
		return 1;
	}
	printf("bytes_sent %d\n", bytes_sent);

	return 0;
}
