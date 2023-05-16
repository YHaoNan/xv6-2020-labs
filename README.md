# Lab: system calls

## System call tracing (moderate)
> In this assignment you will add a system call tracing feature that may help you when debugging later labs. You'll create a new trace system call that will control tracing. It should take one argument, an integer "mask", whose bits specify which system calls to trace. For example, to trace the fork system call, a program calls trace(1 << SYS_fork), where SYS_fork is a syscall number from kernel/syscall.h. You have to modify the xv6 kernel to print out a line when each system call is about to return, if the system call's number is set in the mask. The line should contain the process id, the name of the system call and the return value; you don't need to print the system call arguments. The trace system call should enable tracing for the process that calls it and any children that it subsequently forks, but should not affect other processes.


We provide a trace user-level program that runs another program with tracing enabled (see user/trace.c). When you're done, you should see output like this:
```sh
$ trace 32 grep hello README
3: syscall read -> 1023
3: syscall read -> 966
3: syscall read -> 70
3: syscall read -> 0
$
```
```sh
$ trace 2147483647 grep hello README
4: syscall trace -> 0
4: syscall exec -> 3
4: syscall open -> 3
4: syscall read -> 1023
4: syscall read -> 966
4: syscall read -> 70
4: syscall read -> 0
4: syscall close -> 0
$
```
```sh
$ grep hello README
$
$ trace 2 usertests forkforkfork
usertests starting
test forkforkfork: 407: syscall fork -> 408
408: syscall fork -> 409
409: syscall fork -> 410
410: syscall fork -> 411
409: syscall fork -> 412
410: syscall fork -> 413
409: syscall fork -> 414
411: syscall fork -> 415
...
$   
```

完成过程：

user/user.h中添加trace的原型：
```diff
// user/user.h
// ...
int sleep(int);
int uptime(void);
+int trace(int);
// ...
```

kernel/syscall.h中添加trace的系统调用号`SYS_trace`：

```diff
// kernel/syscall.h
// ...
+#define SYS_trace  22
```

kernel/syscall.c中添加相关定义：
```diff
// kernel/syscall.c

// ...
+extern uint64 sys_trace(void);

static uint64 (*syscalls[])(void) = {
[SYS_fork]    sys_fork,
// ...
+[SYS_trace]   sys_trace,
};
```

在`kernel/proc.h`的`proc`结构体中添加一个字段，用于保存要跟踪的系统调用号掩码：
```diff
struct proc {
  struct spinlock lock;
  // ...
+  uint64  tracemask;           // The mask of tracing syscall
};

```

题目中要求，我们要在`kernel/sysproc.c`中添加`sys_trace`系统调用的函数体，该函数的工作很简单，将参数中的整数设置到`myproc()->tracemask`：

```c
uint64
sys_trace(void) {
  int mask;
  if (argint(0, &mask) < 0) 
    return -1;

  myproc()->tracemask = mask;
  return 0;
}
```


> `argint`实际上是读`a0~a5`寄存器，这应该是c risc-v的调用规约，详见文档：[riscv-calling](https://riscv.org/wp-content/uploads/2015/01/riscv-calling.pdf)

在`kernel/syscall.c`中，当系统调用结束后，我们需要通过掩码判断该系统调用是否应该被跟踪，若是，我们要打印出相应的内容：
```diff
+static char *syscall_names[] = {
+  "fork", "exit", "wait", "pipe", "read", "kill", "exec", "fstat", "chdir", "dup", "getpid", "sbrk",
+  "sleep", "uptime", "open", "write", "mknod", "unlink", "link", "mkdir", "close", "trace"
+};

void
syscall(void)
{
  int num;
  struct proc *p = myproc();

  num = p->trapframe->a7;
  if(num > 0 && num < NELEM(syscalls) && syscalls[num]) {
    p->trapframe->a0 = syscalls[num]();
+    if (p->tracemask & (1 << num)) {
+      printf("%d: syscall %s -> %d\n", p->pid, syscall_names[num], p->trapframe->a0);
+    }
  } else {
    printf("%d %s: unknown sys call %d\n",
            p->pid, p->name, num);
    p->trapframe->a0 = -1;
  }
}
```

> 函数调用的返回值保存到a0和a1，这也是riscv调用规约中规定的。

我不确定`p->tracemask`的初始值是啥，万一没有任何保证是一个随机值，有可能会有莫名奇妙的系统调用被跟踪，所以在进程初始化时设置成0：

```diff
// kernel/proc.c
static void
freeproc(struct proc *p)
{
  if(p->trapframe)
    kfree((void*)p->trapframe);
  p->trapframe = 0;
  if(p->pagetable)
    proc_freepagetable(p->pagetable, p->sz);
  p->pagetable = 0;
  p->sz = 0;
  p->pid = 0;
  p->parent = 0;
  p->name[0] = 0;
  p->chan = 0;
  p->killed = 0;
  p->xstate = 0;
  p->state = UNUSED;
+  p->tracemask = 0;
}
```

> 在初始化时，所有进程都会调用一次`freeproc`。

lab要求中还要求子进程中的系统调用也要被跟踪，所以在`fork`时，我们需要把`tracemask`复制给子进程：
```diff
// kernal/proc.c

int
fork(void)
{
  int i, pid;
  struct proc *np;
  struct proc *p = myproc();

  // ...
  np->sz = p->sz;

  np->parent = p;
+  np->tracemask = p->tracemask;

  // ...
  return pid;
}
```


最后一个问题，用户代码里有`trace`的调用，可是现在该函数只有原型，我们需要提供一个`trace`函数的实体，它会发起系统调用并将系统调用号存到a7，系统调用参数存到a0，最后跳转到`kernel/syscall.c`中的`syscall`函数。`user/usys.pl`是生成这些包装函数实体的的perl脚本：

```diff
// user/usys.pl
print "# generated by usys.pl - do not edit\n";

print "#include \"kernel/syscall.h\"\n";

sub entry {
    my $name = shift;
    print ".global $name\n";
    print "${name}:\n";
    print " li a7, SYS_${name}\n";
    print " ecall\n";
    print " ret\n";
}
	
entry("fork");
// ...
+entry("trace");
```

Makefile中包含了对该脚本的调用，该脚本生成所有系统调用的包装函数到`user/usys.S`，该文件稍后会参加后续的编译链接过程：

最后还得把框架代码中提供的`user/trace.c`添加到Makefile的`UPROGS`中，这里就不写了。

## Sysinfo (moderate)

> In this assignment you will add a system call, sysinfo, that collects information about the running system. The system call takes one argument: a pointer to a struct sysinfo (see kernel/sysinfo.h). The kernel should fill out the fields of this struct: the freemem field should be set to the number of bytes of free memory, and the nproc field should be set to the number of processes whose state is not UNUSED. We provide a test program sysinfotest; you pass this assignment if it prints "sysinfotest: OK".

当你为xv6实现一个系统调用时，你要做一些重复的操作，这些重复的操作在上面一个实验中已经有了，这个实验只会说和实现sysinfo紧密相关的那些。

在`kernel/kalloc.c`和`kernel/proc.c`中分别维护和内存大小、进程数量相关的变量，并向外部提供函数获取它。在内存分配中，每次分配一个页面，`freelist`的锁会保护计数器的更新，在进程管理中，分配和释放进程结构都需要持有进程的锁，我没有太仔细地阅读代码，但我认为当前的实现依然是正确的。

这里不放`kfreemem`和`proc_count`的实现了，可以自己去代码里找，只放`sys_sysinfo`的实现：
```c
// kernel/sysproc.c
uint64 
sys_sysinfo(void) {
  uint64 user_pointer;
  if (argaddr(0, &user_pointer) < 0) 
    return -1;
  struct sysinfo si = {kfreemem(), proc_count()};
  if(copyout(myproc()->pagetable, user_pointer, (char *)&si, sizeof(si)) < 0) {
    return -1;
  }
  return 0;
}
```

一个有趣的点是我们需要`copyout`函数将处于内核页表中的`struct sysinfo`复制到用户提供的指针上，对于特别大的数据结构，内核态到用户态的复制可能会很耗时。现存的系统中可能会有某种zero copy技术？