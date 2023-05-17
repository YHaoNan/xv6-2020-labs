# Lab: page tables

## Speed up system calls (easy)

> 注意，这个题目是我他喵的看错了，进到了2022年的实验里，2020年没有这个实验。不过挺有意思的就做了。

Some operating systems (e.g., Linux) speed up certain system calls by sharing data in a read-only region between userspace and the kernel. This eliminates the need for kernel crossings when performing these system calls. To help you learn how to insert mappings into a page table, your first task is to implement this optimization for the getpid() system call in xv6.

> When each process is created, map one read-only page at USYSCALL (a virtual address defined in memlayout.h). At the start of this page, store a struct usyscall (also defined in memlayout.h), and initialize it to store the PID of the current process. For this lab, ugetpid() has been provided on the userspace side and will automatically use the USYSCALL mapping. You will receive full credit for this part of the lab if the ugetpid test case passes when running pgtbltest.


最开始考虑了半天在用户地址空间的哪里放这个USYSCALL，用户地址空间是这样的：

![xv6用户进程地址空间](./scshot01.png)

最后决定将它放在TRAPFRAME下面，给一个物理页大小：

```diff
// kernel/memlayout.h
#define TRAPFRAME (TRAMPOLINE - PGSIZE)
+#define USYSCALL  (TRAPFRAME  - PGSIZE)
```

在proc.c的`proc_pagetable`函数中，分配一个物理页并创建这一映射：

```diff
// kernel/proc.c
pagetable_t
proc_pagetable(struct proc *p)
{
  pagetable_t pagetable;

  // An empty page table.
  pagetable = uvmcreate();
  if(pagetable == 0)
    return 0;

  // ...

+  struct usyscall *usyscall_ptr = kalloc();
+  usyscall_ptr->pid = p->pid;
+
+  // map the USYSCALL just below TRAPFRAME
+  if (mappages(pagetable, USYSCALL, PGSIZE,
+              (uint64) usyscall_ptr, PTE_R | PTE_U) < 0) {
+    uvmunmap(pagetable, USYSCALL, 1, 0);
+    uvmfree(pagetable, 0);
+    return 0;
+  }

  return pagetable;
}
```

在proc.c的`free_procpagetable`中unmap并释放物理页：
```diff
// Free a process's page table, and free the
// physical memory it refers to.
void
proc_freepagetable(pagetable_t pagetable, uint64 sz)
{
  uvmunmap(pagetable, TRAMPOLINE, 1, 0);
  uvmunmap(pagetable, TRAPFRAME, 1, 0);
+  uvmunmap(pagetable, USYSCALL, 1, 1);
  uvmfree(pagetable, sz);
}
```

我们自己提供一个`ugetpid`看看能不能用：
```c
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
```

运行结果：
```
$ ugetpid
before call ugetpid...
getpid: 3, ugetpid: 3
sub getpid: 4, ugetpid: 4
```

> 之前忘了给USYSCALL页面添加PTE_U这个flag，导致一直报load page fault，卡了我一个小时。因为我看TRAPFRAME和TRAMPOLINE都没做，我以为我也不用做，后来想想那两个是在supervisor权限下，页表还没切换到内核页表时被读取和运行的，所以不用PTE_U。

## Print a page table (easy)

To help you learn about RISC-V page tables, and perhaps to aid future debugging, your first task is to write a function that prints the contents of a page table.

> Define a function called vmprint(). It should take a pagetable_t argument, and print that pagetable in the format described below. Insert if(p->pid==1) vmprint(p->pagetable) in exec.c just before the return argc, to print the first process's page table. You receive full credit for this assignment if you pass the pte printout test of make grade.

Now when you start xv6 it should print output like this, describing the page table of the first process at the point when it has just finished exec()ing init:

```text
page table 0x0000000087f6e000
..0: pte 0x0000000021fda801 pa 0x0000000087f6a000
.. ..0: pte 0x0000000021fda401 pa 0x0000000087f69000
.. .. ..0: pte 0x0000000021fdac1f pa 0x0000000087f6b000
.. .. ..1: pte 0x0000000021fda00f pa 0x0000000087f68000
.. .. ..2: pte 0x0000000021fd9c1f pa 0x0000000087f67000
..255: pte 0x0000000021fdb401 pa 0x0000000087f6d000
.. ..511: pte 0x0000000021fdb001 pa 0x0000000087f6c000
.. .. ..510: pte 0x0000000021fdd807 pa 0x0000000087f76000
.. .. ..511: pte 0x0000000020001c0b pa 0x0000000080007000
```

写起来挺简单的：

```c
// kernel/vm.c
void __print_level(int level) {
  for (int i=1;i<=level; i++) {
    printf("..");
    if (i!=level) {
      printf(" ");
    }
  }
}

// 给定一个页表的起始地址，遍历页表中的512个页表项，并对每一个子页表调用__vmprint
void 
__vmprint(pagetable_t pgtbl, int level) {
  for (int i=0; i<512; i++) {
    pte_t pte = pgtbl[i];
    
    if (pte & PTE_V) {
      uint64 pa = PTE2PA(pte);
      __print_level(level);
      printf("%d: pte %p pa %p\n", i, pte, pa);
      if (level < 3) __vmprint((uint64*)pa, level + 1);
    }
  }
}

void
vmprint(pagetable_t pgtbl) {
  printf("page table %p\n", pgtbl);
  __vmprint(pgtbl, 1);
}
```

> 别忘了在kernel/def.h中添加相应定义

> 教案上将每一个PTE画成了54位，也就是6个字节零6位，可计算机并不允许我们以位进行寻址。通过阅读`walk`的代码，发现了这种通过将`pagetable_t`作为一个uint64数组的方式来遍历每一个pte的方式，这也意味着每一个PTE实际上占用64位（8字节），并非教案上画的54。所以，pte中高十位是没用的。

问题：

Explain the output of vmprint in terms of Fig 3-4 from the text. What does page 0 contain? What is in page 2? When running in user mode, could the process read/write the memory mapped by page 1?

```text
# Fig 3-4
page table 0x0000000087f6d000
.. 0: pte 0x0000000021fda001 pa 0x0000000087f68000
.. .. 0: pte 0x0000000021fd9c01 pa 0x0000000087f67000
.. .. .. 0: pte 0x0000000021fda41f pa 0x0000000087f69000
.. .. .. 1: pte 0x0000000021fd980f pa 0x0000000087f66000
.. .. .. 2: pte 0x0000000021fd941f pa 0x0000000087f65000
.. 255: pte 0x0000000021fdb001 pa 0x0000000087f6c000
.. .. 511: pte 0x0000000021fdac01 pa 0x0000000087f6b000
.. .. .. 509: pte 0x0000000021fda813 pa 0x0000000087f6a000
.. .. .. 510: pte 0x0000000021fdd807 pa 0x0000000087f76000
.. .. .. 511: pte 0x0000000020001c0b pa 0x0000000080007000
```

页面0到页面2都在用户虚拟地址的低处，我们已知stack和guard page肯定是每一个占用一个虚拟页，所以页0应该是用于保存进程所执行程序的文本和数据，页1是guard page，页2是stack。

![xv6用户进程地址空间](./scshot01.png)

关于页1用户进程能不能读写，根据它的PTE内容的最后十位`0000001111`，可以看出它的`PTE_U`标记是没有打开的，用户进程不能访问。

在类unix系统中，进程只能是从一个进程fork出来的，也就是说，如果没有其它系统调用，所有的进程都在执行`/init`程序。`exec`系统调用给了进程放弃当前的所有，执行新程序的能力。在xv6中，exec做的事大概如下：
1. 为进程创建一个新的pagetable，但目前还不应用，依然使用原先的pagetable
2. 读取ELF中的Program Header，对类型为`ELF_PROG_LOAD`（1）的，加载其中定义的部分到新页表的指定地址上，xv6中的可执行程序只有一个要加载的，实际上这个东西会占用一个页的大小（我不确定对于大型程序会不会占用更多，或是xv6硬性要求不允许超过一个页，我只用gdb调试了一个小型程序）
3. 在新页表中刚刚加载的内容上面创建两个新页，一个用于stack，一个用于guardpage
4. 将argc和argv复制到刚刚创建的栈上，确保程序启动时能够正确的读到argc和argv
5. 切换进程结构中的页表、pc计数器、栈指针，释放老页表

所以，我们也可以通过分析exec的执行过程回答上面的问题。

嘶，既然xv6把程序数据和文本数据弄到一个页里了，那我们理论上是能修改进程中的代码的，因为这个页具有RWX标记！！

那么，`vmprint`打印出的最后三个页面是干啥的？最后的是trampoline，倒数第二个是trapframe，倒数第三个是我们映射的usyscall，如果你去看官方lab的打印结果，它们没有我们最后509那个页表项。

## A kernel page table per process (hard)

## Simplify copyin/copyinstr (hard)