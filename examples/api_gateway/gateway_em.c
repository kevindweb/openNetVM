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
