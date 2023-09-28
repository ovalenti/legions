#include "netif.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <sloop/loop.h>
#include <sloop/avl.h>
#include <lwip/init.h>
#include <lwip/netif.h>
#include <lwip/tcp.h>
#include <lwip/prot/ip4.h>

#include "tun.h"
#include "legions.h"

struct legions_netif {
    struct avl_node node;
    struct netif netif;
};

static int addr_comp(const void *k1, const void *k2, void *ptr) {
    return memcmp(k1, k2, sizeof(ip_addr_t));
}

static AVL_TREE(netifs, &addr_comp, 0, NULL);

struct legions_netif *legions_netif_find(ip_addr_t *addr) {
    struct avl_node *node;
    struct legions_netif *target_netif = NULL;
    
    node = avl_find(&netifs, addr);
    
    if (node) {
        target_netif = container_of(node, struct legions_netif, node);
    }

    return target_netif;
}

err_t legions_netif_input(struct pbuf *p, struct legions_netif *netif) {
    return netif->netif.input(p, &netif->netif);
}

static err_t legions_netif_init_cb(struct netif *netif) {
    netif->mtu = MTU;
    netif->output = &tun_send_packet;
    netif_set_up(netif);
    netif_set_link_up(netif);

    return ERR_OK;
}

struct legions_netif *legions_netif_new(ip_addr_t addr) {
    struct legions_netif *n;

    n = (struct legions_netif *)malloc(sizeof(struct legions_netif));
    memset(n, 0, sizeof(struct legions_netif));

    netif_add(&n->netif, &addr, NULL, NULL, NULL, &legions_netif_init_cb, &netif_input);

    n->node.key = (void *)&n->netif.ip_addr;

    avl_insert(&netifs, &n->node);

    return n;
}

static err_t legions_netif_recv_cb(void *arg, struct tcp_pcb *pcb, struct pbuf *pbuf, err_t err) {
    if (pbuf == NULL) {
        // connection closed
        tcp_shutdown(pcb, 0, 1);
        return ERR_OK;
    }

    if (err != ERR_OK) {
        fprintf(stderr, "error receiving data: %s\n", lwip_strerr(err));
        tcp_abort(pcb);
        return ERR_ABRT;
    }

    tcp_recved(pcb, pbuf->tot_len);
    struct pbuf *p = pbuf;
    while (p) {
        tcp_write(pcb, p->payload, p->len, TCP_WRITE_FLAG_COPY);
        p = p->next;
    }
    tcp_output(pcb);

    pbuf_free(pbuf);

    return ERR_OK;
}

static err_t legions_netif_accept_cb(void *arg, struct tcp_pcb *newpcb, err_t err) {
    // struct legions_netif *n = (struct legions_netif *)arg;

    if (err != ERR_OK) {
        fprintf(stderr, "error accepting connection: %s\n", lwip_strerr(err));
        return ERR_OK;
    }

    tcp_recv(newpcb, &legions_netif_recv_cb);

    tcp_write(newpcb, "Hello !\n", 8, 0);
    tcp_output(newpcb);

    return ERR_OK;
}

void legions_netif_listen(struct legions_netif *n, uint32_t port) {
    struct tcp_pcb *pcb = tcp_new();
    err_t err;

    tcp_bind_netif(pcb, &n->netif); 
    err = tcp_bind(pcb, &n->netif.ip_addr, port);

    if (err != ERR_OK) {
        fprintf(stderr, "tcp_bind: %s\n", lwip_strerr(err));
        return;
    }
    
    pcb = tcp_listen(pcb);

    tcp_arg(pcb, n);
    tcp_accept(pcb, &legions_netif_accept_cb);
}
