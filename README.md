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

> 这实验做起来不难，花不了多久，但是总是有很多可以追问的问题，它们才是最消磨时间的...

## A kernel page table per process (hard)

Xv6 has a single kernel page table that's used whenever it executes in the kernel. The kernel page table is a direct mapping to physical addresses, so that kernel virtual address x maps to physical address x. Xv6 also has a separate page table for each process's user address space, containing only mappings for that process's user memory, starting at virtual address zero. Because the kernel page table doesn't contain these mappings, user addresses are not valid in the kernel. Thus, when the kernel needs to use a user pointer passed in a system call (e.g., the buffer pointer passed to write()), the kernel must first translate the pointer to a physical address. The goal of this section and the next is to allow the kernel to directly dereference user pointers.

> Your first job is to modify the kernel so that every process uses its own copy of the kernel page table when executing in the kernel. Modify struct proc to maintain a kernel page table for each process, and modify the scheduler to switch kernel page tables when switching processes. For this step, each per-process kernel page table should be identical to the existing global kernel page table. You pass this part of the lab if usertests runs correctly.

### 分析

比较容易想到的解法是完整的复制所有三级内核页表。公开课老师给出的做法是只为每个进程创建一个根页表，然后根页表的第0个页表项每个进程独自使用，1-511则是通用，所以只需要简单的将1-511的PTE复制过去即可。

老师这样做的原因貌似是因为下面的实验限制了不让用户空间的虚拟地址增长到PLIC（0x0C000000）以上，所以在那之上的内存都不会因为进程不同而改变，换句话说就是无论在哪个进程的内核页表中它们都是通用的，所以直接共享PTE即可。PLIC被映射到根页表的0号PTE覆盖的内存中，意味着0号PTE覆盖的所有内存里有一部分不能共享，所以我们没有复制它。

xv6原来的内核页表中的一些设备映射到了0号PTE覆盖的虚拟地址空间中，所以如果采用老师的方法，要手动映射那些设备。

还有就是每个内核有独立的内核页表了，内核栈也可以独立起来了，而且如果采用老师的办法，内核栈必须在0号PTE覆盖的虚拟地址空间中，否则会混乱。

### 过程

**Step1. 在`kernel/proc.h`的proc结构体中添加`kpagetable`，指向它自己的内核页表**

**Step2. 创建`kvmpagetable_init()`，为进程创建自己的内核页表**

```c
// kernel/vm.c
void
kvmmap2(pagetable_t kpgtable, uint64 va, uint64 pa, uint64 sz, int perm)
{
  if(mappages(kpgtable, va, sz, pa, perm) != 0)
    panic("kvmmap2");
}

pagetable_t kvmpagetable_init() {
  // 分配根页表
  pagetable_t kpgtable = kalloc();
  memset(kpgtable, 0, PGSIZE);
  // 复制1~511的PTE
  for (int i=1; i<512; i++) {
    kpgtable[i] = kernel_pagetable[i];
  }

  // 映射在PTE0中的设备
  kvmmap2(kpgtable, UART0, UART0, PGSIZE, PTE_R | PTE_W);
  kvmmap2(kpgtable, VIRTIO0, VIRTIO0, PGSIZE, PTE_R | PTE_W);
  kvmmap2(kpgtable, CLINT, CLINT, 0x10000, PTE_R | PTE_W);
  kvmmap2(kpgtable, PLIC, PLIC, 0x400000, PTE_R | PTE_W);

  return kpgtable;
}
```

**Step3. 删除在`kernel/proc.c`中的`procinit()`函数里关于向全局页表项创建内核栈的代码**

```c
void
procinit(void)
{
  struct proc *p;
  
  initlock(&pid_lock, "nextpid");
  for(p = proc; p < &proc[NPROC]; p++) {
      initlock(&p->lock, "proc");

      // Allocate a page for the process's kernel stack.
      // Map it high in memory, followed by an invalid
      // guard page.
      // char *pa = kalloc();
      // if(pa == 0)
      //   panic("kalloc");
      // uint64 va = KSTACK((int) (p - proc));
      // kvmmap(va, (uint64)pa, PGSIZE, PTE_R | PTE_W);
      // p->kstack = va;
  }
  kvminithart();
}
```

**Step4. 修改`allocproc`，在分配进程时创建它自己的内核页表和内核栈**

```c
static struct proc*
allocproc(void) {
  // ...

  p->pagetable = proc_pagetable(p);
  
  if(p->pagetable == 0){
    freeproc(p);
    release(&p->lock);
    return 0;
  }

  // 照着上面的创建user pagetable，创建一个kernel pagetable
  p->kpagetable = kvmpagetable_init();
  if(p->kpagetable == 0){
    freeproc(p);
    release(&p->lock);
    return 0;
  }

  // 把内核栈映射到虚拟地址4096上，让0~4095做guard page，不知道会不会有问题...
  char *kstack = kalloc();
  if (kstack == 0) {
    panic("allocproc: create kstack faild.");
  }

  kvmmap2(p->kpagetable, 4096, kstack, PGSIZE, PTE_R | PTE_W);
  p->kstack = 4096;

  // ...
}
```

**Step5. 在调度器swtch前后切换内核页表**

```c
extern pagetable_t kernel_pagetable;
void
scheduler(void)
{
  // ...

    int found = 0;
    for(p = proc; p < &proc[NPROC]; p++) {
      acquire(&p->lock);
      if(p->state == RUNNABLE) {
        // Switch to chosen process.  It is the process's job
        // to release its lock and then reacquire it
        // before jumping back to us.
        p->state = RUNNING;
        c->proc = p;
        // 切换到进程p的内核页表，以便于我们能使用它的内核栈
        w_satp(MAKE_SATP(p->kpagetable)); sfence_vma();
        swtch(&c->context, &p->context);
        // 我们必须切换回kernel_pagetable，因为这个进程可能是由于已经结束了才返回的，它的页表可能已经被释放
        // 可是kernel_pagetable里面已经没有内核栈了啊呜呜呜喂！
        w_satp(MAKE_SATP(kernel_pagetable)); sfence_vma();

        // Process is done running for now.
        // It should have changed its p->state before coming back.
        c->proc = 0;

        found = 1;
        // ...

}
```

**Step6. 在进程被释放时释放内核页表**，此时需要释放的只有内核栈的物理页以及根页表的物理页、根页表第0个PTE覆盖的所有中间页表的物理页：

```c
static void
freeproc(struct proc *p)
{
  if(p->trapframe)
    kfree((void*)p->trapframe);
  p->trapframe = 0;
  if(p->pagetable)
    proc_freepagetable(p->pagetable, p->sz);
  if (p->kpagetable) 
    kvmfree_kpagetable(p->kpagetable);
  p->pagetable = 0;
  p->kpagetable = 0;
  p->kstack = 0;
  // ...
}

// 释放页表但不释放物理页
void __kvmfree_pagetable(pagetable_t pgtbl) {
  for (int i=0; i<512; i++) {
    pte_t pte = pgtbl[i];
    if (pte & PTE_V && (pte & (PTE_R|PTE_W|PTE_X)) == 0) {
      __kvmfree_pagetable((pagetable_t)PTE2PA(pte));
    }
    pgtbl[i] = 0;
  }
  kfree(pgtbl);
}

void 
kvmfree_kpagetable(pagetable_t kpagetable) {
  pte_t stack_pte = *walk(kpagetable, (uint64)4096, 0);
  char *stack_pa = (char*)PTE2PA(stack_pte);

  __kvmfree_pagetable((pagetable_t) PTE2PA(kpagetable[0]));

  kfree(stack_pa);
  kfree(kpagetable);
}
```

**Step7. 运行发现在kvmpa这个函数panic了，发现它需要walk内核栈里的PTE，由于xv6最开始并不是独立内核页表的，所以它walk的是全局内核页表**，改成通过参数传递内核页表的，并且在用到这个函数的地方传入对应进程的内核页表（实际上在这个函数里获取当前进程的内核页表也行）：

```c
uint64
kvmpa(pagetable_t kpagetable, uint64 va)
{
  uint64 off = va % PGSIZE;
  pte_t *pte;
  uint64 pa;

  pte = walk(kpagetable, va, 0);
  if(pte == 0)
    panic("kvmpa 1");
  if((*pte & PTE_V) == 0)
    panic("kvmpa 2");
  pa = PTE2PA(*pte);
  return pa+off;
}
```

> PS：在第一个实验（Speed up system call）中，向用户页表顶端添加了一个USYSCALL，那个会导致execout测试不通过，应该是因为它多占用了一个物理页，把相关逻辑注释就好了。这个卡了我好久...


## Simplify copyin/copyinstr (hard)

The kernel's copyin function reads memory pointed to by user pointers. It does this by translating them to physical addresses, which the kernel can directly dereference. It performs this translation by walking the process page-table in software. Your job in this part of the lab is to add user mappings to each process's kernel page table (created in the previous section) that allow copyin (and the related string function copyinstr) to directly dereference user pointers.

> Replace the body of copyin in kernel/vm.c with a call to copyin_new (defined in kernel/vmcopyin.c); do the same for copyinstr and copyinstr_new. Add mappings for user addresses to each process's kernel page table so that copyin_new and copyinstr_new work. You pass this assignment if usertests runs correctly and all the make grade tests pass.

This scheme relies on the user virtual address range not overlapping the range of virtual addresses that the kernel uses for its own instructions and data. Xv6 uses virtual addresses that start at zero for user address spaces, and luckily the kernel's memory starts at higher addresses. However, this scheme does limit the maximum size of a user process to be less than the kernel's lowest virtual address. After the kernel has booted, that address is 0xC000000 in xv6, the address of the PLIC registers; see kvminit() in kernel/vm.c, kernel/memlayout.h, and Figure 3-4 in the text. You'll need to modify xv6 to prevent user processes from growing larger than the PLIC address.

### 修复之前闯的祸
OK，看到这个题的第一反应就是上一个题目中我们自作聪明将内核栈映射到虚拟地址4096起始的一个页必须得改掉了......

由于这个题目想要让我们在进程内核页表建立来自用户虚拟地址的映射，而用户虚拟地址在`[0, PLIC)`，这恰好包含了我们在`[0, 4096)`的guard page和`[4096, 8192)`的内核栈。嘶，既然低地址不能弄，我们就把它映射到根页表最后一个PTE能覆盖的最高地址吧

```c
// 1GB - GUARD PAGE and KSTACK PAGE
#define NEWKSTACK ((512 * 512 * 4096) - (2 * PGSIZE))
```

> 我之前做实验的时候好像把guard page放到stack下面了！？

```c
// code in kernel/proc.c:allocproc

// Change 4096 to NEWSTACK
uint64 stack_va = NEWKSTACK
kvmmap2(p->kpagetable, stack_va, (uint64)kstack, PGSIZE, PTE_R | PTE_W);
p->kstack = stack_va;
```

然后释放页表的代码，也要改，我这里改了下名但是你改不改都无所谓：
```c
// 释放页表但不释放物理页
void __free_pagetable(pagetable_t pgtbl) {
  for (int i=0; i<512; i++) {
    pte_t pte = pgtbl[i];
    if (pte & PTE_V && (pte & (PTE_R|PTE_W|PTE_X)) == 0) {
      __free_pagetable((pagetable_t)PTE2PA(pte));
    }
    pgtbl[i] = 0;
  }
  kfree(pgtbl);
}

void kvmfree_kpagetable(pagetable_t kpagetable) {
  // Change 4096 to NEWSTACK
  pte_t stack_pte = *walk(kpagetable, NEWKSTACK, 0);
  char *stack_pa = (char*)PTE2PA(stack_pte);

  __free_pagetable((pagetable_t)PTE2PA(kpagetable[0]));
  kfree(stack_pa);
  kfree(kpagetable);
}
```

### 分析&干活

先说下copyin的使用场景，copyin复制处于用户页表中的数据到`dst`，该数据会通过一个`srcva`和一个`sz`传入，`srcva`是用户页表的虚拟地址，`sz`是要复制的大小。由于内核无法直接解引用用户指针，所以它必须`walkaddr`在用户页表中包含待复制数据的每一页，得到实际的内核可以直接解引用的物理地址，然后复制到`dst`。大概是这样：

```c
for 用户页表中包含待复制数据的每一个页的起始地址 va0:
  pa0 = walkaddr(userpagetable, va0)
  memmove(dst, pa0, PGSIZE)
```

当然，这个复制并不像上面伪代码那样简单，实际的复制过程对应`copyin`中的代码，也没复杂太多。

我们的实验目标是跳过这种繁杂的过程。如果内核可以直接解引用用户指针，那它便可以直接通过用户指针进行复制，也就是这样：

```c
memmove(dst, srcva, len)
```

这种新的复制过程对应`copyin_new`中的代码。

我们上一个实验已经帮我们做了一些事情，通过独有的PTE0，每个进程享有1GB的私有内核页表空间，我们可以将用户页表中的数据一一映射到内核页表的同个虚拟地址上，这样当使用内核页表时，就能直接解引用用户指针。当然，这1GB里有一些设备，我们不能覆盖这些设备的内存，所以我们只有PLIC以下的内存可以使用，题目中也限制了，我们必须修改xv6的代码，让进程的大小无法grow到PLIC。

> 需要注意的是PLIC下面还有一个CLINT设备，它启动后就没用了，在进程的内核页表中由于我们可能覆盖它，所以我们得先对它unmap，否则会报remap。

这意味着每一次用户页表发生改变，我们都必须将改变映射给内核页表。题目的hint中给了用户页表会发生改变的位置：`fork()`、`exec()`以及`sbrk()`，我们需要做的是在这里将改动反映到进程的内核页表上，和上一个题目一样，我们不用复制物理页，只建立映射就行。


**Step1. kvmcopy**

基本上是完全复制uvmcopy，只不过可以设置start（因为sbrk要求我们并不是从0开始复制），删除了实际物理页的创建，而是使用原来的物理页进行映射。flag中的`PTE_U`必须清除，因为在supervisor模式下不能读取具有PTE_U标记的PTE。

```c
int
kvmcopy(pagetable_t old, pagetable_t new, uint64 start, uint64 sz)
{
  pte_t *pte;
  uint64 pa, i;
  uint flags;

  for(i = PGROUNDUP(start); i < start + sz; i += PGSIZE){
    if((pte = walk(old, i, 0)) == 0)
      panic("kvmcopy: pte should exist");
    if((*pte & PTE_V) == 0)
      panic("kvmcopy: page not present");
    pa = PTE2PA(*pte);
    flags = PTE_FLAGS(*pte) & ~PTE_U;
    if(mappages(new, i, PGSIZE, (uint64)pa, flags) != 0){
      goto err;
    }
  }
  return 0;

 err:
  uvmunmap(new, 0, i / PGSIZE, 1);
  return -1;
}
```

**Step2. kvmdealloc**

在sbrk中，用户内存缩减时我们也需要反映到内核页表，不然下次再sbrk就remap了。

这个就完全复制`uvmdealloc`，只是在调用`uvmunmap`时将`do_free`设置成0，这样就不会清除实际的物理页了，因为物理页是由与之相对应的`uvmdealloc`清除的：

```c
uint64
kvmdealloc(pagetable_t pagetable, uint oldsz, uint64 newsz) {
  if(newsz >= oldsz)
    return oldsz;

  if(PGROUNDUP(newsz) < PGROUNDUP(oldsz)){
    int npages = (PGROUNDUP(oldsz) - PGROUNDUP(newsz)) / PGSIZE;
    uvmunmap(pagetable, PGROUNDUP(newsz), npages, 0);
  }

  return newsz;
}
```

**Step3. fork/exec/sbrk/userinit的修改**

```c
int
fork(void)
{
  int i, pid;
  struct proc *np;
  struct proc *p = myproc();

  // Allocate process.
  if((np = allocproc()) == 0){
    return -1;
  }

  // Copy user memory from parent to child.
  if(uvmcopy(p->pagetable, np->pagetable, p->sz) < 0){
    freeproc(np);
    release(&np->lock);
    return -1;
  }

  // 拷贝刚刚复制的用户页表给新进程的内核页表
  // 这里能不能用p->pagetable进行拷贝呢？
  // 答案是不能，我之前犯了这个错误，我认为p和np的pagetable都是一样的，但实际上它们拥有不同的物理页
  // 你不能把内核页表里的PTE映射到另一个进程的物理页上，这样就泰酷辣~
  if (kvmcopy(np->pagetable, np->kpagetable, 0, p->sz) < 0) {
    freeproc(np);
    release(&np->lock);
    return -1;
  }

  // ...
}
```

```c
// in exec.c -> exec()
// ...

  for(last=s=path; *s; s++)
    if(*s == '/')
      last = s+1;
  safestrcpy(p->name, last, sizeof(p->name));
    
  // 释放加载另一个程序之前，进程内核页表中的数据
  uvmunmap(p->kpagetable, 0, PGROUNDUP(oldsz) / PGSIZE, 0);
  // 将新的数据拷贝进去
  if (kvmcopy(pagetable, p->kpagetable, 0, sz) < 0) 
    goto bad;
  // Commit to the user image.
  oldpagetable = p->pagetable;
  p->pagetable = pagetable;

// ...
```

> exec里为程序建立了新的用户页表，我们能不能使用类似的策略建立新的内核页表，而不是像上面那样替换呢？
>
> 不行，进程在内核中运行依赖内核页表中的内核栈，如果你将原来的内核页表释放，内核栈也释放了，当完成释放的方法调用后进程将遇到一个内存访问错误，它没法读原来的内核栈。
> 
> 上面的方式没有问题，因为我们解映射的数据不可能超过PLIC，根本碰不到内核栈。

我们还需要在exec加载程序到内存的循环中判断是否已经越过了PLIC，如果越过了我们要终止程序：
```c
// kernel/exec.c
if((sz1 = uvmalloc(pagetable, sz, ph.vaddr + ph.memsz)) == 0)
  goto bad;
if (sz1 >= PLIC) 
  goto bad;
sz = sz1;
```

```c
int
growproc(int n)
{
  uint sz;
  struct proc *p = myproc();

  sz = p->sz;
  if (sz + n >= PLIC) return -1;
  if(n > 0){
    uint64 newsize;
    if((newsize = uvmalloc(p->pagetable, sz, sz + n)) == 0) {
      return -1;
    }
    if (kvmcopy(p->pagetable, p->kpagetable, sz, n) < 0) {
      // 如果copy失败，视为grow失败，要把用户页表中的内容也dealloc掉
      uvmdealloc(p->pagetable, newsize, sz);
      return -1;
    }
    sz = newsize;
  } else if(n < 0){
    uvmdealloc(p->pagetable, sz, sz + n);
    sz = kvmdealloc(p->kpagetable, sz, sz + n);
  }
  p->sz = sz;
  return 0;
}
```

除了`/init`进程，其它进程都是fork出来的，如果需要的话，通过exec加载其它程序。我们把fork和exec中的复制都搞定了，但`/init`进程还没搞，它在`userinit`中使用`uvminit`将`initcode`加载到内存中，`initcode`就是`exec("/init")`：

```c
void
userinit(void)
{
  struct proc *p;

  p = allocproc();
  initproc = p;
  
  // allocate one user page and copy init's instructions
  // and data into it.
  uvminit(p->pagetable, initcode, sizeof(initcode));
  // 复制initcode
  kvmcopy(p->pagetable, p->kpagetable, 0, PGSIZE);
  p->sz = PGSIZE;

  // ...
}
```

**Step3. 修改copyin和copyinstr**

```c
int
copyin(pagetable_t pagetable, char *dst, uint64 srcva, uint64 len)
{
  return copyin_new(pagetable, dst, srcva, len);
}

int
copyinstr(pagetable_t pagetable, char *dst, uint64 srcva, uint64 max)
{
  return copyinstr_new(pagetable, dst, srcva, max);
}
```

Linux uses a technique similar to what you have implemented. Until a few years ago many kernels used the same per-process page table in both user and kernel space, with mappings for both user and kernel addresses, to avoid having to switch page tables when switching between user and kernel space. However, that setup allowed side-channel attacks such as Meltdown and Spectre.

> Explain why the third test srcva + len < srcva is necessary in copyin_new(): give values for srcva and len for which the first two test fail (i.e., they will not cause to return -1) but for which the third one is true (resulting in returning -1).

