#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdlib.h>
#include <signal.h>
#include <syslog.h>
#include <dirent.h>
#include <stdio.h>

#include <fstream>
#include <unordered_map>
#include <boost/crc.hpp>

#define DAEMON_LOG_FILE         "crc32_check_daemon"
#define DAEMON_ENV_DIR          "CRC32_CHECK_DAEMOM_DIR"
#define DAEMON_ENV_TIMEOUT      "CRC32_CHECK_DAEMOM_TIMEOUT"

struct file_info {
    uint32_t crc32_sum;
    bool     checked;
};

static std::unordered_map<std::string, file_info> check_files;

static int save_file_info(const char *path, const char *file_name, uint32_t crc32_sum) {
    try {
        std::string full_name = std::string(path) + "/" + std::string(file_name);
        check_files[full_name] = {crc32_sum, false};
    }  catch (...) {
        return 1;
    }
    return 0;
}

static int check_file_info(const char *path, const char *file_name, uint32_t crc32_sum) {
    try {
        std::string full_name = std::string(path) + "/" + std::string(file_name);

        if (check_files.find(full_name) == check_files.end())
            return 1;

        check_files[full_name].checked = true;

        if (check_files[full_name].crc32_sum != crc32_sum)
            return 2;

        return 3;

    }  catch (...) {
    }
    return 0;
}

static char *get_unchecked_file() {

}

#define BUFFER_SIZE 1024
static uint32_t calc_file_crc32(const char *path, const char *file_name) {
    try {
        std::string full_name = std::string(path) + "/" + std::string(file_name);
        std::ifstream ifs(full_name.c_str(), std::ios_base::binary);
        if (ifs) {
            boost::crc_32_type result;
            do {
                char buffer[BUFFER_SIZE];
                ifs.read(buffer, BUFFER_SIZE);
                result.process_bytes(buffer, ifs.gcount());
            } while (ifs);


            return result.checksum();
        }
        // bad file
        return 0;
    } catch (...) {

    }
    // return when exception
    return 0;
}

static void check_files_in_directory(const char *path_to_dir) {
    DIR *d;
    struct dirent *dir;
    d = opendir(path_to_dir);
    if (!d) {
    } else {
        while ((dir = readdir(d)) != NULL) {
            if (dir->d_type != DT_REG)
                continue;
            uint32_t crc32_sum = calc_file_crc32(path_to_dir, dir->d_name);
            switch (check_file_info(path_to_dir, dir->d_name, crc32_sum)) {
            case 1:
                syslog(LOG_NOTICE, "new file %s\n", dir->d_name);
                break;
            case 2:
                syslog(LOG_NOTICE, "crc32 changed %s\n", dir->d_name);
                break;
            case 3:
                break;
            }
        }
        closedir(d);
    }
}

static void deamon_task(const char *path_to_dir)
{
    int i = 5;
    while (1) {
        sleep(5);
        check_files_in_directory(path_to_dir);
        syslog(LOG_NOTICE, "tik");
        if (i-- == 0)
            return;
    }
}

static int start_daemon(char *path_to_dir, int timeout_s) {
    pid_t pid, sid;
    pid = fork();
    if (pid < 0)
        return EXIT_FAILURE; // child process creation has failed
    else if (pid > 0)
        return EXIT_SUCCESS; // child creation ok, return to parent

    openlog(DAEMON_LOG_FILE, LOG_PID, LOG_DAEMON); // open log file

    sid = setsid(); // new session for the child process
    if (sid < 0) {
        return EXIT_FAILURE;
    }
/* TODO signal handler*/
    pid = fork(); // create daemon in new process (to ensure that daemon can never re-acquire a terminal)
    if (pid < 0)
        return EXIT_FAILURE; // daemon creation has failed
    else if (pid > 0)
        return EXIT_SUCCESS; // daemon creation ok, return to parent
    umask(0); // access to files created by the deamon

    if (chdir("/") < 0) // change the current working directory to root
        return EXIT_FAILURE;
    // close all file descs
    for (int fd = (int)sysconf(_SC_OPEN_MAX); fd >= 0; --fd)
        close(fd);
    // save dir's files info
    DIR *d;
    struct dirent *dir;
    d = opendir(path_to_dir);
    if (d) {
        while ((dir = readdir(d)) != NULL) {
            if (dir->d_type != DT_REG)
                continue;
            uint32_t crc32_sum = calc_file_crc32(path_to_dir, dir->d_name);
            save_file_info(path_to_dir, dir->d_name, crc32_sum);
        }
        closedir(d);
    }




    deamon_task(path_to_dir);
    return EXIT_SUCCESS;
}

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
            /*
        case '?':
            if (optopt == 'f')
                fprintf(stderr, "option -%c requires path to folder as argument\n", opt);
            break;
            */
        }
    }
    // if no arg try to get path from env
    if (!path_to_dir)
        path_to_dir = getenv(DAEMON_ENV_DIR);
    // no env variable for path
    if (!path_to_dir) {
        /* useage ... */
printf("fail\n");
        exit(EXIT_FAILURE);
    }
    struct stat sb;
    if (stat(path_to_dir, &sb) != 0 || !S_ISDIR(sb.st_mode)) {
printf("no such directory: %s\n", path_to_dir);
        exit(EXIT_FAILURE);
    }

    if (!timeout_s) {
        env_timeout = getenv(DAEMON_ENV_TIMEOUT);
        if (env_timeout)
            timeout_s = atoi(env_timeout);
    }

    if (timeout_s < 0)
        timeout_s = -timeout_s;

    if (!timeout_s)
    {
printf("wrong timeout: %d\n", timeout_s);
        exit(EXIT_FAILURE);
    }

printf("find dir %s and timeout %d\n", path_to_dir, timeout_s);

    return start_daemon(path_to_dir, timeout_s);
}
