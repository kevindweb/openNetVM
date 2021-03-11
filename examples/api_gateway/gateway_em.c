#include <errno.h>
#include <getopt.h>
#include <inttypes.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <unistd.h>

#include <rte_common.h>
#include <rte_cycles.h>
#include <rte_hash.h>
#include <rte_ip.h>
#include <rte_malloc.h>
#include <rte_mbuf.h>

#include "api_gateway.h"
#include "onvm_flow_table.h"
#include "onvm_nflib.h"
#include "onvm_pkt_common.h"
#include "onvm_pkt_helper.h"
#include "onvm_table_parser.h"

uint16_t
get_ipv4_dst(struct rte_mbuf *pkt) {
        struct data *data = NULL;
        struct onvm_ft_ipv4_5tuple key;
        uint8_t dst;

        int ret = onvm_ft_fill_key(&key, pkt);

        if (ret < 0)
                return -1;

        // TODO: flow table should return more info about packet (like container id, pipe file descriptors...)
        int tbl_index = onvm_ft_lookup_key(em_tbl, &key, (char **)&data);
        if (tbl_index < 0)
                return -1;
        dst = data->dest;
        return dst;
}

int
setup_hash(struct state_info *stats) {
        em_tbl = onvm_ft_create(HASH_ENTRIES, sizeof(struct data));
        if (em_tbl == NULL) {
                printf("Unable to create flow table");
                return -1;
        }
        add_rules(em_tbl, "ipv4_rules_file.txt", stats->print_keys, ONVM_TABLE_EM);
        return 0;
}

void
buffer(void) {
        int num_deq, i, dst;
        struct rte_mbuf *pkt;
        for (; worker_keep_running;) {
                num_deq =
                    rte_ring_dequeue_burst(gate_buffer_ring, (void **)pkts_deq_burst->buffer, PACKET_READ_SIZE, NULL);
                if (num_deq == 0 && scaling_buf->count != 0) {
                        // TODO: Add a rte_atomic on scaling_buf->count
                        rte_ring_enqueue_burst(gate_buffer_ring, (void **)scaling_buf->buffer, PACKET_READ_SIZE, NULL);
                        scaling_buf->count = 0;
                } else {
                        for (i = 0; i < num_deq; i++) {
                                pkt = pkts_deq_burst->buffer[i];
                                dst = get_ipv4_dst(pkt);
                                if (dst >= 0) {
                                        // send_to_pipe()
                                } else {
                                        pkts_enq_burst->buffer[pkts_enq_burst->count++] = pkt;
                                }
                        }
                        rte_ring_enqueue_burst(gate_buffer_ring, (void **)pkts_enq_burst->buffer, PACKET_READ_SIZE,
                                               NULL);
                }
        }

        printf("Buffer thread exiting\n");
}

/* Turn a container packet into a dpdk rte_mbuf pkt */
void
enqueue_mbuf(struct rte_mbuf *pkt) {
        // find which port to send packet to
        uint16_t dst;
        dst = get_ipv4_dst(pkt);
        if (dst == 0) {
                // TODO: figure out if this is a malicious scenario
                perror("Red flag: container sent packet we couldn't handle\n");
                return;
        }

        // enqueue pkt directly (instead of normal NF tx ring)
        onvm_pkt_enqueue_port(poll_tx_mgr, dst, pkt);
}

/*
 * Create a queue_mgr (replacing manager TX thread)
 * see onvm/onvm_mgr/main.c for example creating queue_mgr
 */
struct queue_mgr *
create_tx_poll_mgr(void) {
        struct queue_mgr *poll_mgr = rte_calloc(NULL, 1, sizeof(struct queue_mgr), RTE_CACHE_LINE_SIZE);
        if (poll_mgr == NULL) {
                return NULL;
        }
        poll_mgr->mgr_type_t = MGR;
        poll_mgr->id = 0;
        poll_mgr->tx_thread_info = NULL;
        poll_mgr->nf_rx_bufs = NULL;
        return poll_mgr;
}

/* thread to continuously poll all tx_fds from containers for packets to send out to network */
void
polling(void) {
        struct rte_mbuf *pkt;
        void *next_fd;
        int num_containers = 0;
        int i;

        pkt = malloc(sizeof(struct rte_mbuf));

        // epoll specific initializers
        int epoll_fd;
        int event_count;
        struct epoll_event events[MAX_CONTAINERS];

        // initialize epoll file descriptor for polling
        epoll_fd = epoll_create1(0);

        for (; worker_keep_running;) {
                // check for new TX fds to add to epoll
                while (rte_ring_dequeue(scale_poll_add_ring, &next_fd) >= 0) {
                        // continuously add fds to epoll
                        struct epoll_event *event = (struct epoll_event *)(malloc(sizeof(struct epoll_event)));
                        event->events = EPOLLIN;
                        epoll_ctl(epoll_fd, EPOLL_CTL_ADD, *(int *)next_fd, event);

                        // make sure we know how many pipes we're tracking
                        num_containers++;
                }

                while (rte_ring_dequeue(scale_poll_del_ring, &next_fd) >= 0) {
                        // remove file descriptor to stop listening to that pipe
                        epoll_ctl(epoll_fd, EPOLL_CTL_DEL, *(int *)next_fd, NULL);

                        num_containers--;
                }

                if (unlikely(num_containers > MAX_CONTAINERS)) {
                        perror("We have too many containers, overflow will occur");
                        break;
                }

                event_count = epoll_wait(epoll_fd, events, num_containers, POLLING_TIMEOUT);
                for (i = 0; i < event_count; i++) {
                        // TODO: implement fairness for polling pipes
                        // read the container pipe buffer until its empty
                        while (read(events[i].data.fd, pkt, sizeof(pkt)) != -1) {
                                // convert packet to rte_mbuf and insert into buffer
                                enqueue_mbuf(pkt);
                        }
                }
        }

        if (close(epoll_fd)) {
                perror("Couldn't close epoll file descriptor\n");
                return;
        }

        printf("Polling thread exiting\n");
}
