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
#include <signal.h>

#define BUFFERSIZE 1024

pthread_t tunread_thread_handle;
static int fd_tun = -1;
static uint32_t rtp_sequence = 0;
static int interrupted = 0;

int tun_alloc(char *dev)
{
	struct ifreq ifr;
	int fd, err;

	if( (fd = open("/dev/net/tun", O_RDWR)) < 0 ) {
		perror("open-tun: ");
		return -1;
	}

	memset(&ifr, 0, sizeof(ifr));

	/* Flags: IFF_TUN   - TUN device (no Ethernet headers) 
	 *        IFF_TAP   - TAP device  
	 *
	 *        IFF_NO_PI - Do not provide packet information  
	 */ 
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

static void signal_terminate(int sig) {
	printf("Received signal %d.\n", sig); fflush(stdout);
	interrupted = sig;
}

void* run_rx_thread(void* dummy) {
	int bytes;
	char buffer[BUFFERSIZE];
	while( (bytes=read(fd_tun, buffer, BUFFERSIZE)) > 0 ) {
		printf("%d bytes confused to tunnel!\n", bytes);
	}
	perror("tun-read: ");
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

int main() {
	char tun_name[IFNAMSIZ];
	sprintf(tun_name, "tun16");

	signal(SIGINT, signal_terminate);   // Ctrl-C

	fd_tun = tun_alloc(tun_name);
	if (fd_tun < 0) {
		printf("Error: tun_alloc returned %d\n", fd_tun);
		return 1;
	}
	printf("\n"); fflush(stdout);
	printf("tunnel %s opened\n", tun_name); fflush(stdout);

//	pthread_create(&tunread_thread_handle, NULL, run_rx_thread, NULL);

	while (!interrupted) {

		uint8_t *text = (uint8_t*)"haaaaaaaaaaaaaaaaaaaaaaaaaaaaalllloooo";
		uint32_t len = strlen((const char*)text);
		struct in_addr source_ipv4;
		inet_aton("192.168.32.60", &source_ipv4);
		struct in_addr stream_out_ipv4;
		inet_aton("192.168.32.3", &stream_out_ipv4);

		struct {
			struct iphdr  ip4_hdr;
			struct udphdr udp_hdr;
			uint8_t buf[1024];
		} packet;

		memcpy(packet.buf, text, len);

		struct udphdr  *udph=(struct udphdr *)(packet.buf-sizeof(struct udphdr));
		struct iphdr   *ip4h=(struct iphdr  *)(packet.buf-sizeof(struct udphdr)-sizeof(struct iphdr));
		uint8_t *ipptr=(uint8_t *)ip4h;
		uint32_t iplen=sizeof(struct udphdr)+sizeof(struct iphdr)+len;
		ip4h->ihl      = 5;
		ip4h->version  = 4;
		ip4h->tos      = 0;
		ip4h->tot_len  = iplen;
		ip4h->id       = htons(rtp_sequence&0x0000FFFF);
		ip4h->frag_off = htons(0x4000);
		ip4h->ttl      = 128;
		ip4h->protocol = IPPROTO_UDP;
		ip4h->check    = 0;
		ip4h->saddr    = htonl(source_ipv4.s_addr);
		ip4h->daddr    = htonl(stream_out_ipv4.s_addr);
		ip4h->check    = csum((unsigned short *)ip4h, sizeof(struct iphdr));


		udph->source = htons(12345);
		udph->dest   = htons(51000);
		udph->len    = htons(len+8);
		udph->check  = 0;

		printf("f %p vs %p version=%d\n", ipptr, &packet.ip4_hdr, packet.ip4_hdr.version); fflush(stdout);
		printf("0x%08x 0x%08x 0x%08x\n", ((uint32_t*)ipptr)[0], ((uint32_t*)ipptr)[1], ((uint32_t*)ipptr)[2]);
		int bytes = write(fd_tun, ipptr,iplen);
		if (bytes < 0) {
			int errsv = errno;
			printf("write to tunnel failed: %s\n", strerror(errsv));
			close(fd_tun);
			return 2;
		}

		printf("Sent %d bytes (%d data-bytes) via tunnel!\n", bytes, len); fflush(stdout);
	}
	close(fd_tun);

	return 0;
}




