#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/param.h"

int main(int argc, char *argv[]) {

    if (argc <= 1) {
        fprintf(2, "Usage. xargs <original command> [original parameters]\n");
        exit(1);
    }

    char buf[1024];
    char *params[MAXARG];

    memset(params, sizeof(params), 0);

    int curparam_pos = 0; // 控制当前的参数在params数组中的位置
    int curparam_index = 0; // 控制当前参数的下一个字符下标

    read(0, buf, 1024);   // 假设输入最多1024个字符

    char *call_prog = argv[1];
    params[curparam_pos++] = call_prog;
    for (int i=2; i<argc; i++) {
        params[curparam_pos++] = argv[i];
    }

    for (int i=0; i<1024; i++) {
        char ch = buf[i];
        if (ch == '\n' || ch == ' ') {
            params[curparam_pos][curparam_index] = '\0';
            curparam_index = 0; curparam_pos++;
            if (ch == '\n') {
                params[curparam_pos] = 0;
                if (fork() == 0) {
                    exec(call_prog, params);
                } else {
                    wait(0);
                    memset(params, sizeof(params), 0);
                    curparam_pos = 0;
                    params[curparam_pos++] = call_prog;
                    for (int i=2; i<argc; i++) {
                        params[curparam_pos++] = argv[i];
                    }
                }
            }
        } else  {
            if (params[curparam_pos]==0) {
                params[curparam_pos] = malloc(sizeof(char) * 1024);
            }
            params[curparam_pos][curparam_index++] = ch;
        }
    }

    exit(0);
}