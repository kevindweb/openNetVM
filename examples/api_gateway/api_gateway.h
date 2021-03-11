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

#include "onvm_common.h"
#include "onvm_flow_table.h"
#include "onvm_pkt_common.h"

#define NUM_CONTAINERS 4
#define CONT_RX_PIPE_NAME "/tmp/rx/%d"
#define CONT_TX_PIPE_NAME "/tmp/tx/%d"
#define PKTMBUF_POOL_NAME "MProc_pktmbuf_pool"
#define _GATE_2_BUFFER "GATEWAY_2_BUFFER"
#define _SCALE_2_BUFFER "SCALE_2_BUFFER"

// 1024 maximum open file descriptors (stated by linux) / (2 pipes/container)
#define MAX_CONTAINERS 512

/* This defines the maximum possible number entries in out flow table. */
#define HASH_ENTRIES 100  /// TODO: Possibly move this over to state struct.

// How many milliseconds should we set for epoll_wait in the polling thread
#define POLLING_TIMEOUT 500

/* Handle signals and shutdowns between threads */
static uint8_t worker_keep_running;

struct onvm_ft *em_tbl;

struct container_nf *cont_nfs;

const struct rte_memzone *mz_cont_nf;

struct rte_mempool *pktmbuf_pool;

struct queue_mgr *poll_tx_mgr;

/* Each new flow is a new container to scale, gateway increments, scaler satisfies request */
rte_atomic16_t containers_to_scale;

// buffer pulls from gateway and scaler ring buffers

// ring to add and delete TX pipes from the polling thread
// static const char *_SCALE_2_POLL_ADD = "SCALE_2_POLL_ADD";
// static const char *_SCALE_2_POLL_DEL = "SCALE_2_POLL_DEL";

struct rte_ring *scale_buffer_ring, *gate_buffer_ring, *scale_poll_add_ring, *scale_poll_del_ring;
struct packet_buf *scaling_buf, *pkts_deq_burst, *pkts_enq_burst;

struct onvm_nf_local_ctx *nf_local_ctx;

/*Struct that holds all NF state information */
struct state_info {
        uint64_t statistics[NUM_CONTAINERS];
        uint64_t dest_eth_addr[NUM_CONTAINERS];
        uint64_t packets_dropped;
        uint32_t print_delay;
        uint8_t print_keys;
        uint8_t max_containers;
};

/* Function pointers for LPM or EM functionality. */

int
setup_hash(struct state_info *stats);

uint16_t
get_ipv4_dst(struct rte_mbuf *pkt);

void
nf_setup(struct onvm_nf_local_ctx *nf_local_ctx);

/* functions to start up threads as child NFs */
int
start_child(const char *tag);

void *
start_scaler(void *arg);

void
scaler(void);

void *
start_buffer(void *arg);

void
buffer(void);

void *
start_polling(void *arg);

void
polling(void);

// Helpers

void
init_rings(void);

void
sig_handler(int sig);

struct queue_mgr *
create_tx_poll_mgr(void);

void
enqueue_mbuf(struct rte_mbuf *pkt);

// Pipe read/write helpers
struct rte_mbuf *
read_pipe(int fd);

int
write_pipe(int fd, struct rte_mbuf *packet);
