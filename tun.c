#include "tun.h"

#include <sys/ioctl.h>
#include <sys/types.h>
#include <fcntl.h>
#include <linux/if.h>
#include <linux/if_tun.h>
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

#include "legions.h"
#include "netif.h"

static struct loop_watch tun_watch = { 0 };
static LIST_HEAD(send_queue);

struct send_queue_buf {
    struct list_head list;
    struct pbuf *p;
};

static int open_tap_socket(const char *name) {
    struct ifreq ifr;
    int fd, err;

    fd = open("/dev/net/tun", O_RDWR);
    if (fd < 0) {
        perror("open(/dev/net/tun");
        return -1;
    }

    memset(&ifr, 0, sizeof(ifr));

        ifr.ifr_flags = IFF_TUN | IFF_NO_PI;

    if (name == NULL) {
        name = "tun0";
    }

    strncpy(ifr.ifr_name, name, IFNAMSIZ);

    err = ioctl(fd, TUNSETIFF, (void *) &ifr);
    if (err < 0) {
        close(fd);
        perror("TUNSETIFF");
        return -1;
    }
    return fd;
}

static void legions_tun_watch_set() {
    enum loop_io_event events = EVENT_READ;

    if (!list_empty(&send_queue)) {
        events |= EVENT_WRITE;
    }

    loop_watch_set(&tun_watch, events);
}

err_t tun_send_packet(struct netif *netif, struct pbuf *p, const ip4_addr_t *ipaddr) {
    struct send_queue_buf *b;

    b = (struct send_queue_buf *)malloc(sizeof(*b));

    b->p = p;
    pbuf_ref(p);

    list_add_tail(&b->list, &send_queue);

    legions_tun_watch_set();

    return ERR_OK;
}

static void legions_tun_watch_cb(struct loop_watch *watch, enum loop_io_event events) {
    if (events & EVENT_READ) {
        unsigned char packet[MTU];
        ssize_t res;
        err_t err_code;
        struct pbuf *p;
        struct avl_node *node;
        struct legions_netif *target_netif;
        ip_addr_t *dest_addr;

        res = read(watch->fd, packet, sizeof(packet));
        if (res < 0) {
            perror("read");
            loop_exit();
            return;
        }

        if (res < sizeof(struct ip_hdr)) {
            fprintf(stderr, "drop: short packet\n");
            return;
        }

        // should be fine since IP headers are aligned.
        dest_addr = (ip_addr_t*)&((struct ip_hdr *)packet)->dest;

        p = pbuf_alloc(PBUF_IP, res, PBUF_RAM);
        pbuf_take(p, packet, res);

        target_netif = legions_netif_find(dest_addr);

        if (target_netif == NULL) {
            fprintf(stderr, "drop: unknown destination address\n");
            return;
        }

        err_code = legions_netif_input(p, target_netif);

        if (err_code != ERR_OK) {
            fprintf(stderr, "packet rejected: %s\n", lwip_strerr(err_code));
            pbuf_free(p);
        }
    }

    if (events & EVENT_WRITE && !list_empty(&send_queue)) {
        unsigned char packet_buffer[MTU];
        void *packet;
        ssize_t res;
        struct send_queue_buf *buffer;

        buffer = list_first_entry(&send_queue, struct send_queue_buf, list);
        list_del(send_queue.next);

        packet = pbuf_get_contiguous(buffer->p, packet_buffer,sizeof(packet_buffer), buffer->p->tot_len, 0);
        res = write(tun_watch.fd, packet, buffer->p->tot_len);
        
        if (res < 0) {
            perror("write");
        }

        pbuf_free(buffer->p);
        free(buffer);
        
        legions_tun_watch_set();
    }
}

int tun_register(const char *device_name) {
    tun_watch.fd = open_tap_socket(device_name);
    if (tun_watch.fd < 0)
        return 1;

    tun_watch.cb = &legions_tun_watch_cb;

    legions_tun_watch_set();
    return 0;
}

 
