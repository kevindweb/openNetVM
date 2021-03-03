#include "cont_nf.h"


/* global struct of tx and rx fds */ 
struct pipe_fds* warm_pipes; 

/* 
 * Create rx and tx pipes
 * Return 0 on success, -1 on failure 
 */ 
int
create_pipes() { 
    // remove any old pipes with same name
    remove(CONT_RX_PIPE_NAME);
    remove(CONT_TX_PIPE_NAME);

    // create rx pipe
    if (mkfifo(CONT_RX_PIPE_NAME, 0666) == -1) {
            perror("mkfifo");
            return -1;
    }

    // create tx pipe
    if (mkfifo(CONT_TX_PIPE_NAME, 0666) == -1) {
            perror("mkfifo");
            return -1;
    }

    // init fds to -1
    warm_pipes = malloc (sizeof(struct pipe_fds)); 
    warm_pipes->rx_fd = -1;
    warm_pipes->tx_fd = -1;

    return 0;
}

/* 
 * Open rx and tx pipes
 * Return 0 on success, -1 on failure
 */
int
open_pipes() {
    int tx_fd, rx_fd;

    if (warm_pipes->rx_fd == -1) {
        if (rx_fd = open(CONT_RX_PIPE_NAME, O_RDONLY | O_NONBLOCK) == -1) {
            return -1;
        } else {
            warm_pipes->rx_fd = rx_fd;
        }
    }

    if (warm_pipes->tx_fd == -1) {
        if (tx_fd = open(CONT_TX_PIPE_NAME, O_WRONLY | O_NONBLOCK) == -1) {
            return -1;
        }
        else {
            warm_pipes->tx_fd = tx_fd;
        }
    }

    return 0;
}

/* 
 * Cleanup pipes and close fds
 */
void
pipe_cleanup() {
    remove(CONT_RX_PIPE_NAME);
    remove(CONT_TX_PIPE_NAME);

    close(warm_pipes->rx_fd);
    close(warm_pipes->tx_fd); 
}

/*
 * Receive incoming packets
 */
void
receive_packets() {
    /* put in loop or however we want to set this up */ 
    pkt* packet = malloc (sizeof(pkt));

    if (read(warm_pipes->rx_fd, packet, sizeof(pkt)) == -1) {
        perror("Read ");
        return;
    }
}

int 
main(void)
{
    int ret = -1;

    /* create pipes */ 
    if (create_pipes() == -1) {
        pipe_cleanup();
        exit(0);
    }

    /* open pipes */ 
    while (ret = -1) {
        ret = open_pipes();
    }

    /* receive packets */ 
    receive_packets();

    pipe_cleanup();
    return 0;
}