#ifndef DAEMON_HEADER
#define DAEMON_HEADER

#ifdef __cplusplus
extern "C" {
#endif

// start observing directory
// param[in] path_to_dir - path to observing directory
// param[in] timeous_s - deamon's timeout in sec
// return operation's result: 0 - deamon started without errors; 1 - error
int start_daemon(char *path_to_dir, int timeout_s);

#ifdef __cplusplus
}
#endif

#endif // DAEMON_HEADER
