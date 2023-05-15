#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"

void find(int dirfd, char *path, char *match) {
    struct dirent de;
    int fd;
    struct stat st;

    int pl = strlen(path);

    while (read(dirfd, &de, sizeof(de)) == sizeof(de)) {
        if (de.inum == 0) continue;
        if (strcmp(de.name, ".") == 0) continue;
        if (strcmp(de.name, "..") == 0) continue;

        int ppl = strlen(de.name);
        char *newpath = malloc(pl + ppl + 2);
        strcpy(newpath, path);
        newpath[pl] = '/';
        strcpy(newpath + pl + 1, de.name);

        if ((fd = open(newpath, 0)) < 0) continue;
        if (fstat(fd, &st) < 0) continue;
        if (st.type == T_DIR) {
            find(fd, newpath, match);
        } else {
            if (strcmp(de.name, match) == 0) {
                fprintf(1, "%s\n", newpath);
            }
        }
        close(fd);
        free(newpath);
    }
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(2, "Usage. find <path> <filename>\n");
        exit(1);
    }

    int fd;

    struct stat st;

    if ((fd = open(argv[1], 0)) < 0) {
        fprintf(2, "find: cannot open %s\n", argv[1]);
        exit(1);
    }

    if(fstat(fd, &st) < 0){
        fprintf(2, "find: cannot stat %s\n", argv[1]);
        exit(1);
    }

    if (st.type != T_DIR) {
        fprintf(2, "find: %s is not a directory\n", argv[1]);
        exit(1);
    }

    find(fd, argv[1], argv[2]);
    close(fd);

    exit(0);
}