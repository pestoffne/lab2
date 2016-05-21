#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>

#include "common.h"
#include "select.h"

static void handle_child(int signal, siginfo_t *siginfo, void *context) {
    chld_exit_code = signal;
    loop = 0;
}

void process_select(char * logfile, char * command) {
    struct sigaction sa;
    sa.sa_flags = SA_SIGINFO;
    sigset_t mask;
    sigfillset(&mask);
    sa.sa_mask = mask;
    sa.sa_sigaction = &handle_child;
    if (-1 == sigaction(SIGCHLD, &sa, NULL)) {
        fprintf(stderr, "%10d Fail while creating handler. %s\n", getpid(), strerror(errno));
        exit(2);
    }

    init_pipes();

    int log_fd = my_file_open(logfile);
    loop = 1;

    pid_t pid = fork();
    if (0 == pid) { // child
        if (-1 == dup2(pfd[0][0], 0)) {
            perror("Error while dup2(pfd[0][0], 0). ");
            exit(2);
        }
        if (-1 == dup2(pfd[1][1], 1)) {
            perror("Error while dup2(pfd[1][1], 1). ");
            exit(2);
        }
        if (-1 == dup2(pfd[2][1], 2)) {
            perror("Error while dup2(pfd[2][1], 2). ");
            exit(2);
        }
        my_execute(command);
    } else if (pid > 0) { // parent
        //// int fcntl(int fildes, int cmd, ...);
        //int i, flags;
        //for (i = 0; i < 3; i++) {
        //    flags = fcntl(pfd[i][0], F_GETFL); // TODO handle error? see man
        //    fprintf(stderr, "b flags = %o\n", flags); // DEBUG
        //    if (fcntl(pfd[i][0], F_SETFL, O_NONBLOCK)) {
        //        perror("fcntl");
        //        exit(2);
        //    }
        //    flags = fcntl(pfd[i][0], F_GETFL);
        //    fprintf(stderr, "a flags = %o\n", flags); // DEBUG
        //}

        fcntl(pfd[1][0], F_SETFD, O_NONBLOCK);
        fcntl(pfd[2][0], F_SETFL, O_NONBLOCK);

        do {
            struct timeval tv;
            tv.tv_sec = 1;
            tv.tv_usec = 0;

            fd_set readfds;
            FD_ZERO(&readfds);
            FD_SET(0, &readfds);

            fd_set writefds;
            FD_ZERO(&writefds);
            FD_SET(pfd[1][0], &writefds);
            FD_SET(pfd[2][0], &writefds);

            int retval = select(9, &readfds, &writefds, NULL, &tv);
            if (-1 == retval) {
                if (EINTR == errno) {
                    // do nothing
                } else {
                    perror("select. ");
                    exit(2);
                }
            } else if (0 == retval) {
                write_noio(log_fd);
                //write_noio(2);
            } else {
                if (FD_ISSET(0, &readfds)) {
                    fprintf(stderr, "read_avaible_c"); // DEBUG
                    read_avaible_c(0, pfd[0][1], log_fd);
                }
                if (FD_ISSET(pfd[1][0], &writefds)) {
                    read_avaible(pfd[1][0], 1, log_fd);
                }
                if (FD_ISSET(pfd[2][0], &writefds)) {
                    read_avaible(pfd[2][0], 2, log_fd);
                }
            }
        } while (loop);

        fprintf(stderr, "%10d TERMINATED WITH EXIT CODE: %d\n", getpid(), chld_exit_code);
    } else { // (-1 == pid) error
        fprintf(stderr, "%10d Pid is negative, after fork. %s\n", getpid(), strerror(errno));
        exit(2);
    }
}
