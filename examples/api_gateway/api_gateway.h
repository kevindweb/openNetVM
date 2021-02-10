/*********************************************************************
 *                     openNetVM
 *              https://sdnfv.github.io
 *
 *   BSD LICENSE
 *
 *   Copyright(c)
 *            2015-2020 George Washington University
 *            2015-2020 University of California Riverside
 *   All rights reserved.
 *
 *   Redistribution and use in source and binary forms, with or without
 *   modification, are permitted provided that the following conditions
 *   are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *     * The name of the author may not be used to endorse or promote
 *       products derived from this software without specific prior
 *       written permission.
 *
 *   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * api_gateway.h - This application performs L3 forwarding.
 ********************************************************************/

#include "onvm_flow_table.h"

#define NUM_CONTAINERS 4
#define CONT_NF_RXQ_NAME "Cont_Client_%u_RX"
#define CONT_NF_TXQ_NAME "Cont_Client_%u_TX"

/* This defines the maximum possible number entries in out flow table. */
#define HASH_ENTRIES 100 /// TODO: Possibly move this over to state struct.

struct onvm_ft *em_tbl;

struct container_nf *cont_nfs;

const struct rte_memzone *mz_cont_nf;

/*Struct that holds all NF state information */
struct state_info {
        uint64_t statistics[NUM_CONTAINERS];
        uint64_t dest_eth_addr[NUM_CONTAINERS];
        uint64_t packets_dropped;
        uint32_t print_delay;
        uint8_t print_keys;
        uint8_t max_containers;
};

struct container_nf {
        struct rte_ring *rx_q;
        struct rte_ring *tx_q;
        uint16_t instance_id;
        uint16_t service_id;
};
/* Function pointers for LPM or EM functionality. */

int
setup_hash(struct state_info *stats);

uint16_t
get_ipv4_dst(struct rte_mbuf *pkt, struct state_info *stats);

void
nf_cont_init_rings(struct container_nf *nf);

void
init_cont_nf(struct state_info *stats);
