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

// END globals section

/* API Calls below */
/* Return the number of containers up in docker-compose */
// int
// num_running_containers(void) {
//         FILE* fp;
//         int id_hash_length = 14;
//         char container_id[id_hash_length];
//         int num = 0;
//         char docker_call[100];

//         sprintf(docker_call, "docker ps -aq --filter='name=%s'", SERVICE);
//         fp = popen(docker_call, "r");
//         if (!fp) {
//                 printf("Docker failed to execute\n");
//                 return -1;
//         }

//         while (fgets(container_id, id_hash_length, fp) != NULL) {
//                 // remove new line character
//                 container_id[id_hash_length - 2] = '\0';
//                 printf("Container hash: %s\n", container_id);
//                 num++;
//         }

//         // close file descriptor
//         pclose(fp);

//         return num;
// }

/* Initialize docker stack to bring the services up */
int
init_stack(void) {
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
kill_docker(void) {
        char docker_call[100];
        sprintf(docker_call, "docker stack rm %s", SERVICE);
        system(docker_call);
}

void
cleanup(void) {
        kill_docker();
        clean_pipes();
}

/* scaler runs to maintain warm containers and garbage collect old ones */
void
scaler(void) {
        if (init_stack() == -1) {
                printf("Failed to start up docker stack\n");
                return;
        }

        int num_to_scale;

        // initlaize, to be modified by pipes.c
        created_not_ready = 0;

        for (; worker_keep_running;) {
                num_to_scale = WARM_CONTAINERS_REQUIRED - (rte_ring_count(scale_buffer_ring) + created_not_ready);
                if (num_to_scale > 0) {
                        /*
                         * The number of initialized + "not ready" containers represents
                         * the current amount of "warm" containers
                         * Need to make sure this number stays constant (psuedo-auto-scaling)
                         */
                        printf("Need to scale %d more containers\n", num_to_scale);
                        scale_docker(num_to_scale);
                } else if (unlikely(num_to_scale) < 0) {
                        perror("The number to scale should not be negative!");
                        break;
                } else {
                        // no new flows, just sleep for a bit
                        usleep(10);
                }

                if (created_not_ready > 0) {
                        // only test pipe readiness if there are some that haven't succeeded
                        ready_pipes();
                } else if (unlikely(created_not_ready) < 0) {
                        perror("We shouldn't have a negative amount of pipes!");
                        break;
                }

                break;
        }

        printf("Scaler thread exiting\n");
        cleanup();
}
