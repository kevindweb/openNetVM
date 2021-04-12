#include "cont_nf.h"
#include "dpdk/rte_mbuf_core.h"

const int pkt_size = sizeof(struct rte_mbuf*);

/* file descriptors for TX and RX pipes */
int tx_fd = 0;
int rx_fd = 0;

int
open_pipes(void) {
        if (rx_fd < 1 && (rx_fd = open(CONT_RX_PIPE_NAME, O_RDONLY)) == -1) {
                perror("open rx fail");
                return -1;
        }
        printf("Opened rx pipe\n");
        if (tx_fd < 1 && (tx_fd = open(CONT_TX_PIPE_NAME, O_WRONLY | O_NONBLOCK)) == -1) {
                perror("open tx fail");
                return -1;
        }
        printf("Opened tx pipe\n");

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
        /*
         * do this in loop:
         * read packet from rx_fd from host
         * convert rte_mbuf into LWIP pbuf
         * call net_if.input to push packet to TCP stack
         */
        while (1) {
                struct rte_mbuf* packet = read_packet();
                if (packet == NULL) {
                        perror("Couldn't read packet data");
                        return;
                }
                printf("Received packet from port %d\n", packet->port);

                // dummy to let host know we modified packet data
                packet->port = 10;

                if (write_packet(packet) == -1) {
                        perror("Couldn't write data to TX pipe");
                        return;
                }
        }
}
/*
void
receive_packets(void) {
        // initialize tcp stack
        if (init_stack() < 0) {
                printf("Couldn't initialize lwip stack");
                return;
        }
        // do this in loop:
        // read packet from rx_fd from host
        // convert rte_mbuf into LWIP pbuf
        // call net_if.input to push packet to TCP stack

        struct rte_mbuf* packet;
        printf("Running main loop to check for packets\n");
        while (1) {
                packet = read_packet();
                if (packet == NULL) {
                        printf("Could not read packet data\n");
                        continue;
                }
                printf("Received packet from port %d\n", packet->port);

                // convert mbuf to lwip pbuf and input into the TCP stack
        }

        if (write_packet(packet) == -1) {
                perror("Couldn't write data to TX pipe");
        }
}
*/

int
main(void) {
        /* open pipes */
        printf("Starting to open pipes\n");
        int count = 0;

        // pipes should be open when container initializes
        while (open_pipes() == -1 && count++ < RETRY_OPEN_PIPES) {
                printf("Pipes, rx: %s and tx: %s not configured correctly\n", CONT_RX_PIPE_NAME, CONT_TX_PIPE_NAME);
                sleep(1);
                // exit(1);
        }

        printf("Initialization finished\n");

        /* receive packets */
        receive_packets();

        // most likely an error, as most containers run the lifespan of a client
        printf("Finished executing\n");

        // pipe_cleanup();
        return 0;
}
