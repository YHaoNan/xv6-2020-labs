#include "kernel/types.h"
#include "kernel/riscv.h"
#include "kernel/param.h"
#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/memlayout.h"

int ugetpid() {
    struct usyscall *usc = (struct usyscall*)USYSCALL;
    return usc->pid;
}

int main() {
    printf("getpid: %d, ugetpid: %d\n", getpid(), ugetpid());

    if (fork() == 0) {
        printf("sub getpid: %d, ugetpid: %d\n", getpid(), ugetpid());
        exit(0);
    }

    wait(0);
    exit(0);
}