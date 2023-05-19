# Lab: traps

## RISC-V assembly (easy)
It will be important to understand a bit of RISC-V assembly, which you were exposed to in 6.004. There is a file user/call.c in your xv6 repo. make fs.img compiles it and also produces a readable assembly version of the program in user/call.asm.

Read the code in call.asm for the functions g, f, and main. The instruction manual for RISC-V is on the [reference page](https://pdos.csail.mit.edu/6.S081/2020/reference.html). Here are some questions that you should answer (store the answers in a file answers-traps.txt):

> Which registers contain arguments to functions? For example, which register holds 13 in main's call to printf?
> 
> 在RISC-V中，a0-a7（x10-x17）用于保存方法调用的参数。根据`user/call.asm`中的汇编代码，可以看出a2保存了参数13，而且`f(8)+1`实际上被编译器优化成了返回值12，a0应该保存了格式化字符串的指针
> ```asm
>  24:	4635                	li	a2,13
>  26:	45b1                	li	a1,12
>  28:	00000517          	auipc	a0,0x0
>  2c:	7a050513          	addi	a0,a0,1952 # 7c8 <malloc+0xe8>
> ```


> Where is the call to function f in the assembly code for main? Where is the call to g? (Hint: the compiler may inline functions.)
>
> 在汇编代码中没有到f和到g的调用，因为它们可以被内联。f(x) = g(x)，g(x) = x + 3，那么f(8)就等于8+3=12。在上一个问题中也看到了编译器直接将12加载到了a1上。

> At what address is the function printf located?
> 
> printf通过静态链接被链接到了可执行程序call中，通过相对定位的方式调用printf，也就是当前pc+1528
> ```c
> 30:	00000097          	auipc	ra,0x0
> 34:	5f8080e7          	jalr	1528(ra) # 628 <printf>
> ```

> What value is in the register ra just after the jalr to printf in main?
>
> 是`jalr`的下一行语句，也就是`printf`函数的返回地址。

> Run the following code.
> ```c
> unsigned int i = 0x00646c72;
> printf("H%x Wo%s", 57616, &i);
> ```      
> What is the output? [Here's an ASCII table](http://web.cs.mun.ca/~michael/c/ascii-table.html) that maps bytes to characters.  
> The output depends on that fact that the RISC-V is little-endian. If the RISC-V were instead big-endian what would you set i to in order to yield the same output? Would you need to change 57616 to a different value?
> 
> [Here's a description of little- and big-endian](http://www.webopedia.com/TERM/b/big_endian.html) and [a more whimsical description](http://www.networksorcery.com/enp/ien/ien137.txt).
>
> 输出是`He110 World`。0x64=100，在ascii中对应字符d，0x6c对应l，0x72对应r：
> ```py
> # in python interpreter
> >>> chr(0x72) + chr(0x6c) + chr(0x64)
> 'rld'
> ```
> 如果采用大端法，则i需要改成0x726c64。
> 
> 57616不用改变，大小端法是决定机器如何解释字节，57616在大端法和小端法中是不一样的字节。不过无论怎样，57616都是十六进制的e110。

> In the following code, what is going to be printed after 'y='? (note: the answer is not a specific value.) Why does this happen?
> ```c
> printf("x=%d y=%d", 3);
> ```
> 在我的机器上，它总是输出5299。
> 
> 这实际上取决于`printf`方法调用时寄存器a2的值。如果我们在调用前加入一个到其它函数的调用：
> ```c
> x(1,4,6);
> printf("x=%d y=%d", 3);
> ```
> 调用`x`会把a2设置成6，这样`printf`就会输出`x=3 y=6`。（需要在Makefile的CFLAGS中关闭编译器优化并make clean）


## Backtrace (moderate)

For debugging it is often useful to have a backtrace: a list of the function calls on the stack above the point at which the error occurred.

> Implement a backtrace() function in kernel/printf.c. Insert a call to this function in sys_sleep, and then run bttest, which calls sys_sleep. Your output should be as follows:
> ```
> backtrace:
> 0x0000000080002cda
> 0x0000000080002bb6
> 0x0000000080002898
> ```
> After bttest exit qemu. In your terminal: the addresses may be slightly different but if you run addr2line -e kernel/kernel (or riscv64-unknown-elf-addr2line -e kernel/kernel) and cut-and-paste the above addresses as follows:
> ```sh
> $ addr2line -e kernel/kernel
> 0x0000000080002de2
> 0x0000000080002f4a
> 0x0000000080002bfc
> Ctrl-D
> ```
> 
> You should see something like this:
> ```
> kernel/sysproc.c:74
> kernel/syscall.c:224
> kernel/trap.c:85
> ```

### 思路

Hint中给了一个[课堂笔记](https://pdos.csail.mit.edu/6.828/2020/lec/l-riscv-slides.pdf)，其中包含栈的样子：

![栈是什么样的](./scshoot01.png)

栈中包含若干个栈帧（stack frame），每一个方法调用就是一个栈帧。fp（frame pointer）寄存器指向当前栈帧的顶部。除此之外，每一个栈帧的第二个槽位保存了上一个栈帧的fp。所以我们就可以通过读当前fp寄存器，然后递归的读栈上的第二个槽位找到前一个栈帧即可完成实验。

gcc用s0作为frame pointer。

**Step1. 在kernel/risc-v.h中添加读取fp寄存器的函数**

```c
// kernel/risc-v.c
// read the current frame pointer
static inline uint64
r_fp() {
  uint64 x;
  asm volatile("mv %0, s0" : "=r" (x) );
  return x;
}
```

**Step2. 在printf.c中添加backtrace函数**

我们可以通过判断fp是不是在当前栈中来结束循环，因为xv6的栈只占用一页，我们可以通过`PGROUNDUP`来获得栈顶位置，`PGROUNDDOWN`获得栈底位置，若某一次得到的下一个fp地址不在栈中，就跳出循环。

```c
// kernel/printf.c
uint64 read_slot(uint64 fp, int slot) {
  int offset = (slot+1) * 8;
  return *((uint64*)(fp - offset));
}

void backtrace() {
  uint64 curr_fp = r_fp(), sttop = PGROUNDUP(curr_fp), stbottom = PGROUNDDOWN(curr_fp);
  printf("backtrace\n");
  while(curr_fp < sttop && curr_fp > stbottom) {
    printf("%p\n", read_slot(curr_fp, 0));
    curr_fp = read_slot(curr_fp, 1);
  }
}
```

**Step.3 向sys_sleep、defs.h、Makefile中添加内容**

略

## Alarm (hard)

> In this exercise you'll add a feature to xv6 that periodically alerts a process as it uses CPU time. This might be useful for compute-bound processes that want to limit how much CPU time they chew up, or for processes that want to compute but also want to take some periodic action. More generally, you'll be implementing a primitive form of user-level interrupt/fault handlers; you could use something similar to handle page faults in the application, for example. Your solution is correct if it passes alarmtest and usertests.

You should add a new sigalarm(interval, handler) system call. If an application calls sigalarm(n, fn), then after every n "ticks" of CPU time that the program consumes, the kernel should cause application function fn to be called. When fn returns, the application should resume where it left off. A tick is a fairly arbitrary unit of time in xv6, determined by how often a hardware timer generates interrupts. If an application calls sigalarm(0, 0), the kernel should stop generating periodic alarm calls.

You'll find a file user/alarmtest.c in your xv6 repository. Add it to the Makefile. It won't compile correctly until you've added sigalarm and sigreturn system calls (see below).

alarmtest calls sigalarm(2, periodic) in test0 to ask the kernel to force a call to periodic() every 2 ticks, and then spins for a while. You can see the assembly code for alarmtest in user/alarmtest.asm, which may be handy for debugging. Your solution is correct when alarmtest produces output like this and usertests also runs correctly:

```sh
$ alarmtest
test0 start
........alarm!
test0 passed
test1 start
...alarm!
..alarm!
...alarm!
..alarm!
...alarm!
..alarm!
...alarm!
..alarm!
...alarm!
..alarm!
test1 passed
test2 start
................alarm!
test2 passed
$ usertests
...
ALL TESTS PASSED
$
```

### test0: invoke handler

Get started by modifying the kernel to jump to the alarm handler in user space, which will cause test0 to print "alarm!". Don't worry yet what happens after the "alarm!" output; it's OK for now if your program crashes after printing "alarm!".

**Step1. 预备工作**

预备工作，添加系统调用，以及向Makefile的UPROG里加东西。这里不写出来了。

**Step2. 向proc结构体中添加相应字段，编写sys_sigalarm**

```c
// kernel/proc.h
struct proc {
  // ...
  int sigticks;                // 需要多少个ticks调用一次sighandler
  int ticks_sincelastcall;     // 距离上次调用sighandler已经有了多少ticks
  void (*sighandler)();        // 当ticks == sigticks时，要调用的函数指针。
                               // 有些情况下用户传入的handler函数在0的位置，我们认为，只有当sighandler和sigtick共同为0时，alarm功能是关闭的。
};
```

`sys_sigalarm`里面没啥东西，就是将用户传入系统调用的参数读出来，设置到进程结构中：
```c
uint64
sys_sigalarm(void) {

  int ticks;
  uint64 handler;
  struct proc *p = myproc();

  if (argint(0, &ticks) < 0 || argaddr(1, &handler) < 0) {
    return -1;
  }

  p->sigticks = ticks;
  p->ticks_sincelastcall = 0;
  p->sighandler = (void (*)())handler;

  return 0;
}
```

**Step3. 在usertrap中监控ticks**

> Every tick, the hardware clock forces an interrupt, which is handled in usertrap() in kernel/trap.c. You only want to manipulate a process's alarm ticks if there's a timer interrupt; you want something like：
> ```c
> if(which_dev == 2) ...
> ```

在usertrap中，我们检测来自中断设备的信号，每次中断视作一个tick。若tick数已经达到预期，我们就通过将`p->trapframe->epc`设置到handler函数的位置，以让控制流返回到用户空间时从这里开始执行。为了恢复以前的epc，我们还需要另一个变量来保存老的epc，这里我使用了一个`oepc`：

```c
// kernel/trap.c   usertrap function
  if(p->killed)
    exit(-1);

  // give up the CPU if this is a timer interrupt.
  if(which_dev == 2) {
    if (p->sigticks != 0 || p->sighandler != 0) {
      if ((++p->ticks_sincelastcall) == p->sigticks) {
        p->ticks_sincelastcall = 0;
        p->oepc = p->trapframe->epc;
        p->trapframe->epc = p->sighandler;
      }
    }
    yield();
  }
```

那么在`sys_sigreturn`中要做的事也明了了，将`oepc`恢复给`p->trapframe->epc`：
```c
uint64
sys_sigreturn(void) {
  struct proc *p = myproc();
  p->trapframe->epc = p->oepc;
  return 0;
}
```

运行：
```sh
$ alarmtest
test0 start
...........alarm!
test0 passed
usertrap(): unexpected scause 0x000000000000000c pid=3
            sepc=0x0000000000013f50 stval=0x0000000000013f50
```

test0已经通过了，但是在usertrap中抛出了一个异常，`0xc`是一个Instruction Page Fault。

### test1/test2(): resume interrupted code

Chances are that alarmtest crashes in test0 or test1 after it prints "alarm!", or that alarmtest (eventually) prints "test1 failed", or that alarmtest exits without printing "test1 passed". To fix this, you must ensure that, when the alarm handler is done, control returns to the instruction at which the user program was originally interrupted by the timer interrupt. You must ensure that the register contents are restored to the values they held at the time of the interrupt, so that the user program can continue undisturbed after the alarm. Finally, you should "re-arm" the alarm counter after each time it goes off, so that the handler is called periodically.

As a starting point, we've made a design decision for you: user alarm handlers are required to call the sigreturn system call when they have finished. Have a look at periodic in alarmtest.c for an example. This means that you can add code to usertrap and sys_sigreturn that cooperate to cause the user process to resume properly after it has handled the alarm.

考虑`sig_handler`的执行流程：
1. 定时器中断发生
2. trampoline代码会保存所有用户寄存器，然后进入usertrap
3. usertrap中，我们发现tick到达预期值，将`p->trapframe->epc`改成handler的地址
4. usertrapret返回到用户空间，从`handler`地址开始执行

但是这中间有一些问题，既然`handler`是一个由编译器编译的C函数，它就一定遵循C在riscv上的Calling Convention，我们应该将所有Caller Saved寄存器保存，因为`handler`中会更改它们。

它们分别是：`ra`、`t0~t6`、`a0~a7`。实际上还有浮点数计数器，不过xv6中没用到（trapframe中都没有），我们可以不用考虑。

我们再拿一个trapframe给proc结构体，用于保存这些寄存器状态：
```c
// kernel/proc.h    struct proc
int sigticks;                // 需要多少个ticks调用一次sighandler
int ticks_sincelastcall;     // 距离上次调用sighandler已经有了多少ticks
uint64 sighandler;           // 当ticks == sigticks时，要调用的函数指针。我们认为，当sighandler和sigtick共同为0时，alarm功能是关闭的。
struct trapframe *trapframe4sig; // data page for trampoline.S
```

别忘了在`allocproc`中分配它，以及在`freeproc`中释放它：

```c
// kernel/proc.c   allocproc
if((p->trapframe = (struct trapframe *)kalloc()) == 0){
  release(&p->lock);
  return 0;
}
// Allocate a trapframe page for sigalarm
if((p->trapframe4sig = (struct trapframe *)kalloc()) == 0){
  release(&p->lock);
  return 0;
}
```

```c
// kernel/proc.c   freeproc
static void
freeproc(struct proc *p)
{
  if(p->trapframe)
    kfree((void*)p->trapframe);
  if(p->trapframe4sig)
    kfree((void*)p->trapframe4sig);
  // ...
}
```



在`usertrap`中保存那些callersaved寄存器，在`sys_sigreturn`中恢复这些寄存器，既然trapframe已经创建，我们就来个简单的，也不考虑要保存callersaved还是啥了，直接全部复制：
```c
// kernel/trap.c   usertrap
  if(which_dev == 2) {
    if (p->sigticks != 0 || p->sighandler != 0) {
      if ((++p->ticks_sincelastcall) == p->sigticks) {
        p->ticks_sincelastcall = 0;
        memmove(p->trapframe4sig, p->trapframe, sizeof(struct trapframe));
        p->trapframe->epc = p->sighandler;
      }
    }
```

```c
// kernel/sysproc.c
uint64
sys_sigreturn(void) {
  struct proc *p = myproc();
  memmove(p->trapframe, p->trapframe4sig, sizeof(struct trapframe));
  return 0;
}
```

运行，在test2失败：
```
test2 failed: alarm handler called more than once
```

> Hint: Prevent re-entrant calls to the handler----if a handler hasn't returned yet, the kernel shouldn't call it again. test2 tests this.

这个可太好改了，咋改我就不写了，最后：

```sh
$ alarmtest
test0 start
...........alarm!
test0 passed
test1 start
...alarm!
..alarm!
...alarm!
....alarm!
...alarm!
..alarm!
....alarm!
..alarm!
...alarm!
....alarm!
test1 passed
test2 start
..............alarm!
test2 passed
```

usertests通过了，不贴了，内容太多，我们的代码也不可能破坏内核的其它部分，我们写的很小心。


