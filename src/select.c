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

int chld_exit_code;
int pfd[3][2];
char loop;

static void handle_child(int signal, siginfo_t *siginfo, void *context) {
    chld_exit_code = signal;
    loop = 0;
}

void process_select(char * logfile, char * command) {
    fprintf(stderr, "%10d process_select(\"%s\", \"%s\");\n", getpid(), logfile, command);
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

    int i;
    for (i = 0; i < 3; i++) {
        if (-1 == pipe(pfd[i])) {
            fprintf(stderr, "%10d Error while pipe. %s\n", getpid(), strerror(errno));
            exit(2);
        }
    }

    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(pfd[0][0], &fds);
    FD_SET(pfd[1][0], &fds);
    FD_SET(pfd[2][0], &fds);
    int fd_sup = 9;

    loop = 1;

    pid_t pid = fork();
    if (0 == pid) { // child
        if (-1 == dup2(pfd[0][1], 0)) {
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
        for (i = 0; i < 3; i++) {
            int flags = fcntl(pfd[i][0], F_GETFL, 0); // TODO handle error? see man
            if (fcntl(pfd[i][0], F_SETFL, flags | O_NONBLOCK)) {
                perror("fcntl");
                exit(2);
            }
        }

        do {
            struct timeval tv;
            tv.tv_sec = 1;
            tv.tv_usec = 0;

            int count;
            int retval = select(fd_sup, &fds, NULL, NULL, &tv);
            if (-1 == retval) {
                if (EINTR != errno) {
                    perror("select. ");
                    exit(2);
                } // else if (EINTR == errno) { do nothing }
            } else if (0 == retval) {
                fprintf(stderr, "DATE/TIME, NOIO\n"); // TODO
            } else if (FD_ISSET(pfd[0][0], &fds)) {
                while (1) {
                    count = read(pfd[0][0], buffer, READ_BUFFER_SIZE);
                    if (-1 == count) {
                        if (EAGAIN == errno) {
                            fprintf(stderr, "break EAGAIN\n"); // DEBUG
                            break;
                        } else if (EINTR != errno) {
                            perror("read. ");
                            exit(2);
                        } // else if (EINTR == errno) { do nothing }
                    } else if (0 == count) {
                        fprintf(stderr, "break 0 == count\n"); // DEBUG
                        break;
                    } else { // (count > 0)
                        buffer[count] = 0;
                        write_buffer(0, buffer);
                    }
                }
            } else if (FD_ISSET(pfd[1][0], &fds)) {
                while (1) {
                    count = read(pfd[1][0], buffer, READ_BUFFER_SIZE);
                    if (-1 == count) {
                        if (EAGAIN == errno) {
                            fprintf(stderr, "break EAGAIN\n"); // DEBUG
                            break;
                        } else if (EINTR != errno) {
                            perror("read. ");
                            exit(2);
                        } // else if (EINTR == errno) { do nothing }
                    } else if (0 == count) {
                        fprintf(stderr, "break 0 == count\n"); // DEBUG
                        break;
                    } else { // (count > 0)
                        buffer[count] = 0;
                        write_buffer(1, buffer);
                    }
                }
            } else if (FD_ISSET(pfd[2][0], &fds)) {
                while (1) {
                    count = read(pfd[2][0], buffer, READ_BUFFER_SIZE);
                    if (-1 == count) {
                        if (EAGAIN == errno) {
                            fprintf(stderr, "break EAGAIN\n"); // DEBUG
                            break;
                        } else if (EINTR != errno) {
                            perror("read. ");
                            exit(2);
                        } // else if (EINTR == errno) { do nothing }
                    } else if (0 == count) {
                        fprintf(stderr, "break 0 == count\n"); // DEBUG
                        break;
                    } else { // (count > 0)
                        buffer[count] = 0;
                        write_buffer(2, buffer);
                    }
                }
            }
        } while (loop);

        fprintf(stderr, "%10d TERMINATED WITH EXIT CODE: %d\n", getpid(), chld_exit_code);
    } else { // (-1 == pid) error
        fprintf(stderr, "%10d Pid is negative, after fork. %s\n", getpid(), strerror(errno));
        exit(2);
    }
}