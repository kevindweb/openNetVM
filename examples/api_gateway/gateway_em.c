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

const char *
get_cont_rx_queue_name(unsigned id) {
        /* buffer for return value. Size calculated by %u being replaced
         * by maximum 3 digits (plus an extra byte for safety) */
        static char buffer[sizeof(CONT_NF_RXQ_NAME) + 4];
        sprintf(buffer, CONT_NF_RXQ_NAME, id);
        return buffer;
}

const char *
get_cont_tx_queue_name(unsigned id) {
        /* buffer for return value. Size calculated by %u being replaced
         * by maximum 3 digits (plus an extra byte for safety) */
        static char buffer[sizeof(CONT_NF_TXQ_NAME) + 4];
        sprintf(buffer, CONT_NF_TXQ_NAME, id);
        return buffer;
}

void
buffer() {
        struct packet_buf *pkts_deq_burst;
        struct packet_buf *pkts_enq_burst;
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
pkt_to_mbuf(struct rte_mbuf *pkt) {
        // find which port to send packet to
        uint16_t dst;
        dst = get_ipv4_dst(pkt);

        onvm_pkt_enqueue_port(nf_local_ctx->nf->nf_tx_mgr, dst, pkt);
}

/* thread to continuously poll all tx_fds from containers for packets to send out to network */
void
polling() {
        struct rte_mbuf *pkt;
        void *next_fd;
        int num_containers = 0;
        int i;

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
                                pkt_to_mbuf(pkt);
                        }
                }
        }

        if (close(epoll_fd)) {
                perror("Couldn't close epoll file descriptor\n");
                return;
        }

        printf("Polling thread exiting\n");
}