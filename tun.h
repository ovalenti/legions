#ifndef TUN_H
#define TUN_H

#include <lwip/netif.h>

int tun_register(const char *device_name);

err_t tun_send_packet(struct netif *netif, struct pbuf *p, const ip4_addr_t *ipaddr); 

#endif
