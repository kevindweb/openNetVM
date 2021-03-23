/*********************************************************************
 *                     openNetVM
 *              https://sdnfv.github.io
 *
 *   BSD LICENSE
 *
 *   Copyright(c)
 *            2015-2021 George Washington University
 *            2015-2021 University of California Riverside
 *   All rights reserved.
 *
 *   Redistribution and use in source and binary forms, with or without
 *   modification, are permitted provided that the following conditions
 *   are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *     * The name of the author may not be used to endorse or promote
 *       products derived from this software without specific prior
 *       written permission.
 *
 *   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * pipes.c - API for handling pipe initlization and cleanup
 ********************************************************************/

#include <errno.h>
#include <getopt.h>
#include <inttypes.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/queue.h>
#include <sys/stat.h>
#include <unistd.h>

#include "api_gateway.h"
#include "scaler.h"

// list head for initialized pipes not ready for communication
struct init_pipe* head = NULL;

/* Create rx and tx pipes in /tmp/rx and /tmp/tx */
int
create_pipes(int ref) {
        // rx pipe name
        static char rx_pipe[sizeof(CONT_RX_PIPE_NAME) + 4];
        sprintf(rx_pipe, CONT_RX_PIPE_NAME, ref);

        // remove any old pipes with same name
        remove(rx_pipe);

        // create rx pipe
        if (mkfifo(rx_pipe, 0666) == -1) {
                perror("mkfifo");
                return -1;
        }

        // tx pipe name
        static char tx_pipe[sizeof(CONT_TX_PIPE_NAME) + 4];
        sprintf(tx_pipe, CONT_TX_PIPE_NAME, ref);

        // remove any old pipes with same name
        remove(tx_pipe);

        // create tx pipe
        if (mkfifo(tx_pipe, 0666) == -1) {
                perror("mkfifo");
                return -1;
        }

        // add initialized pipe to list
        struct init_pipe* new_pipe = (struct init_pipe*)malloc(sizeof(struct init_pipe));
        if (new_pipe == NULL) {
                printf("Couldn't malloc new pipe\n");
                return -1;
        }
        new_pipe->ref = ref;
        strncpy(new_pipe->tx_pipe, tx_pipe, strlen(tx_pipe));
        strncpy(new_pipe->rx_pipe, rx_pipe, strlen(rx_pipe));
        new_pipe->next = NULL;

        if (head == NULL) {
                head = new_pipe;
        } else {
                struct init_pipe* iterator = head;
                while (iterator->next != NULL) {
                        iterator = iterator->next;
                }
                iterator->next = new_pipe;
        }

        created_not_ready++;

        return 0;
}

/* Add ready containers to warm containers stack */
void
ready_pipes(void) {
        int tx_fd, rx_fd;
        struct init_pipe* tmp;
        struct init_pipe* iterator = head;
        struct init_pipe* prev = iterator;
        struct pipe_fds* warm_pipes;

        // open in nonblock write only will fail if pipe isn't open on read end
        while (iterator->next != NULL) {
                // pipe ready
                if (((tx_fd = open(iterator->tx_pipe, O_WRONLY | O_NONBLOCK)) >= 0) &&
                    (rx_fd = open(iterator->rx_pipe, O_RDONLY | O_NONBLOCK)) >= 0) {
                        // add (tx_fd, rx_fd) to the stack
                        warm_pipes = malloc(sizeof(struct pipe_fds));
                        warm_pipes->rx_pipe = rx_fd;
                        warm_pipes->tx_pipe = tx_fd;

                        if (rte_ring_enqueue(scale_buffer_ring, (void*)warm_pipes) < 0) {
                                perror("Failed to send containers to gateway\n");
                                return;
                        }

                        created_not_ready--;

                        // remove from init pipes list
                        if (iterator == head) {
                                if (iterator->next == NULL) {
                                        head = NULL;
                                        prev = head;
                                        free(prev);
                                } else {
                                        prev = iterator;
                                        head = iterator->next;
                                        free(prev);
                                }
                        } else {
                                prev->next = iterator->next;
                                tmp = iterator;
                                free(tmp);
                        }
                }
                prev = iterator;
                iterator = iterator->next;
        }

        // pipe ready
        if (((tx_fd = open(iterator->tx_pipe, O_WRONLY | O_NONBLOCK)) >= 0) &&
            (rx_fd = open(iterator->rx_pipe, O_RDONLY | O_NONBLOCK)) >= 0) {
                // add (tx_fd, rx_fd) to the stack
                warm_pipes->rx_pipe = rx_fd;
                warm_pipes->tx_pipe = tx_fd;

                if (rte_ring_enqueue(scale_buffer_ring, (void*)warm_pipes) < 0) {
                        perror("Failed to send containers to gateway\n");
                        return;
                }

                created_not_ready--;

                // remove from init pipes list
                if (iterator == head) {
                        head = NULL;
                        prev = head;
                        free(prev);
                } else {
                        prev->next = iterator->next;
                        tmp = iterator;
                        free(tmp);
                }
        }
}

struct rte_mbuf*
read_packet(int rx_fd) {
        size_t pkt_size = sizeof(struct rte_mbuf);
        struct rte_mbuf* packet = malloc(pkt_size);
        if (read(rx_fd, packet, pkt_size) == -1) {
                return NULL;
        }

        return packet;
}

int
write_packet(int tx_fd, struct rte_mbuf* packet) {
        if (write(tx_fd, packet, sizeof(struct rte_mbuf*)) == -1) {
                return -1;
        }

        return 0;
}

void
clean_pipes(void) {
        return;
}
