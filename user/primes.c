#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

void sub_process(int leftfds[]) {
    close(leftfds[1]);
    int num, p = 0;
    int rightfds[2] = {-1, -1};
    while (read(leftfds[0], &num, 4)) {
        if (p == 0) {
            printf("prime %d\n", num);
            p = num;
            continue;
        } 
        if (num % p == 0) continue;
        if (rightfds[0] == -1) {
            pipe(rightfds);
            if (fork() == 0) sub_process(rightfds);
            // for child can copy fds, we must close read end after fork().
            close(rightfds[0]);
        }

        write(rightfds[1], &num, 4);
    }
    close(leftfds[0]);
    close(rightfds[1]);
    // wait , if we have created a sub process
    if (rightfds[0] != -1) wait(0);
    exit(0);
}

int main() {
    printf("prime 2\n");

    int fds[2]; pipe(fds);
    if (fork() == 0) sub_process(fds); // sub_process will never return

    close(fds[0]);
    for (int i=3; i<=35; i++) {
        write(fds[1], &i, 4);
    }

    close(fds[1]);
    wait(0);
    exit(0);
}