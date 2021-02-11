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
 * scale.c - A container auto-scaling API for gateway to communicate with.
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
#include <unistd.h>

#include <rte_branch_prediction.h>
#include <rte_common.h>
#include <rte_debug.h>
#include <rte_eal.h>
#include <rte_launch.h>
#include <rte_lcore.h>
#include <rte_log.h>
#include <rte_memory.h>
#include <rte_per_lcore.h>
#include <rte_ring.h>

#include "scaler.h"

// START globals section
static const char* _GATE_2_SCALE = "GATEWAY_2_SCALER";
static const char* _SCALE_2_GATE = "SCALER_2_GATEWAY";
struct rte_ring *send_ring, *recv_ring;

// tell scaler thread to stop executing
int DONE = 0;

// next container ID (auto-incremented)
int NEXT_CONTAINER = 0;

// number of "warm" containers (haven't been assigned a flow)
int NUM_WARM_CONTAINERS = 0;

// service name for the docker containers
const char* SERVICE = "skeleton";

// docker command to start container (sprintf used to insert ID variable)
const char* COMMAND =
    "sudo docker run \
    --name='%s%d' \
    --hostname='%s%d' \
    --volume=/tmp/container/%d:/tmp/pipe \
    --volume=/local/onvm/openNetVM/container:/container \
    --detach \
    ubuntu:20.04 \
    /bin/bash -c './container/test.sh'";

// only a global variable for testing between threads
// we will eventually put new container IDs into a thread-safe linked list
char container_id[13];

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

        sprintf(docker_call, "/bin/sudo docker ps -aq --filter='name=%s'", SERVICE);
        fp = popen(docker_call, "r");
        if (!fp) {
                printf("Docker failed to execute\n");
                return -1;
        }

        while (fgets(container_id, id_hash_length, fp) != NULL) {
                // remove new line character
                container_id[id_hash_length - 2] = '\0';
                num++;
        }

        // close file descriptor
        pclose(fp);

        return num;
}

/* Initialize warm docker containers, with unique named pipe volumes */
int
init_container(int retry) {
        /*
         * Set up named pipe
         * Initialize docker container
         * Place container ID in the shared struct of "warm" containers
         */
        FILE* fp;
        int id_hash_length = 13;
        // char container_id[id_hash_length];
        int stat;
        char docker_call[300];

        sprintf(docker_call, COMMAND, SERVICE, NEXT_CONTAINER, SERVICE, NEXT_CONTAINER, NEXT_CONTAINER);
        fp = popen(docker_call, "r");
        if (!fp) {
                printf("Docker failed to execute\n");
                return -1;
        }

        // output should be one line (the container ID which is a 64 character hash with \0 terminator)
        fgets(container_id, id_hash_length, fp);

        // close popen
        stat = pclose(fp);

        if (WEXITSTATUS(stat) != 0) {
                // docker command failed
                return -1;
        }

        // increment for next scale call
        NEXT_CONTAINER++;

        // add this container ID to the warm pool Linked List
        return 0;
}

// initialize "scale" number of warm containers
int
scale_docker(int scale) {
        for (int i = 0; i < scale; i++) {
                if (init_container(0) == -1) {
                        // failed to instantiate container
                        printf("Couldn't init container %d\n", (i + 1));
                        return -1;
                }
        }

        // increment number of warm containers
        NUM_WARM_CONTAINERS += scale;

        return 0;
}

/* Garbage collector helper to kill container by ID */
int
kill_container_id(char* hash_id) {
        char docker_call[36];
        // docker_call string is 36 characters with a 12 character container hash id + \0
        // 36 characters -> /bin/sudo docker rm -f <container hash id>\0
        sprintf(docker_call, "/bin/sudo docker rm -f %s", hash_id);
        return system(docker_call);
}

/* Helper for testing, kill the docker service */
void
kill_docker() {
        char docker_call[100];
        sprintf(docker_call, "/bin/sudo docker rm -f $(/bin/sudo docker ps -aq --filter='name=%s')", SERVICE);
        system(docker_call);
}

void
init_rings() {
        const unsigned flags = 0;
        const unsigned ring_size = 64;

        send_ring = rte_ring_create(_SCALE_2_GATE, ring_size, rte_socket_id(), flags);
        recv_ring = rte_ring_create(_GATE_2_SCALE, ring_size, rte_socket_id(), flags);
        if (send_ring == NULL)
                rte_exit(EXIT_FAILURE, "Problem getting sending ring\n");
        if (recv_ring == NULL)
                rte_exit(EXIT_FAILURE, "Problem getting receiving ring\n");
}

/* scaler runs to maintain warm containers and garbage collect old ones */
void
scaler() {
        // while (!DONE) {
        //         // maintain the correct number of "warm" containers
        //         // garbage collect
        //         // printf("Inside thread\n");
        //         sleep(5);
        //         printf("Scaler container kill: %s\n", container_id);
        //         kill_container_id(container_id);
        // }
        while (1) {
                void* msg;
                if (rte_ring_dequeue(recv_ring, &msg) < 0) {
                        usleep(5);
                        continue;
                }

                // gateway asked us to do something
                printf("Received '%s'\n", (char*)msg);
                break;
        }

        return;
}

void
test_done() {
        // DONE = 1;
        // pthread_join(tid, NULL);
        printf("Containers running: %d\n", num_running_containers());
        kill_docker();
}

int
main(int argc, char* argv[]) {
        /*
         * create API that checks how many containers are alive
         * create scaling thread that maintains the specific number of live containers
         * periodically checks the last timestamp (when was the last packet a container received (is it old))
         */

        // pthread_t tid;
        // scaler acts as initializer and garbage collector
        // pthread_create(&tid, NULL, scaler, NULL);

        // run API tests
        init_rings();
        scaler();
        // if (scale_docker(1) != 0) {
        //         printf("Couldn't initialize containers\n");
        //         test_done();
        //         return -1;
        // }
        // sleep(1);
        // printf("Containers running: %d\n", num_running_containers());

        // tell scaler we're finished
        test_done();
        return 0;
}