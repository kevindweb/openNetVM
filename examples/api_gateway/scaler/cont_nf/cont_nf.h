#include <fcntl.h>
#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>

#define CONT_RX_PIPE_NAME "/rx_pipe"
#define CONT_TX_PIPE_NAME "/tx_pipe"

int
create_pipes(void);

int
open_pipes(void);

struct pipe_fds {
        int rx_fd;
        int tx_fd;
};