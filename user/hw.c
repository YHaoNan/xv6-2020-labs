#include "kernel/param.h"
#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int x(int a, int b, int c) {
  return a+b+c;
}
int main() {
  x(1,4,6);
  printf("x=%d y=%d", 3);
  exit(0);
}
