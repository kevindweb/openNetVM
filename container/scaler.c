#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

int DONE = 0;

void
scaler() {
        while (!DONE) {
                // maintain the correct number of "warm" containers
                // garbage collect
        }
}

/* API Calls below */
/* Return the number of containers up in docker-compose */
int
num_running_containers() {
        FILE* fp;
        int id_hash_length = 64;
        char container_id[id_hash_length];
        int num;

        fp = popen("/bin/sudo docker-compose ps -q", "r");
        if (!fp) {
                printf("Docker compose failed to execute\n");
                return -1;
        }

        while (fgets(container_id, id_hash_length, fp) != NULL) {
                printf("Container ID: %s\n", container_id);
                num++;
        }

        return num;
}

/* Helper for testing, init docker compose service */
void
init_docker_compose() {
        // initialize docker-compose through docker-compose.yml and "-d" (detach)
        system("sudo docker-compose up -d");
}

/* Helper for testing, kill the docker compose service */
void
kill_docker_compose(char* service) {
        char docker_call[100];
        sprintf("sudo docker-compose kill %s", service);
        system(docker_call);
}

int
main(int argc, char* argv[]) {
        const char* container_service = "skeleton";
        /*
         * create API that checks how many containers are alive
         * create scaling thread that maintains the specific number of live containers
         * periodically checks the last timestamp (when was the last packet a container received (is it old))
         */

        pthread_t tid;

        pthread_create(&tid, NULL, scaler, NULL);

        // run API tests
        init_docker_compose();

        // tell scaler we're finished
        DONE = 1;
        pthread_join(tid, NULL);
        kill_docker_compose(container_service);
}