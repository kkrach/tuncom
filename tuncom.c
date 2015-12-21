/*

openvpn --mktun --dev tun16
ip link set tun16 up
ip rule add iif tun16 table 50
ip route add 0/0 dev eth1 table 50

ip rule add to 192.168.32.99 table 40
ip route add dev tun16 table 40
*/

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <linux/if_tun.h>
#include <net/if.h>
#include <netinet/ip.h>			// struct iphdr
#include <netinet/udp.h>		// struct udphdr
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#define BUFFERSIZE 1024

static pthread_t tunread_thread_handle;
static int fd_tun = -1;

int tun_alloc(char *dev)
{
	struct ifreq ifr;
	int fd, err;
	if( (fd = open("/dev/net/tun", O_RDWR)) < 0 ) {
		perror("open-tun: ");
		return fd;
	}
	memset(&ifr, 0, sizeof(ifr));
	ifr.ifr_flags = IFF_TUN | IFF_NO_PI;
	if( *dev ) strncpy(ifr.ifr_name, dev, IFNAMSIZ);
	if( (err = ioctl(fd, TUNSETIFF, (void *) &ifr)) < 0 ){
		perror("TUNSETIFF: ");
		close(fd);
		return err;
	}
	strcpy(dev, ifr.ifr_name);
	return fd;
}

void* run_rx_thread(void* dummy) {
	int bytes;
	char buffer[BUFFERSIZE];
	while( (bytes=read(fd_tun, buffer, BUFFERSIZE)) > 0 ) {
		printf("# %d bytes received from tunnel!\n", bytes);
	}
	return NULL;
}

unsigned short csum(unsigned short *ptr,int nbytes) {
	register long sum;
	unsigned short oddbyte;
	register short answer;
	sum=0;
	while(nbytes>1) {
		sum+=*ptr++;
		nbytes-=2;
	}
	if(nbytes==1) {
		oddbyte=0;
		*((u_char*)&oddbyte)=*(u_char*)ptr;
		sum+=oddbyte;
	}
	sum = (sum>>16)+(sum & 0xffff);
	sum = sum + (sum>>16);
	answer=(short)~sum;
	return answer;
}

void usage(const char* prg) {
	printf("%s [TUN] [SRC] [DST]\n\n", prg);
	printf("    TUN: name of tunnel (e.g. 'tun16')\n");
	printf("    SRC: source address (fails if it's the device address)\n");
	printf("    DST: destination address\n");
	printf("\nExample: %s tun16 192.168.32.50 192.168.32.3\n", prg);
}

int main(int argc, const char* argv[]) {
	char tun_name[IFNAMSIZ];
	struct in_addr source_ipv4;
	struct in_addr stream_out_ipv4;
	
	if (argc < 4) {
		usage(argv[0]);
		return 1;
	}
	sprintf(tun_name, argv[1]);
	inet_aton(argv[2], &source_ipv4);
	inet_aton(argv[3], &stream_out_ipv4);

	fd_tun = tun_alloc(tun_name);
	if (fd_tun < 0) return 1;

	pthread_create(&tunread_thread_handle, NULL, run_rx_thread, NULL);

	struct {
		struct iphdr  ip4_hdr;
		struct udphdr udp_hdr;
		char buf[BUFFERSIZE];
	} packet;

	printf("Press CTRL+D to terminate program.\n");
	printf("> ");
	while (fgets(packet.buf, BUFFERSIZE , stdin)) {
		uint32_t dat_len = strlen(packet.buf);
		uint32_t hdr_len = sizeof(struct udphdr)+sizeof(struct iphdr);
		uint32_t pkg_len = hdr_len + dat_len;
		memset(&packet, 0, hdr_len);

		packet.ip4_hdr.ihl      = 5;
		packet.ip4_hdr.version  = 4;
		packet.ip4_hdr.tos      = 0;
		packet.ip4_hdr.tot_len  = pkg_len;
		packet.ip4_hdr.id       = htons(0);
		packet.ip4_hdr.frag_off = htons(0x4000);
		packet.ip4_hdr.ttl      = 128;
		packet.ip4_hdr.protocol = IPPROTO_UDP;
		packet.ip4_hdr.check    = 0;
		packet.ip4_hdr.saddr    = htonl(source_ipv4.s_addr);
		packet.ip4_hdr.daddr    = htonl(stream_out_ipv4.s_addr);
		packet.ip4_hdr.check    = csum((unsigned short *)&packet.ip4_hdr, sizeof(struct iphdr));

		packet.udp_hdr.source = htons(12345);
		packet.udp_hdr.dest   = htons(51000);
		packet.udp_hdr.len    = htons(dat_len+8);
		packet.udp_hdr.check  = 0;

		int bytes = write(fd_tun, &packet, pkg_len);
		if (bytes < 0) {
			int errsv = errno;
			printf("# write to tunnel failed: %s\n", strerror(errsv));
			close(fd_tun);
			return 2;
		}

		printf("# Sent %d bytes (%d data-bytes) via tunnel!\n", bytes, dat_len); fflush(stdout);
		printf("> ");
	}
	close(fd_tun);

	return 0;
}




