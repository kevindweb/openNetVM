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

#define IPV4_L3FWD_EM_NUM_ROUTES 4
#define HASH_ENTRIES 100

struct ipv4_em_route {
    struct onvm_ft_ipv4_5tuple key;
    uint8_t if_out;
};

// src_addr, dst_addr, src_port, dst_port, proto
struct ipv4_em_route ipv4_l3fwd_em_route_array[] = {
    {{RTE_IPV4(101, 0, 0, 0), RTE_IPV4(100, 10, 0, 1),  0, 1, IPPROTO_TCP}, 0},
    {{RTE_IPV4(201, 0, 0, 0), RTE_IPV4(200, 20, 0, 1),  1, 0, IPPROTO_TCP}, 1},
    {{RTE_IPV4(111, 0, 0, 0), RTE_IPV4(100, 30, 0, 1),  0, 2, IPPROTO_TCP}, 2},
    {{RTE_IPV4(211, 0, 0, 0), RTE_IPV4(200, 40, 0, 1),  2, 0, IPPROTO_TCP}, 3},
};

/* Struct that holds info about each flow, and is stored at each flow table entry. */
struct data {
    uint8_t  if_out;
};

static inline void
populate_ipv4_table(struct onvm_ft *h) {
    uint32_t i;
    int32_t ret;

    for (i = 0; i < IPV4_L3FWD_EM_NUM_ROUTES; i++) {
        struct ipv4_em_route entry;
        union ipv4_5tuple_host newkey;
        struct data *data = NULL;

        entry = ipv4_l3fwd_em_route_array[i];
        int tbl_index = onvm_ft_add_key(h, &entry.key, (char **)&data);
        data->if_out = entry.if_out;
        if (tbl_index < 0)
            rte_exit(EXIT_FAILURE, "Unable to add entry %u\n", i);
        printf("\nAdding key:");
        _onvm_ft_print_key(&entry.key);
    }
    printf("Hash: Adding 0x%" PRIx64 " keys\n",
        (uint64_t)IPV4_L3FWD_EM_NUM_ROUTES);
}

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
    dst = data->if_out;
    return dst;
}

int
setup_hash(struct state_info *stats) {
    em_tbl = onvm_ft_create(HASH_ENTRIES, sizeof(struct ipv4_em_route));
    if (em_tbl == NULL) {
            printf("Unable to create flow table");
            return -1;
    }
    populate_ipv4_table(em_tbl);
    return 0;
}

const char cb_port_delim[] = ":";

int
get_cb_field(char **in, uint32_t *fd, int base, unsigned long lim,
		char dlm)
{
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
parse_ipv4_net(char *in, uint32_t *addr)
{
	uint32_t a, b, c, d, m;

	if (get_cb_field(&in, &a, 0, UINT8_MAX, '.'))
		return -EINVAL;
	if (get_cb_field(&in, &b, 0, UINT8_MAX, '.'))
		return -EINVAL;
	if (get_cb_field(&in, &c, 0, UINT8_MAX, '.'))
		return -EINVAL;

	addr[0] = RTE_IPV4(a, b, c, d);
	return 0;
}

int
parse_ipv4_5tuple_rule(char *str, struct rte_eth_ntuple_filter *ntuple_filter)
{
	int i, ret;
	char *s, *sp, *in[CB_FLD_NUM];
	static const char *dlm = " \t\n";
	int dim = CB_FLD_NUM;
	uint32_t temp;

	s = str;
	for (i = 0; i != dim; i++, s = NULL) {
		in[i] = strtok_r(s, dlm, &sp);
		if (in[i] == NULL)
			return -EINVAL;
	}

	ret = parse_ipv4_net(in[CB_FLD_SRC_ADDR],
			&ntuple_filter->src_ip);

	ret = parse_ipv4_net(in[CB_FLD_DST_ADDR],
			&ntuple_filter->dst_ip);
	
	if (strncmp(in[CB_FLD_SRC_PORT_DLM], cb_port_delim,
			sizeof(cb_port_delim)) != 0)
		return -EINVAL;

	if (get_cb_field(&in[CB_FLD_SRC_PORT], &temp, 0, UINT16_MAX, 0))
		return -EINVAL;
	ntuple_filter->src_port = (uint16_t)temp;

	if (get_cb_field(&in[CB_FLD_DST_PORT], &temp, 0, UINT16_MAX, 0))
		return -EINVAL;
	ntuple_filter->dst_port = (uint16_t)temp;

	if (strncmp(in[CB_FLD_SRC_PORT_DLM], cb_port_delim,
			sizeof(cb_port_delim)) != 0)
		return -EINVAL;

	if (get_cb_field(&in[CB_FLD_PROTO], &temp, 0, UINT8_MAX, 0))
		return -EINVAL;
	ntuple_filter->proto = (uint8_t)temp;

	if (get_cb_field(&in[CB_FLD_PRIORITY], &temp, 0, UINT16_MAX, 0))
		return -EINVAL;
	ntuple_filter->priority = (uint16_t)temp;

	return ret;
}

/* Bypass comment and empty lines */
int
is_bypass_line(char *buff)
{
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
add_rules(const char *rule_path, struct flow_classifier *cls_app)
{
	FILE *fh;
	char buff[LINE_MAX];
	unsigned int i = 0;
	unsigned int total_num = 0;
	struct rte_eth_ntuple_filter ntuple_filter;
	int ret;
	rule_path = "ipv4_rules_file.txt";
	fh = fopen(rule_path, "rb");
	if (fh == NULL)
		rte_exit(EXIT_FAILURE, "%s: fopen %s failed\n", __func__,
			rule_path);

	ret = fseek(fh, 0, SEEK_SET);
	if (ret)
		rte_exit(EXIT_FAILURE, "%s: fseek %d failed\n", __func__,
			ret);

	i = 0;
	while (fgets(buff, LINE_MAX, fh) != NULL) {
		i++;

		if (is_bypass_line(buff))
			continue;

		if (total_num >= HASH_ENTRIES - 1) {
			printf("\nINFO: classify rule capacity %d reached\n",
				total_num);
			break;
		}

		if (parse_ipv4_5tuple_rule(buff, &ntuple_filter) != 0)
			rte_exit(EXIT_FAILURE,
				"%s Line %u: parse rules error\n",
				rule_path, i);
        
        struct onvm_ft_ipv4_5tuple key;
        key.dst_port = ntuple_filter.dst_port;
        key.dst_addr = ntuple_filter.dst_ip;
        key.src_addr = ntuple_filter.src_ip;
        key.src_port = ntuple_filter.src_port;
        key.proto = ntuple_filter.proto;

		_onvm_ft_print_key(&key);
	}

	fclose(fh);
	return 0;
}
