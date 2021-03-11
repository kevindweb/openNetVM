#include <fcntl.h>
#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>

#define CONT_RX_PIPE_NAME "/rx_pipe"
#define CONT_TX_PIPE_NAME "/tx_pipe"

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
struct rte_mbuf*
read_packet(void);

/* Helper to send packet out through network */
int
write_packet(struct rte_mbuf*);