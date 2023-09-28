#include "console.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sloop/loop.h>
#include <lwip/ip_addr.h>

#include "netif.h"
#include "legions.h"

static struct loop_watch console_watch = { 0 };

static void legions_command_handle_echo(int argc, char * const *argv) {
    ip_addr_t addr_first;
    ip_addr_t addr_last;
    uint32_t addr_iter;
    char *hyphen;
    uint16_t port = DEFAULT_PORT;

    if (argc >= 3) {
        port = atoi(argv[2]);
    }
    printf("Port: %d\n", (int)port);

    hyphen = strchr(argv[1], '-');
    if (hyphen) {
        *hyphen = '\0';
    }

    if (!ipaddr_aton(argv[1], &addr_first)) {
        fprintf(stderr, "Invalid address format.\n");
        return;
    }
    
    addr_last = addr_first;

    if (hyphen) {
        if (!ipaddr_aton(hyphen + 1, &addr_last)) {
            fprintf(stderr, "Invalid address format\n");
            return;
        }
    }

    addr_first.addr = lwip_ntohl(addr_first.addr);
    addr_last.addr = lwip_ntohl(addr_last.addr);

    printf("New echo instances: %d\n", (int)(addr_last.addr - addr_first.addr + 1));

    addr_iter = addr_first.addr;

    for (addr_iter = addr_first.addr; addr_iter <= addr_last.addr; addr_iter++) {
        ip_addr_t net_byte_order;
        net_byte_order.addr = lwip_htonl(addr_iter);

        struct legions_netif *netif = legions_netif_find(&net_byte_order);

        if (!netif) {
            netif = legions_netif_new(net_byte_order);
        }

        legions_netif_listen(netif, port);
    }
}

static void legions_command_handle_help(int argc, char * const *argv);

static struct command_handler {
    const char *name;
    void (*handler)(int argc, char * const *argv);
    const char *help;
} command_handlers[] = {
    { "echo", &legions_command_handle_echo, "echo <address>[-<address>] [<port>]\tbind an echo service on every address (addresses created as needed)" },
    { "help", &legions_command_handle_help, "help [<command>]\tdisplay help about commands" },
    { 0 }
};

static struct command_handler *legions_command_find(const char *name) {
    struct command_handler *h = command_handlers;

    while (h->name) {
        if (!strcmp(name, h->name))
            return h;
        h++;
    }

    return NULL;
}

static void legions_command_handle_help(int argc, char * const *argv) {
    if (argc > 1) {
        struct command_handler *h = legions_command_find(argv[1]);
        if (h) {
            if (h->help) {
                printf("%s\n", h->help);
            } else {
                printf("No help available for this command\n");
            }
        } else {
            printf("Unknown command\n");
        }
    } else {
        struct command_handler *h = command_handlers;
        while (h->name) {
            const char *s = h->help ? h->help : h->name;
            printf("%s\n", s);
            h++;
        }
    }
}

static void legions_command_handle(char *cmd_line) {
    static const unsigned int MAX_ARGS = 8;
    char *argv[MAX_ARGS];
    char *saveptr;
    char *tok;
    int argc = 0;

    tok = strtok_r(cmd_line, " ", &saveptr);
    while (tok && argc < MAX_ARGS) {
        argv[argc++] = tok;
        tok = strtok_r(NULL, " ", &saveptr);
    }

    if (argc > 0) {
        struct command_handler *h = legions_command_find(argv[0]);

        if (h) {
            h->handler(argc, argv);
        } else {
            fprintf(stderr, "Unknown command: %s\n", argv[0]);
        }
    }
    printf("> ");
    fflush(stdout);
}

    /*
    ip_addr_t n_addr = IPADDR4_INIT_BYTES(223, 42, 0, 2);
    struct legions_netif *n = legions_netif_new(n_addr);
    avl_insert(&netifs, &n->node);

    legions_netif_listen(n, 1234);
    */


static char cmd_line[256] = { 0 };
static unsigned int cmd_line_off = 0;

static void legions_console_watch_cb(struct loop_watch *watch, enum loop_io_event events) {
    if (events & EVENT_READ) {
        ssize_t res;
        char *eol;

        if (cmd_line_off == sizeof(cmd_line) - 1)
            cmd_line_off = 0;

        res = read(watch->fd, cmd_line + cmd_line_off, sizeof(cmd_line) - 1 - cmd_line_off);
        if (res <= 0) {
            loop_watch_set(watch, 0);
            loop_exit();
            return;
        }

        cmd_line_off += res;
        cmd_line[cmd_line_off] = '\0';

        while ((eol = strchr(cmd_line, '\n'))) {
            size_t cmd_line_len;

            *eol = '\0';
            legions_command_handle(cmd_line);

            cmd_line_len = (eol - cmd_line);

            memmove(cmd_line, eol + 1, cmd_line_off - cmd_line_len);
            cmd_line_off -= cmd_line_len + 1;
        }
    }
}

void console_register() {
    console_watch.fd = 0;
    console_watch.cb = &legions_console_watch_cb;
    loop_watch_set(&console_watch, EVENT_READ);

    legions_command_handle("");
}
	
