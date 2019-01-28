#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <linux/if_packet.h>
#include <linux/socket.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/types.h>

#ifndef ARPHRD_ENOCEAN
#define ARPHRD_ENOCEAN 832
#endif

#ifndef ETH_P_ERP2
#define ETH_P_ERP2 0x0100
#endif

int main(void)
{
	int skt = socket(PF_PACKET, SOCK_DGRAM, htons(ETH_P_ERP2));
	if (skt == -1) {
		int err = errno;
		fprintf(stderr, "socket failed: %s\n", strerror(err));
		return 1;
	}
	printf("socket %d\n", skt);

	struct ifreq ifr;
	strcpy(ifr.ifr_name, "enocean0");
	int ret = ioctl(skt, SIOCGIFINDEX, &ifr);
	if (ret == -1) {
		int err = errno;
		fprintf(stderr, "ioctl failed: %s\n", strerror(err));
		return 1;
	}
	printf("ifindex %d\n", ifr.ifr_ifindex);

	struct sockaddr_ll addr;
	memset(&addr, 0, sizeof(addr));
	addr.sll_family = AF_PACKET;
	addr.sll_protocol = htons(ETH_P_ERP2);
	addr.sll_ifindex = ifr.ifr_ifindex;
	addr.sll_halen = 0;

	ret = bind(skt, (struct sockaddr *)&addr, sizeof(addr));
	if (ret == -1) {
		int err = errno;
		fprintf(stderr, "bind failed: %s\n", strerror(err));
		return 1;
	}

	char buf[15];
	buf[0] = 0xD2;
	buf[1] = 0xDD;
	buf[2] = 0xDD;
	buf[3] = 0xDD;
	buf[4] = 0xDD;
	buf[5] = 0xDD;
	buf[6] = 0xDD;
	buf[7] = 0xDD;
	buf[8] = 0xDD;
	buf[9] = 0xDD;
	buf[10] = 0x00;
	buf[11] = 0x80;
	buf[12] = 0x35;
	buf[13] = 0xC4;
	buf[14] = 0x00;
	int bytes_sent = write(skt, buf, 15);
	if (bytes_sent == -1) {
		int err = errno;
		fprintf(stderr, "write failed: %s\n", strerror(err));
		return 1;
	}
	printf("bytes_sent %d\n", bytes_sent);

	return 0;
}
