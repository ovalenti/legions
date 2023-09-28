#include <stdio.h>

#include <sloop/loop.h>
#include <lwip/netif.h>
#include <lwip/init.h>

#include "console.h"
#include "tun.h"

int main(int argc, char **argv) {
    const char *device_name = NULL;

    if (argc > 1)
        device_name = argv[1];

    if (tun_register(device_name)) {
        return 1;
    }
    console_register();
  
    lwip_init();

    loop_run();

    printf("exit.\n");

    return 0;
}
