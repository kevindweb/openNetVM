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
get_ipv4_dst(struct rte_mbuf *pkt, struct state_info *stats) {
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
nf_cont_init_rings(struct container_nf *nf) {
        unsigned instance_id;
        unsigned socket_id;
        const char *rq_name;
        const char *tq_name;
        const unsigned ringsize = NF_QUEUE_RINGSIZE;

        instance_id = nf->instance_id;
        socket_id = rte_socket_id();
        rq_name = get_cont_rx_queue_name(instance_id);
        tq_name = get_cont_tx_queue_name(instance_id);
        nf->rx_q =
                rte_ring_create(rq_name, ringsize, socket_id, RING_F_SC_DEQ); /* multi prod, single cons */
        nf->tx_q =
                rte_ring_create(tq_name, ringsize, socket_id, RING_F_SC_DEQ); /* multi prod, single cons */

        if (nf->rx_q == NULL)
                rte_exit(EXIT_FAILURE, "Cannot create rx ring queue for NF %u\n", instance_id);

        if (nf->tx_q == NULL)
                rte_exit(EXIT_FAILURE, "Cannot create tx ring queue for NF %u\n", instance_id);
}