#include <linux/if_ether.h>

#include "../include/libnetoam.h"

/* Prototypes */
int oam_hwaddr_bin2str(uint8_t *binary_addr, char *string_mac);

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
