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

const char cb_port_delim[] = ":";

int
get_cb_field(char **in, uint32_t *fd, int base, unsigned long lim, char dlm) {
        unsigned long val;
        char *end;

        errno = 0;
        val = strtoul(*in, &end, base);
        if (errno != 0 || end[0] != dlm || val > lim)
                return -EINVAL;
        *fd = (uint32_t)val;
        *in = end + 1;
        return 0;
}

int
parse_ipv4_net(char *in, uint32_t *addr, uint32_t *depth) {
        uint32_t a, b, c, d, m;

        if (get_cb_field(&in, &a, 0, UINT8_MAX, '.'))
                return -EINVAL;
        if (get_cb_field(&in, &b, 0, UINT8_MAX, '.'))
                return -EINVAL;
        if (get_cb_field(&in, &c, 0, UINT8_MAX, '.'))
                return -EINVAL;
        if (get_cb_field(&in, &d, 0, UINT8_MAX, '/'))
                return -EINVAL;
        if (get_cb_field(&in, &m, 0, sizeof(uint32_t) * CHAR_BIT, 0))
                return -EINVAL;
        addr[0] = RTE_IPV4(a, b, c, d);
        depth[0] = m;
        return 0;
}

/* This function fills the key and return the destination or action to be stored in the table entry.*/
int
parse_ipv4_5tuple_rule(char *str, struct onvm_parser_ipv4_5tuple *ipv4_tuple) {
        int i, ret;
        char *s, *sp, *in[CB_FLD_NUM];
        static const char *dlm = " \t\n";
        int dim = CB_FLD_NUM;
        uint32_t temp;

        struct onvm_ft_ipv4_5tuple *key = &ipv4_tuple->key;
        s = str;
        for (i = 0; i != dim; i++, s = NULL) {
                in[i] = strtok_r(s, dlm, &sp);
                if (in[i] == NULL)
                        return -EINVAL;
        }

        ret = parse_ipv4_net(in[CB_FLD_SRC_ADDR], &key->src_addr, &ipv4_tuple->src_addr_depth);
        if (ret < 0) {
                return ret;
        }

        ret = parse_ipv4_net(in[CB_FLD_DST_ADDR], &key->dst_addr, &ipv4_tuple->dst_addr_depth);
        if (ret < 0) {
                return ret;
        }

        if (strncmp(in[CB_FLD_SRC_PORT_DLM], cb_port_delim, sizeof(cb_port_delim)) != 0)
                return -EINVAL;

        if (get_cb_field(&in[CB_FLD_SRC_PORT], &temp, 0, UINT16_MAX, 0))
                return -EINVAL;
        key->src_port = (uint16_t)temp;

        if (get_cb_field(&in[CB_FLD_DST_PORT], &temp, 0, UINT16_MAX, 0))
                return -EINVAL;
        key->dst_port = (uint16_t)temp;

        if (strncmp(in[CB_FLD_SRC_PORT_DLM], cb_port_delim, sizeof(cb_port_delim)) != 0)
                return -EINVAL;

        if (get_cb_field(&in[CB_FLD_PROTO], &temp, 0, UINT8_MAX, 0))
                return -EINVAL;
        key->proto = (uint8_t)temp;

        if (get_cb_field(&in[CB_FLD_DEST], &temp, 0, UINT16_MAX, 0))
                return -EINVAL;

        // Return the destination or action to be performed by the NF.
        return (uint16_t)temp;
}

/* Bypass comment and empty lines */
int
is_bypass_line(char *buff) {
        int i = 0;

        /* comment line */
        if (buff[0] == COMMENT_LEAD_CHAR)
                return 1;
        /* empty line */
        while (buff[i] != '\0') {
                if (!isspace(buff[i]))
                        return 0;
                i++;
        }
        return 1;
}

int
add_rules(void *tbl, const char *rule_path, uint8_t print_keys, int table_type) {
        FILE *fh;
        char buff[LINE_MAX];
        unsigned int i = 0;
        struct onvm_parser_ipv4_5tuple ipv4_tuple;
        int ret;
        fh = fopen(rule_path, "rb");
        if (fh == NULL)
                rte_exit(EXIT_FAILURE, "%s: fopen %s failed\n", __func__, rule_path);

        ret = fseek(fh, 0, SEEK_SET);
        if (ret)
                rte_exit(EXIT_FAILURE, "%s: fseek %d failed\n", __func__, ret);
        i = 0;
        while (fgets(buff, LINE_MAX, fh) != NULL) {
                i++;

                if (is_bypass_line(buff))
                        continue;

                uint8_t dest = parse_ipv4_5tuple_rule(buff, &ipv4_tuple);
                // TODO: fix error with comparison
                // error: comparison is always false due to limited range of data type [-Werror=type-limits]
                // if (dest < 0)
                //         rte_exit(EXIT_FAILURE, "%s Line %u: parse rules error\n", rule_path, i);

                struct data *data = NULL;
                int tbl_index = -EINVAL;
                if (table_type == ONVM_TABLE_EM) {
                        tbl_index = onvm_ft_add_key((struct onvm_ft *)tbl, &ipv4_tuple.key, (char **)&data);
                } else if (table_type == ONVM_TABLE_LPM) {
                        // Adds to the lpm table using the src ip adress.
                        tbl_index = rte_lpm_add((struct rte_lpm *)tbl, ipv4_tuple.key.src_addr,
                                                ipv4_tuple.src_addr_depth, dest);
                }
                data->dest = dest;
                if (tbl_index < 0)
                        rte_exit(EXIT_FAILURE, "Unable to add entry %u\n", i);
                if (print_keys) {
                        printf("\nAdding key:");
                        _onvm_ft_print_key(&ipv4_tuple.key);
                }
        }

        fclose(fh);
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
