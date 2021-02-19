#include <errno.h>
#include <getopt.h>
#include <inttypes.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <rte_common.h>
#include <rte_cycles.h>
#include <rte_hash.h>
#include <rte_ip.h>
#include <rte_mbuf.h>

#include "api_gateway.h"
#include "onvm_flow_table.h"
#include "onvm_nflib.h"
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

void*
buffer(void* in) {
        struct packet_buf *pkts_deq_burst;
        struct packet_buf *pkts_enq_burst;
        int num_deq,i,dst;
        struct rte_mbuf *pkt;
        while (1) {
                num_deq = rte_ring_dequeue_burst(scale_buffer_ring, (void **)pkts_deq_burst->buffer, PACKET_READ_SIZE, NULL);
                if (num_deq == 0 && scaling_buf->count != 0) {
                        // TODO: Is a lock necessary here?
                        rte_ring_enqueue_burst(scale_buffer_ring, (void **)scaling_buf->buffer, PACKET_READ_SIZE, NULL);
                        scaling_buf->count = 0;
                } else {
                        for (i=0; i<num_deq; i++) {
                                pkt = pkts_deq_burst->buffer[i];
                                dst = get_ipv4_dst(pkt);
                                if (dst >= 0) {
                                        //send_to_pipe()
                                }
                                else {
                                        pkts_enq_burst->buffer[pkts_enq_burst->count++] = pkt;
                                }
                        }
                        rte_ring_enqueue_burst(scale_buffer_ring, (void **)pkts_enq_burs->buffer, PACKET_READ_SIZE, NULL);
                }
        }
        printf("Buffer thread exiting\n");
        return NULL;
}