#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

// globals to communicate between threads

// tell scaler thread to stop executing
int DONE = 0;

// next container ID (auto-incremented)
int NEXT_CONTAINER = 0;

// number of "warm" containers (haven't been assigned a flow)
int NUM_WARM_CONTAINERS = 0;

// service name for the docker containers
const char* SERVICE = "skeleton";

/* scaler runs to maintain warm containers and garbage collect old ones */
void*
scaler(void* in) {
        while (!DONE) {
                // maintain the correct number of "warm" containers
                // garbage collect
                // printf("Inside thread\n");
                sleep(1);
        }
        return NULL;
}

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

        int ret;
        char docker_call[100];

        sprintf(docker_call, "./docker.sh -i %d", NEXT_CONTAINER);
        ret = system(docker_call);

        if (ret == -1) {
                // failed to create docker container
                if (retry == 0) {
                        // retry once
                        return init_container(1);
                }

                // retried too many times with failure
                return -1;
        }
        // increment for next scale call
        NEXT_CONTAINER++;

        // add this container ID to the warm pool list
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

int
test_done(pthread_t tid) {
        DONE = 1;
        pthread_join(tid, NULL);
        kill_docker();

        return -1;
}

int
main(int argc, char* argv[]) {
        /*
         * create API that checks how many containers are alive
         * create scaling thread that maintains the specific number of live containers
         * periodically checks the last timestamp (when was the last packet a container received (is it old))
         */

        pthread_t tid;
        // scaler acts as initializer and garbage collector
        pthread_create(&tid, NULL, scaler, NULL);

        // run API tests
        if (scale_docker(3) != 0) {
                printf("Couldn't initialize containers\n");
                return test_done(tid);
        }
        sleep(1);
        printf("Containers running: %d\n", num_running_containers());

        // tell scaler we're finished
        return test_done(tid);
}