#include "cont_nf.h"
#include "dpdk/rte_mbuf_core.h"

/* file descriptors for TX and RX pipes */
int tx_fd = 0;
int rx_fd = 0;

int
open_pipes(void) {
        if ((rx_fd = open(CONT_RX_PIPE_NAME, O_RDONLY)) == -1) {
                perror("open rx fail");
                return -1;
        }

        if ((tx_fd = open(CONT_TX_PIPE_NAME, O_WRONLY | O_NONBLOCK)) == -1) {
                perror("open tx fail");
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

int
read_packet(struct rte_mbuf* packet) {
        return read(rx_fd, packet, sizeof(struct rte_mbuf));
}

int
write_packet(struct rte_mbuf* packet) {
        return write(tx_fd, packet, sizeof(struct rte_mbuf));
}

/*
 * Receive incoming packets and push to LWIP
 */
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
        int ret;
        struct rte_mbuf packet;
        while (1) {
                ret = read(rx_fd, &packet, sizeof(struct rte_mbuf));
                if (ret < 1) {
                        // no data yet
                        sleep(1);
                        continue;
                }
                printf("Received packet from port %d\n", packet.port);

                // dummy to let host know we modified packet data
                // packet.port = 8080;

                if (input_mbuf_to_if(&packet) == -1) {
                        printf("Couldn't input packet with port %d to TCP stack\n", packet.port);
                        continue;
                }
        }
}

int
main(void) {
        printf("Starting Container NF\n");
        // pipes should be open when container initializes
        if (open_pipes() == -1) {
                printf("Pipes, rx: %s and tx: %s not configured correctly\n", CONT_RX_PIPE_NAME, CONT_TX_PIPE_NAME);
                exit(1);
        }

        printf("Initialization finished\n");

        /* receive packets */
        receive_packets();

        // most likely an error, as most containers run the lifespan of a client
        printf("Finished executing\n");
        return 0;
}
