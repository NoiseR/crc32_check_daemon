#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/stat.h>
#include "daemon.h"

#define DAEMON_ENV_DIR          "CRC32_CHECK_DAEMOM_DIR"
#define DAEMON_ENV_TIMEOUT      "CRC32_CHECK_DAEMOM_TIMEOUT"

int main(int argc, char *argv[]) {
    char *path_to_dir = NULL;
    char *env_timeout = NULL;
    int timeout_s = 0;
    int opt = 0;
    // try to get options from args
    while ((opt = getopt(argc, argv, "d:t:")) != -1) {
        switch (opt) {
        case 'd':
            path_to_dir = optarg;
            break;
        case 't':
            timeout_s = atoi(optarg);
            break;
        }
    }
    // if no arg try to get path from env
    if (!path_to_dir)
        path_to_dir = getenv(DAEMON_ENV_DIR);
    // no env variable for path
    if (!path_to_dir) {
        printf("[ERROR] provide path to directory via -d arg or %s env variable\n", DAEMON_ENV_DIR);
        exit(EXIT_FAILURE);
    }
    struct stat sb;
    if (stat(path_to_dir, &sb) != 0 || !S_ISDIR(sb.st_mode)) {
        printf("[ERROR] no such directory: %s\n", path_to_dir);
        exit(EXIT_FAILURE);
    }
    if (!timeout_s) {
        env_timeout = getenv(DAEMON_ENV_TIMEOUT);
        if (env_timeout)
            timeout_s = atoi(env_timeout);
        else {
            printf("[ERROR] provide deamon's timeout via -t arg or %s env variable\n", DAEMON_ENV_TIMEOUT);
            exit(EXIT_FAILURE);
        }
    }
    if (timeout_s <= 0) {
        printf("[ERROR] timeout must be > 0 (%d)\n", timeout_s);
        exit(EXIT_FAILURE);
    }
    // start working
    printf("[start deamon] dir %s, timeout %d sec ... ", path_to_dir, timeout_s);
    int start_res = start_daemon(path_to_dir, timeout_s);
    if (start_res == EXIT_SUCCESS) {
        printf("ok\n");
    } else {
        printf("error\n");
    }
    return start_res;
}
