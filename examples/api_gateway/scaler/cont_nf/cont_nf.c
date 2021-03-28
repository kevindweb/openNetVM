#include "cont_nf.h"
#include "dpdk/rte_mbuf_core.h"

const int pkt_size = sizeof(struct rte_mbuf*);

/* file descriptors for TX and RX pipes */
int tx_fd;
int rx_fd;

int
open_pipes(void) {
        if ((rx_fd = open(CONT_RX_PIPE_NAME, O_RDONLY | O_NONBLOCK)) == -1) {
                return -1;
        }

        if ((tx_fd = open(CONT_TX_PIPE_NAME, O_WRONLY | O_NONBLOCK)) == -1) {
                return -1;
        }

        return 0;
}

void
pipe_cleanup(void) {
        close(rx_fd);
        close(tx_fd);

        remove(CONT_RX_PIPE_NAME);
        remove(CONT_TX_PIPE_NAME);
}

struct rte_mbuf*
read_packet(void) {
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
receive_packets(void) {
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
        /* open pipes */
        printf("Starting to open pipes\n");

        // pipes should be open when container initializes
        if (open_pipes() == -1) {
                printf("Pipes, rx: %s and tx: %s not configured correctly\n", CONT_RX_PIPE_NAME, CONT_TX_PIPE_NAME);
                exit(1);
        }

        printf("Initialization finished\n");

        /* receive packets */
        receive_packets();

        pipe_cleanup();
        return 0;
}
