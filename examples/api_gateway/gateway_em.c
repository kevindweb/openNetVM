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

#include "onvm_flow_table.h"
#include "onvm_nflib.h"
#include "onvm_pkt_common.h"
#include "onvm_pkt_helper.h"

#include "api_gateway.h"

int epoll_fd = -1;

void
buffer(void) {
        uint32_t i = 0;
        struct pipe_fds *pipe;
        struct rte_hash *hash = buffer_map->hash;
        struct rte_ring *ring;
        struct onvm_ft_ipv4_5tuple *flow;

        for (; worker_keep_running;) {
                if (rte_atomic16_read(&num_running_containers) >= MAX_CONTAINERS) {
                        // cannot afford to add new container
                        perror("Max containers/flows hit");
                        continue;
                }

                // TODO: use buffer map to make sure we have new flows that need a container, sleep if not
                while (rte_hash_iterate(hash, (const void **)(&flow), (void **)(&ring), &i) >= 0 &&
                       rte_ring_dequeue(scale_buffer_ring, (void **)(&pipe)) == 0) {
                        /*
                         * New containers/pipes are ready and we have an unassigned IP flow
                         * Get the first available buffered flow (maybe random for fairness)
                         * De-buffer every packet from this flow and push to the tx pipe fd
                         * Add to flow table with <flow>:<struct container> (with pipe fds and other data)
                         * Add rx pipe to epoll for polling
                         */

                        // push all buffered packets to the pipe
                        if (dequeue_and_free_buffer_map(flow, ring, pipe->tx_pipe) < 0) {
                                perror("Failed to dequeue packet flows from ring");
                                continue;
                        }

                        struct data *data = NULL;
                        if (onvm_ft_add_key(em_tbl, flow, (char **)&data) < 0) {
                                perror("Couldn't add IP flow to flow table");
                                continue;
                        }

                        add_fd_epoll(pipe->rx_pipe);
                }

                // either we have no more flows, or no more warm containers, sleep
                usleep(10);
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
        int i;

        // epoll specific initializers
        int event_count;
        struct epoll_event events[MAX_CONTAINERS];

        // initialize epoll file descriptor for polling
        epoll_fd = epoll_create1(0);

        for (; worker_keep_running;) {
                // blocking wait on epoll for the file descriptors
                event_count = epoll_wait(epoll_fd, events, rte_atomic16_read(&num_running_containers), POLLING_TIMEOUT);
                for (i = 0; i < event_count; i++) {
                        // TODO: implement fairness for polling pipes
                        // read the container pipe buffer until its empty
                        while ((pkt = read_packet(events[i].data.fd)) != NULL) {
                                // enqueue rte_mbuf onto DPDK port
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

/* Helpers to manipulate epoll list */
int
add_fd_epoll(int fd) {
        if (fd < 0 || epoll_fd < 0) {
                // bad input
                return -1;
        }

        struct epoll_event *event = (struct epoll_event *)(malloc(sizeof(struct epoll_event)));
        event->events = EPOLLIN;
        if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, fd, event) != 0) {
                return -1;
        }

        // success, new container should be polled
        rte_atomic16_inc(&num_running_containers);
        return 0;
}

int
rm_fd_epoll(int fd) {
        if (fd < 0 || epoll_fd < 0 || rte_atomic16_read(&num_running_containers) > MAX_CONTAINERS) {
                // bad input
                return -1;
        }

        if (epoll_ctl(epoll_fd, EPOLL_CTL_DEL, fd, NULL) != 0) {
                return -1;
        }

        // successfullly removed container from epoll list - no need to track anymore
        rte_atomic16_dec(&num_running_containers);
        return 0;
}