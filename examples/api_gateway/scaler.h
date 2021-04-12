/*********************************************************************
 *                     openNetVM
 *              https://sdnfv.github.io
 *
 *   BSD LICENSE
 *
 *   Copyright(c)
 *            2015-2020 George Washington University
 *            2015-2020 University of California Riverside
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
 * scaler.h - This application performs L3 forwarding.
 ********************************************************************/

#include <ftw.h>

// warm container ring
#define WARM_CONTAINER_RING "warm_cont_ring"

// service name for the docker containers
#define SERVICE "skeleton"

// how many warm containers to maintain at any time
#define WARM_CONTAINERS_REQUIRED 1

// stack to hold warm container pipe fds
struct rte_ring* warm_containers;

// how many pipes/containers have we spun up, but aren't ready for flows
int created_not_ready;

int
init_container(int);

int
init_stack(void);

int
scale_docker(int);

int
move_buffer_to_container(void);

int
kill_container_id(char*);

void
kill_docker(void);

void
cleanup(void);

/* pipe API to handle tx/rx for containers */
int
init_pipe_dir(void);

void
clean_pipes(void);

int
create_pipes(int ref);

void
ready_pipes(void);

int
unlink_cb(const char* fpath, __attribute__((unused)) const struct stat* sb, __attribute__((unused)) int typeflag,
          __attribute__((unused)) struct FTW* ftwbuf);

int
rmrf(const char* path);

int
create_pipe_dir(int ref);

/* list of initialized pipes */
struct init_pipe {
        int ref;
        char tx_pipe[20];
        char rx_pipe[20];
        int rx_fd;
        struct init_pipe* next;
};
