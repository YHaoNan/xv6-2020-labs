#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int main() {
    int fds[2]; // 0 refers to read end, 1 write end;
    pipe(fds);
    char ch;

    int pid = fork();

    if (pid != 0) {
        // parent

        write(fds[1], "x", 1);
        // wait child exit. otherwise parent may read the char
        wait(0);
        read(fds[0], &ch, 1);
        printf("%d: received pong\n", getpid());
        close(fds[0]);
        close(fds[1]);
        exit(0);
    } else {
        // child
        read(fds[0], &ch, 1);
        printf("%d: received ping\n", getpid());
        write(fds[1], &ch, 1);
        close(fds[0]);
        close(fds[1]);
        exit(0);
    }

}