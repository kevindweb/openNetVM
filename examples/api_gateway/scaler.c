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
 * scaler.c - A container auto-scaling API for gateway to communicate with.
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

#include <rte_common.h>
#include <rte_mbuf.h>

#include "api_gateway.h"
#include "scaler.h"

// START globals section

// how many containers have we called docker service scale to overall
int total_scaled = 0;

// initialized by docker service scale, but pipes not ready
int num_initialized = 0;

// number of containers requested by gw, but not fulfilled by scaler
int num_requested = 0;

// list head for initialized pipes not ready for communication
struct init_pipe* head = NULL;

// stack to hold warm container pipe fds
struct rte_ring* warm_containers;

// END globals section

/* API Calls below */
/* Return the number of containers up in docker-compose */
int
num_running_containers() {
        FILE* fp;
        int id_hash_length = 14;
        char container_id[id_hash_length];
        int num = 0;
        char docker_call[100];

        sprintf(docker_call, "docker ps -aq --filter='name=%s'", SERVICE);
        fp = popen(docker_call, "r");
        if (!fp) {
                printf("Docker failed to execute\n");
                return -1;
        }

        while (fgets(container_id, id_hash_length, fp) != NULL) {
                // remove new line character
                container_id[id_hash_length - 2] = '\0';
                printf("Container hash: %s\n", container_id);
                num++;
        }

        // close file descriptor
        pclose(fp);

        return num;
}

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
        new_pipe->ref = ref; 
        strcpy(new_pipe->tx_pipe, tx_pipe); 
        strcpy(new_pipe->tx_pipe, rx_pipe); 
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

        return 0;
}

/* Return array of ref IDs of ready containers */
int*
ready_pipes() {
        int i, counter = 0;
        int fd;
        struct init_pipe* tmp;
        struct init_pipe* iterator = head;
        struct init_pipe* prev = iterator;
        int pipes[NUM_CONTAINERS];

        // open in nonblock write only will fail if pipe isn't open on read end
        while (iterator->next != NULL) {
                // pipe ready
                if ((fd = open(iterator->tx_pipe, O_WRONLY | O_NONBLOCK)) >= 0) {
                        /*
                         * Add (tx_fd, rx_fd) to the stack
                         * dont close pipe
                         * gateway can just call write(fd);
                         */
                        pipes[i] = iterator->ref;
                        close(fd);
                        counter++;

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
                } else {
                        prev = iterator;
                        iterator = iterator->next;
                }
                i++;
        }

        // pipe ready
        if ((fd = open(iterator->tx_pipe, O_WRONLY | O_NONBLOCK)) >= 0) {
                pipes[i] = iterator->ref;
                close(fd);
        }

        int* warm_pipes = (int*)malloc(sizeof(int) * counter);
        for (i = 0; i < counter; i++) {
                warm_pipes[i] = pipes[i];
        }

        return warm_pipes;
}

/* Initialize docker stack to bring the services up */
int
init_stack() {
        /*
         * Set up first named pipe
         * Initialize docker container
         * Place container ID in the shared struct of "warm" containers
         */

        // set up first named pipe
        if (create_pipes(++total_scaled) == -1)
                // failed pipe creation
                return -1;

        // docker command to start container
        const char* command = "docker stack deploy -c ./scaler/docker-compose.yml skeleton";
        int ret = system(command);

        if (WEXITSTATUS(ret) != 0)
                return -1;

        // initialize ring
        const unsigned flags = 0;
        warm_containers = rte_ring_create(WARM_CONTAINER_RING, MAX_CONTAINERS, rte_socket_id(), flags);
        if (warm_containers == NULL) {
                perror("Problem getting ring for warm containers\n");
                return -1;
        }

        return 0;
}

// initialize "scale" more containers (they will start in the "warm queue")
int
scale_docker(int scale) {
        char docker_call[100];
        for (int i = total_scaled + 1; i <= scale + total_scaled; i++) {
                // create the pipes for a specific container ID
                printf("scale %d\n", i);
                if (create_pipes(i) == -1) {
                        // failed to make pipe
                        printf("Could not create pipes to scale containers\n");
                        return -1;
                }
        }

        // increment number of containers that were scaled
        total_scaled += scale;

        sprintf(docker_call, "docker service scale %s_%s=%d", SERVICE, SERVICE, total_scaled);

        int ret = system(docker_call);
        if (WEXITSTATUS(ret) != 0)
                return -1;

        return 0;
}

/* Garbage collector helper to kill container by ID */
int
kill_container_id(char* hash_id) {
        char docker_call[36];
        // docker_call string is 36 characters with a 12 character container hash id + \0
        // 36 characters -> docker rm -f <container hash id>\0
        sprintf(docker_call, "docker rm -f %s", hash_id);
        return system(docker_call);
}

/* Helper for testing, kill the docker service */
void
kill_docker() {
        char docker_call[100];
        sprintf(docker_call, "docker stack rm %s", SERVICE);
        system(docker_call);
}

/* Send warm container fds to gateway */
void
send_containers() {
        // pipe file descriptors in the stack
        void** pipe_fds;
        int num_to_pop;
        int ret;

        // number of completely ready pipes
        int num_warm = rte_ring_count(warm_containers);

        if (num_warm == 0 || num_requested == 0) {
                // nothing to do, no containers to send
                return;
        }

        if (num_warm <= num_requested) {
                // need to pop the entire available stack
                num_requested -= num_warm;
                num_to_pop = num_warm;
        } else {
                // only need to pop the first <num_requested>
                num_to_pop = num_requested;
                num_requested = 0;
        }

        ret = rte_ring_dequeue_bulk(warm_containers, pipe_fds, num_to_pop, NULL);
        if (ret == 0) {
                perror("Could not pop the stack pipes to send\n");
                return;
        }

        // now that they're scaled, enqueue to the scale_to_gate
        if (rte_ring_enqueue(to_gate_ring, pipe_fds) < 0) {
                perror("Failed to send containers to gateway\n");
                return;
        }

        // our number of initialized containers drops after we pop them
        num_initialized -= num_to_pop;
}

/* scaler runs to maintain warm containers and garbage collect old ones */
void*
scaler(void* in) {
        if (init_stack() == -1) {
                printf("Failed to start up docker stack\n");
                return NULL;
        }

        while (1) {
                void* msg;
                if (rte_ring_dequeue(to_scale_ring, &msg) < 0) {
                        // no new flows yet, just do auto-scaling work
                        usleep(5);
                        continue;
                }

                // gateway asked us to do something
                num_requested += *((int*)msg);
                int num_to_scale = num_requested - num_initialized;
                printf("Received %d containers and need to scale up %d\n", num_requested, num_to_scale);

                if (num_to_scale > 0) {
                        // need to scale more to answer the gateway's request
                        scale_docker(num_to_scale);
                }

                // now that they're scaled, enqueue to the scale_to_gate
                send_containers();

                break;
        }

        printf("Scaler thread exiting\n");
        kill_docker();
        return NULL;
}