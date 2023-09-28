#ifndef NETIF_H
#define NETIF_H

#include <lwip/err.h>
#include <lwip/ip_addr.h>
#include <lwip/pbuf.h>

struct legions_netif;

struct legions_netif *legions_netif_new(ip_addr_t addr);
void legions_netif_listen(struct legions_netif *n, uint32_t port);

struct legions_netif *legions_netif_find(ip_addr_t *addr);
err_t legions_netif_input(struct pbuf *p, struct legions_netif *netif);

#endif
