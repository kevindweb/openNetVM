#include "cont_nf.h"
#include "onvm_nflib.h"
#include "onvm_pkt_helper.h"

#define NF_TAG "cont_nf"


const char *
get_cont_pipe_name(unsigned id) {
    /* buffer for return value. Size calculated by %u being replaced
    * by maximum 3 digits (plus an extra byte for safety) */
    static char buffer[sizeof(CONT_NF_PIPE_NAME) + 4];
    sprintf(buffer, CONT_NF_PIPE_NAME, id);
    return buffer;
}

int 
main(void)
{
    /* just see if we can receive a packet */ 
    int fd = -1;
    
	pkt* packet = malloc (sizeof(pkt));

    /* open fifo to read */
    while(fd == -1) {
        fd = open(get_cont_pipe_name(1), O_RDONLY);
    }
    
    while (1) {
        /* get packet */ 
        read(fd, packet, sizeof(pkt));
        printf("Received port: %d\n", packet->pkt_len);
    }

    close(fd);
}