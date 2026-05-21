#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/ioctl.h>  
#include <netinet/ether.h>
#include <net/if.h>      
#include <linux/if_packet.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <errno.h>

/**
 * struct eth_header - Standard Ethernet II header
 * @dst_mac: Destination MAC address (6 bytes)
 * @src_mac: Source MAC address (6 bytes)
 * @eth_type: EtherType field (network byte order)
 */
struct eth_header {
    uint8_t  dst_mac[6];
    uint8_t  src_mac[6];
    uint16_t eth_type;
};

/**
 * parse_mac - Converts a MAC address string to a byte array
 * @mac_str: String representation (e.g., "00:11:22:33:44:55")
 * @mac: Output buffer for the 6-byte array
 */
void parse_mac(const char *mac_str, uint8_t *mac) {
    for (int i = 0; i < 6; i++) {
        sscanf(mac_str + i * 3, "%2hhx", &mac[i]);
    }
}

int main(int argc, char *argv[]) {

    /* Verify root privileges required for raw socket operations */
    if (getuid() != 0) {
        fprintf(stderr, "Error: Root privileges required for raw socket operations.\n");
        return EXIT_FAILURE;
    }

    /* Configuration: Update these values as per your network environment */
    const char *interface = "wlp0s20f3";             /* Target network interface name */
    const char *dst_mac_str = "77:02:03:04:05:06";   /* Destination hardware address */
    const char *ethtype = "0x8888";                  /* Custom EtherType for the frame */
    unsigned int eth_type = (unsigned int)strtoul(ethtype, NULL, 16);
    const char *payload = "HELLO WORLD FROM LAYER 2 ETHERNET";

    size_t payload_len = strlen(payload);

    /* Open a raw socket to send Ethernet frames */
    int sockfd = socket(AF_PACKET, SOCK_RAW, ETH_P_ALL);
    if (sockfd < 0) {
        perror("socket");
        exit(EXIT_FAILURE);	
    }

    /* Retrieve the interface index for the specified interface name */
    struct ifreq if_idx;
    memset(&if_idx, 0, sizeof(if_idx));
    strncpy(if_idx.ifr_name, interface, IFNAMSIZ - 1);
    if (ioctl(sockfd, SIOCGIFINDEX, &if_idx) < 0) {
        perror("SIOCGIFINDEX");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    /* Retrieve the hardware (MAC) address of the source interface */
    struct ifreq if_mac;
    memset(&if_mac, 0, sizeof(if_mac));
    strncpy(if_mac.ifr_name, interface, IFNAMSIZ - 1);
    if (ioctl(sockfd, SIOCGIFHWADDR, &if_mac) < 0) {
        perror("SIOCGIFHWADDR");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    /* Initialize and populate the Ethernet header */
    struct eth_header eth;
    parse_mac(dst_mac_str, eth.dst_mac);
    memcpy(eth.src_mac, if_mac.ifr_hwaddr.sa_data, 6);
    eth.eth_type = htons(eth_type);

    /* Allocate buffer to hold the complete Ethernet frame (Header + Payload) */
    size_t frame_len = sizeof(struct eth_header) + payload_len;
    uint8_t *frame = malloc(frame_len);

    if (!frame) {
        perror("malloc");
        close(sockfd);
        exit(EXIT_FAILURE);
    }
	
    /* Assemble the frame by copying header and payload into the buffer */
    memcpy(frame, &eth, sizeof(struct eth_header));
    memcpy(frame + sizeof(struct eth_header), payload, payload_len);

    /* Prepare the link-layer destination address structure */
    struct sockaddr_ll sockaddr;
    memset(&sockaddr, 0, sizeof(sockaddr));
    sockaddr.sll_family = AF_PACKET;
    sockaddr.sll_protocol = htons(eth_type);
    sockaddr.sll_ifindex = if_idx.ifr_ifindex;
    sockaddr.sll_halen = 6;
    memcpy(sockaddr.sll_addr, eth.dst_mac, 6);

    /* Transmit the raw Ethernet frame */
    if (sendto(sockfd, frame, frame_len, 0, (struct sockaddr *)&sockaddr, sizeof(sockaddr)) < 0) {
        perror("sendto");
        free(frame);
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    printf("Successfully sent %zu-byte frame on %s\n", frame_len, interface);
    printf("EtherType: 0x%04X, Destination MAC: %s\n", eth_type, dst_mac_str);

    /* Clean up allocated resources and close the socket */
    free(frame);
    close(sockfd);
    return EXIT_SUCCESS;
}