#include <linux/if_ether.h>
#include <sys/ioctl.h>

#include "../include/libnetoam.h"

/* Prototypes */
int oam_hwaddr_bin2str(uint8_t *binary_addr, char *string_mac);
int oam_set_if(const char *ifname, int state);

int oam_hwaddr_bin2str(uint8_t *binary_addr, char *string_mac)
{
    for (int i = 0; i < ETH_ALEN; i++) {
        snprintf(string_mac + (i * 3), 3, "%02x", binary_addr[i]);

        if (i < ETH_ALEN - 1) {
            string_mac[i * 3 + 2] = ':';
        }
    }
    string_mac[ETH_ALEN * 3 - 1] = '\0';

    return 0;
}

int oam_set_if(const char *ifname, int state)
{
    int sockfd;
    struct ifreq ifr;

    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        perror("socket");
        return -1;
    }

    memset(&ifr, 0, sizeof(ifr));
    strncpy(ifr.ifr_name, ifname, IFNAMSIZ - 1);

    if (ioctl(sockfd, SIOCGIFFLAGS, &ifr) < 0) {
        perror("ioctl(SIOCGIFFLAGS)");
        close(sockfd);
        return -1;
    }

    if (state) {
        ifr.ifr_flags |= IFF_UP;
    } else {
        ifr.ifr_flags &= ~IFF_UP;
    }

    if (ioctl(sockfd, SIOCSIFFLAGS, &ifr) < 0) {
        perror("ioctl(SIOCSIFFLAGS)");
        close(sockfd);
        return -1;
    }

    close(sockfd);
    return 0;
}
