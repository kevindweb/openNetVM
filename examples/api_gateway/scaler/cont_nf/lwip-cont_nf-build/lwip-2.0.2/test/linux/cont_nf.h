#include <fcntl.h>
#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>

#include "dpdk/rte_mbuf_core.h"

// cont_nf reads through the host's write (tx) pipe
#define CONT_RX_PIPE_NAME "/pipe/tx"
// and writes (tx) with the pipe the host reads (rx) from
#define CONT_TX_PIPE_NAME "/pipe/rx"

#define RETRY_OPEN_PIPES 20

// TODO: set this to a realistic cutoff point
#define PKT_MAX_SZ 256

#define UNUSED(x) (void)(x)

// TODO: not sure we still need this if pies are created on the host side
int
create_pipes(void);

/*
 * Open rx and tx pipes (created on host-side)
 * Return 0 on success, -1 on failure
 */
int
open_pipes(void);

/* Cleanup pipes and close fds */
void
pipe_cleanup(void);

/* Helper to fill packet from RX pipe */
int
read_packet(struct rte_mbuf*);

/* Helper to send packet out through network */
int
write_packet(struct rte_mbuf*);

void
receive_packets(void);

/* List of functions in lwip_stack.c */

/* Init connections and entire TCP stack */
int
init_stack(void);

/* Turn dpdk mbuf pkt into lwip pbuf and push to TCP iface */
int
input_mbuf_to_if(struct rte_mbuf*);