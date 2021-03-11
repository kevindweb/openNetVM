#include <rte_mbuf_core.h>

#include "cont_nf.h"

const int pkt_size = sizeof(struct rte_mbuf*);

/* file descriptors for TX and RX pipes */
int tx_fd;
int rx_fd;

/*
 * Create rx and tx pipes
 * Return 0 on success, -1 on failure
 */
// int
// create_pipes() {
//         // remove any old pipes with same name
//         remove(CONT_RX_PIPE_NAME);
//         remove(CONT_TX_PIPE_NAME);

//         // create rx pipe
//         if (mkfifo(CONT_RX_PIPE_NAME, 0666) == -1) {
//                 perror("mkfifo");
//                 return -1;
//         }

//         // create tx pipe
//         if (mkfifo(CONT_TX_PIPE_NAME, 0666) == -1) {
//                 perror("mkfifo");
//                 return -1;
//         }

//         // init fds to -1
//         warm_pipes = malloc(sizeof(struct pipe_fds));
//         warm_pipes->rx_fd = -1;
//         warm_pipes->tx_fd = -1;

//         return 0;
// }

int
open_pipes() {
        if (rx_fd = open(CONT_RX_PIPE_NAME, O_RDONLY | O_NONBLOCK) == -1) {
                return -1;
        }

        if (tx_fd = open(CONT_TX_PIPE_NAME, O_WRONLY | O_NONBLOCK) == -1) {
                return -1;
        }

        return 0;
}

void
pipe_cleanup() {
        remove(CONT_RX_PIPE_NAME);
        remove(CONT_TX_PIPE_NAME);

        close(rx_fd);
        close(tx_fd);
}

struct rte_mbuf*
read_packet() {
        size_t pkt_size = sizeof(struct rte_mbuf);
        struct rte_mbuf* packet = malloc(pkt_size);
        if (read(rx_fd, packet, pkt_size) == -1) {
                return NULL;
        }

        return packet;
}

int
write_packet(struct rte_mbuf* packet) {
        if (write(tx_fd, packet, pkt_size) == -1) {
                return -1;
        }

        return 0;
}

/*
 * Receive incoming packets
 */
void
receive_packets() {
        /* put in loop or however we want to set this up */
        struct rte_mbuf* packet = read_packet();
        if (packet == NULL) {
                perror("Couldn't read packet data\n");
        }
        printf("Received packet from port %d\n", packet->port);

        // dummy to let host know we modified packet data
        packet->port = 10;

        if (write_packet(packet) == -1) {
                perror("Couldn't write data to TX pipe");
        }
}

int
main(void) {
        int ret = -1;

        /* create pipes */
        // if (create_pipes() == -1) {
        //         pipe_cleanup();
        //         exit(0);
        // }

        /* open pipes */
        printf("Starting to open pipes\n");
        while (open_pipes() == -1) {
        }

        printf("Initialization finished\n");

        /* receive packets */
        receive_packets();

        pipe_cleanup();
        return 0;
}
