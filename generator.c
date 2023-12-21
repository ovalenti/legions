#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>
#include <errno.h>

#include <sloop/loop.h>

static int conn_rate;
static int conn_nb;
static uint32_t range_base;
static uint32_t range_end;
static int port;

static void usage(const char *prg_name) {
	fprintf(stderr, "%s <conn-rate> <conn-nb> <ip-range-start> <ip-range-end> <port>\n"
			"\t<conn-rate>\tNumber of new connections per second.\n"
			"\t<conn-nb>\tThe target number of connections\n",
			prg_name);
}

static int parse_parameters(int argc, char **argv) {
	struct in_addr addr;

	if (argc != 6)
		return 1;

	conn_rate = atoi(argv[1]);
	conn_nb = atoi(argv[2]);

	if (!inet_aton(argv[3], &addr))
		return 1;

	range_base = ntohl(addr.s_addr);

	if (!inet_aton(argv[4], &addr))
		return 1;

	range_end = ntohl(addr.s_addr);

	port = atoi(argv[5]);

	return 0;
}

struct connection {
	uint32_t addr;
	struct loop_watch watch;
	struct list_head active;
	struct list_head list;
};

LIST_HEAD(connections);
LIST_HEAD(active_connections);

static int nb_active_connections = 0;
static uint32_t next_ip;

static void connection_becomes_active(struct connection *c) {
	list_add(&c->active, &active_connections);
	nb_active_connections++;
}

static void connection_destroy(struct connection *c) {
	loop_watch_set(&c->watch, 0);

	if (c->watch.fd >= 0)
		close(c->watch.fd);

	if (!list_empty(&c->active)) {
		nb_active_connections--;
		list_del(&c->active);
	}

	list_del(&c->list);

	free(c);
}

static void connection_watch_cb(struct loop_watch *w, enum loop_io_event events) {
	struct connection *c = container_of(w, struct connection, watch);

	if (events & EVENT_WRITE) {
		int error;
		socklen_t len = sizeof(error);
		getsockopt(w->fd, SOL_SOCKET, SO_ERROR, &error, &len);
		if (error == 0) {
			connection_becomes_active(c);
		} else {
			fprintf(stderr, "socket error=%d (%d.%d.%d.%d)\n", error,
				       (int)(c->addr >> 24),
				       (int)(c->addr >> 16) & 0xFF,
				       (int)(c->addr >> 8) & 0xFF,
				       (int)(c->addr) & 0xFF);
		}
		loop_watch_set(w, 0);
	}
}

static void every_second_cb(struct loop_timeout *t) {
	int i;
	loop_timeout_add(t, 1000);

	printf("Active connections: %d\n", nb_active_connections);
	// Create connections to reach the expected rate.
	for (i = 0; i < conn_rate; i++) {
		struct connection *c = (struct connection *)calloc(1, sizeof(*c));
		struct sockaddr_in sockaddr;

		list_add(&c->list, &connections);
		INIT_LIST_HEAD(&c->active);

		c->addr = next_ip++;
		if (next_ip == range_end)
			next_ip = range_base;

		c->watch.fd = socket(AF_INET, SOCK_STREAM, 0);
		if (c->watch.fd < 0)
			perror("socket");

		c->watch.cb = &connection_watch_cb;

		fcntl(c->watch.fd, F_SETFL, fcntl(c->watch.fd, F_GETFL) | O_NONBLOCK);

		sockaddr.sin_family = AF_INET;
		sockaddr.sin_addr.s_addr = htonl(c->addr);
		sockaddr.sin_port = htons(port);

		if (connect(c->watch.fd, (struct sockaddr *)&sockaddr, sizeof(sockaddr))) {
			if (errno == EINPROGRESS) {
				// wait for connection to be established
				loop_watch_set(&c->watch, EVENT_WRITE);
			} else {
				perror("connect");
			}
		} else {
			// the connection is already established
			connection_becomes_active(c);
		}
	}

	// Drop as many connection as needed to stay at the expected number
	while (nb_active_connections > conn_nb) {
		connection_destroy(container_of(active_connections.prev, struct connection, active));
	}
}

int main(int argc, char **argv) {
	struct loop_timeout every_second = { 0 };

	if (parse_parameters(argc, argv)) {
		usage(argv[0]);
		return 1;
	}

	signal(SIGPIPE, SIG_IGN);

	next_ip = range_base;

	every_second.cb = &every_second_cb;
	loop_timeout_add(&every_second, 1000);

	loop_run();

	return 0;
}
