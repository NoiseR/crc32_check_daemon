#include <sys/types.h>
#include <sys/stat.h>
#include <sys/msg.h>
#include <unistd.h>
#include <stdlib.h>
#include <signal.h>
#include <syslog.h>
#include <dirent.h>
#include <signal.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <time.h>
#include <linux/limits.h>

#include "crc32.h"
#include "file_repo.h"

// deamon's log
#define DAEMON_LOG_FILE  "crc32_check_daemon"

// check request type for queue
#define MSG_TYPE 1

// data type for request
typedef enum {
    CHECK_REQUEST,
    STOP_DAEMON
} RequestData;

// request type for queue
typedef struct {
   long type;
   char data;
} CheckRequest;

// queue id for check directory request
static int queue_id;

// path to directory
static char path_to_check_directory[PATH_MAX];

// timer for periodical check
static timer_t timerid;

static void request_for_check();
static void sig_on();

static void sig_handler(int signo) {
    if (signo == SIGUSR1) { //request from user
        request_for_check(CHECK_REQUEST);
    } else if (signo == SIGRTMIN) { // request from timer
        request_for_check(CHECK_REQUEST);
    } else if (signo == SIGTERM) {
        request_for_check(STOP_DAEMON);
    }
    sig_on();
}

// general signal init
static void (*mysignal(int signo, void (*hndlr)(int)))(int) {
    struct sigaction act, oact;
    sigemptyset(&act.sa_mask);
    act.sa_flags = 0;
    if (signo == SIGUSR1 || signo == SIGRTMIN || signo == SIGTERM) {
        act.sa_handler = hndlr;
        act.sa_flags |= SA_RESTART;
    } else {
        act.sa_handler = SIG_IGN;
    }
    if (sigaction(signo, &act, &oact) < 0) {
        return (SIG_ERR);
    }
    return (oact.sa_handler);
}

// deamon's signal init
static void sig_on() {
    if (mysignal(SIGUSR1, sig_handler) == SIG_ERR) {
        syslog(LOG_ERR, "[ERROR] SIGUSR1 handling\n");
    }
    if (mysignal(SIGQUIT, sig_handler) == SIG_ERR) {
        syslog(LOG_ERR, "[ERROR] SIGQUIT handling\n");
    }
    if (mysignal(SIGINT, sig_handler) == SIG_ERR) {
        syslog(LOG_ERR, "[ERROR] SIGINT handling\n");
    }
    if (mysignal(SIGHUP, sig_handler) == SIG_ERR) {
        syslog(LOG_ERR, "[ERROR] SIGHUP handling\n");
    }
    if (mysignal(SIGCONT, sig_handler) == SIG_ERR) {
        syslog(LOG_ERR, "[ERROR] SIGCONT handling\n");
    }
    if (mysignal(SIGRTMIN, sig_handler) == SIG_ERR) {
        syslog(LOG_ERR, "[ERROR] SIGRTMIN handling\n");
    }
    if (mysignal(SIGTERM, sig_handler) == SIG_ERR) {
        syslog(LOG_ERR, "[ERROR] SIGTERM handling\n");
    }
}

// save start information
static void init_directory_info(const char *path_to_dir) {
    DIR *d;
    struct dirent *dir;
    d = opendir(path_to_dir);
    if (d) {
        while ((dir = readdir(d)) != NULL) {
            if (dir->d_type != DT_REG)
                continue;
            uint32_t crc32_sum = calc_file_crc32(path_to_dir, dir->d_name);
            push_file(dir->d_name, crc32_sum);
        }
        closedir(d);
    }
}

// check present information
static void check_files_in_directory(const char *path_to_dir) {
    int fails = 0;
    DIR *d;
    struct dirent *dir;
    d = opendir(path_to_dir);
    if (!d) { // no directory. will print all files as deleted
    } else {
        while ((dir = readdir(d)) != NULL) {
            if (dir->d_type != DT_REG)
                continue;
            uint32_t crc32_sum = calc_file_crc32(path_to_dir, dir->d_name);
            switch (check_file_attr(dir->d_name, crc32_sum)) {
            case FILE_NOT_FOUND:
                fails++;
                syslog(LOG_NOTICE,
                       "Integrity check: FAIL (%s - new file)\n",
                       dir->d_name);
                break;
            case ATTR_CHANGED:
                fails++;
                syslog(LOG_NOTICE,
                       "Integrity check: FAIL (%s - new crc 0x%X, old crc 0x%X)\n",
                       dir->d_name,
                       crc32_sum,
                       get_file_attr(dir->d_name));
                break;
            case VALID_ATTR:
                break;
            case CHECK_ERROR:
            default:
                syslog(LOG_ERR, "[ERROR] check directory\n");
                break;
            }
        }
        closedir(d);
    }
    const char *name = NULL;
    while ((name = get_next_unchecked_file()) != NULL) {
        syslog(LOG_NOTICE,
               "Integrity check: FAIL (%s - file was deleted)\n",
               name);
        fails++;
    }
    if (fails == 0)
        syslog(LOG_NOTICE, "Integrity check: OK\n");
}

// request for dir's checking
static void request_for_check(int request) {
    CheckRequest msg;
    msg.type = MSG_TYPE;
    msg.data = request;
    while (1) {
        if (msgsnd(queue_id, &msg, sizeof(msg.data), 0) == -1) {
            switch (errno) {
            case EINTR:
                continue;
            default:
                syslog(LOG_ERR, "[ERROR] request for check failed %d\n", errno);
                return;
            }
        } else {
            return; // ok
        }
    }
}

static int init_daemon(char *path_to_dir, int timeout_s) {
    // save path
    int cpy_n = snprintf(path_to_check_directory, PATH_MAX, "%s", path_to_dir);
    if (cpy_n < 0 || cpy_n > PATH_MAX) {
        syslog(LOG_ERR, "[ERROR] save path to directory failed\n");
        return EXIT_FAILURE;
    }
    // create queue
    key_t key;
    if ((key = ftok(".", 'm')) == -1) {
        syslog(LOG_ERR, "[ERROR] get key failed\n");
        return EXIT_FAILURE;
    }
    if ((queue_id = msgget(key, IPC_CREAT | 0666)) == -1) {
        syslog(LOG_ERR, "[ERROR] create queue failed\n");
        return EXIT_FAILURE;
    }
    // create timer
    struct sigevent sev;
    sev.sigev_notify = SIGEV_SIGNAL;
    sev.sigev_signo = SIGRTMIN;
    sev.sigev_value.sival_ptr = &timerid;
    while (1) {
        if (timer_create(CLOCK_REALTIME, &sev, &timerid) == -1) {
            switch (errno) {
            case EAGAIN:
                continue;
            default:
                syslog(LOG_ERR, "[ERROR] create timer failed %d\n", errno);
                return EXIT_FAILURE;
            }
        } else {
            break;
        }
    }
    // start timer
    struct itimerspec its;
    its.it_value.tv_sec = timeout_s;
    its.it_value.tv_nsec = 0;
    its.it_interval.tv_sec = its.it_value.tv_sec;
    its.it_interval.tv_nsec = its.it_value.tv_nsec;
    if (timer_settime(timerid, 0, &its, NULL) == -1) {
        syslog(LOG_ERR, "[ERROR] start timer failed\n");
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

static void deinit_daemon() {
    struct msqid_ds msg_ctl;
    if (msgctl(queue_id, IPC_RMID, &msg_ctl) == -1)
        syslog(LOG_ERR, "[ERROR] delete queue failed\n");
}

static int deamon_task() {
    while (1) {
        CheckRequest msg;
        int result = msgrcv(queue_id, &msg, sizeof(msg) - sizeof(long), MSG_TYPE, 0);
        if (result < 0) {
            switch (errno) {
            case EINTR:
                continue;
            default:
                syslog(LOG_ERR, "[ERROR] receive request failed\n");
                return EXIT_FAILURE;
            }
        } else {
            switch (msg.data) {
            case CHECK_REQUEST:
                check_files_in_directory(path_to_check_directory);
                break;
            case STOP_DAEMON:
                deinit_daemon();
                syslog(LOG_NOTICE, "The deamon was stopped\n");
                return EXIT_SUCCESS;
                break;
            default:
                deinit_daemon();
                syslog(LOG_ERR, "[ERROR] stop deamon\n");
                return EXIT_FAILURE;
                break;
            }
        }
    }
}

int start_daemon(char *path_to_dir, int timeout_s) {
    pid_t pid, sid;
    pid = fork();
    if (pid < 0)
        return EXIT_FAILURE; // child process creation has failed
    else if (pid > 0)
        return EXIT_SUCCESS; // child creation ok, return to parent
    // open log file
    openlog(DAEMON_LOG_FILE, LOG_PID, LOG_DAEMON);
    // close all file descs
    for (int fd = (int)sysconf(_SC_OPEN_MAX); fd >= 0; --fd)
        close(fd);
    // new session for the child process
    sid = setsid();
    if (sid < 0) {
        syslog(LOG_ERR, "[ERROR] get new session failed\n");
        return EXIT_FAILURE;
    }
    // start signal handling
    sig_on();
    // create daemon in new process (to ensure that daemon can never re-acquire a terminal)
    pid = fork();
    if (pid < 0) {
        syslog(LOG_ERR, "[ERROR] fork 1 deamon failed\n");
        return EXIT_FAILURE;
    }
    else if (pid > 0) {
        return EXIT_SUCCESS;
    }
    // access to files created by the deamon
    umask(0);
    // change the current working directory to root
    if (chdir("/") < 0) {
        syslog(LOG_ERR, "[ERROR] changing working directory failed\n");
        return EXIT_FAILURE;
    }
    // load information about the directory
    init_directory_info(path_to_dir);
    // init daemon
    if (init_daemon(path_to_dir, timeout_s) == EXIT_FAILURE)
        return EXIT_FAILURE;
    // starting deamon
    return deamon_task();
}
