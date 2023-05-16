#include "kernel/types.h"
#include "kernel/riscv.h"
#include "kernel/sysinfo.h"
#include "user/user.h"

int main() {
    struct sysinfo si;
    sysinfo(&si);
    printf("proc %d, freemem %d\n", si.nproc, si.freemem);
    exit(0);
}