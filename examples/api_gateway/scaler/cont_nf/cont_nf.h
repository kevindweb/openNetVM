#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <inttypes.h>

#define CONT_RX_PIPE_NAME "rx_pipe"
#define CONT_TX_PIPE_NAME "tx_pipe"

typedef struct pkt {
	void * buf_addr;
    uint16_t refcnt; 
    uint16_t nb_segs;
    uint16_t port;
    uint64_t ol_flags;
    uint32_t pkt_len;
    uint16_t data_len;
    uint16_t vlan_tci;
    uint16_t vlan_tci_outer;
    uint16_t buf_len;
    uint16_t priv_size;
    uint16_t timesync;
    uint16_t dynfield1[9];
    uint32_t packet_type;
    uint8_t l2_type:4;
    uint8_t l3_type:4;
    uint8_t l4_type:4;
    uint8_t tun_type:4;
    uint8_t inner_esp_next_proto;
    uint8_t inner_l2_type:4;
    uint8_t inner_l3_type:4;
    uint8_t inner_l4_type:4;
} pkt;

int
create_pipes(void);

int
open_pipes(void);

struct pipe_fds {
    int rx_fd;
    int tx_fd;
};