# Lab: xv6 lazy page allocation

One of the many neat tricks an O/S can play with page table hardware is lazy allocation of user-space heap memory. Xv6 applications ask the kernel for heap memory using the sbrk() system call. In the kernel we've given you, sbrk() allocates physical memory and maps it into the process's virtual address space. It can take a long time for a kernel to allocate and map memory for a large request. Consider, for example, that a gigabyte consists of 262,144 4096-byte pages; that's a huge number of allocations even if each is individually cheap. In addition, some programs allocate more memory than they actually use (e.g., to implement sparse arrays), or allocate memory well in advance of use. To allow sbrk() to complete more quickly in these cases, sophisticated kernels allocate user memory lazily. That is, sbrk() doesn't allocate physical memory, but just remembers which user addresses are allocated and marks those addresses as invalid in the user page table. When the process first tries to use any given page of lazily-allocated memory, the CPU generates a page fault, which the kernel handles by allocating physical memory, zeroing it, and mapping it. You'll add this lazy allocation feature to xv6 in this lab.

## Eliminate allocation from sbrk() (easy)

Your first task is to delete page allocation from the sbrk(n) system call implementation, which is the function sys_sbrk() in sysproc.c. The sbrk(n) system call grows the process's memory size by n bytes, and then returns the start of the newly allocated region (i.e., the old size). Your new sbrk(n) should just increment the process's size (myproc()->sz) by n and return the old size. It should not allocate memory -- so you should delete the call to growproc() (but you still need to increase the process's size!).

Try to guess what the result of this modification will be: what will break?

> 任何尝试调用sbrk分配内存，然后使用刚刚分配的虚拟地址的操作都会失败。因为页表中还没有这些虚拟地址的映射。

Make this modification, boot xv6, and type echo hi to the shell. You should see something like this:

```sh
init: starting sh
$ echo hi
usertrap(): unexpected scause 0x000000000000000f pid=3
            sepc=0x0000000000001258 stval=0x0000000000004008
va=0x0000000000004000 pte=0x0000000000000000
panic: uvmunmap: not mapped
```

The "usertrap(): ..." message is from the user trap handler in trap.c; it has caught an exception that it does not know how to handle. Make sure you understand why this page fault occurs. The "stval=0x0..04008" indicates that the virtual address that caused the page fault is 0x4008.

```c
// kernel/sysproc.c
uint64
sys_sbrk(void)
{
  int addr;
  int n;

  if(argint(0, &n) < 0)
    return -1;
  addr = myproc()->sz;
  myproc()->sz += n;
  // if(growproc(n) < 0)
  //   return -1;
  return addr;
}
```

## Lazy allocation (moderate)

> Modify the code in trap.c to respond to a page fault from user space by mapping a newly-allocated page of physical memory at the faulting address, and then returning back to user space to let the process continue executing. You should add your code just before the printf call that produced the "usertrap(): ..." message. Modify whatever other xv6 kernel code you need to in order to get echo hi to work.

查阅riscv文档，异常码13和15分别是load page fault和store page fault，sbrk后的地址是用户用于读写数据的，正对应这两个异常。

在`trap.c`的用户陷阱处理程序`usertrap`中，添加处理这两个page fault的逻辑：
```c
// kernel/trap.c   usertrap()
  } else if(r_scause() == 13 || r_scause() == 15) {
    uint64 page_va = PGROUNDDOWN(r_stval());
    if (page_va >= TRAPFRAME) {
      printf("usertrap(): can not handle load/store page fault above TRAPFRAME.");
      p->killed = 1;
    } else {
      void *page_pa;
      if (!((page_pa = kalloc()) != 0 && 
        mappages(p->pagetable, page_va, PGSIZE, (uint64)page_pa, PTE_U | PTE_W | PTE_R)==0)) {
          printf("usertrap(): lazy allocation faild. maybe no more physical memory.");
          p->killed = 1;
      }
    }
  }
```

实验介绍里已经说了，程序通常希望预先分配更多的内存，即使它们不会使用。但在进程结束时，所有它们分配的内存都会被unmap，在原始的xv6中，是不允许没有被实际分配的虚拟内存unmap的。现在，我们的系统有了lazy allocation，这种保护就不成立了：

```c
void
uvmunmap(pagetable_t pagetable, uint64 va, uint64 npages, int do_free)
{
  uint64 a;
  pte_t *pte;

  if((va % PGSIZE) != 0)
    panic("uvmunmap: not aligned");

  for(a = va; a < va + npages*PGSIZE; a += PGSIZE){
    // 对于未分配的页，去除之前的panic，直接跳过
    if((pte = walk(pagetable, a, 0)) == 0) continue;
    if((*pte & PTE_V) == 0) continue;
      // panic("uvmunmap: not mapped");
    if(PTE_FLAGS(*pte) == PTE_V)
      panic("uvmunmap: not a leaf");
    if(do_free){
      uint64 pa = PTE2PA(*pte);
      kfree((void*)pa);
    }
    *pte = 0;
  }
}
```

至此，`echo hi`可以正常工作了：
```sh
$ echo hi
hi
```

## Lazytests and Usertests (moderate)

We've supplied you with lazytests, an xv6 user program that tests some specific situations that may stress your lazy memory allocator. Modify your kernel code so that all of both lazytests and usertests pass.

- Handle negative sbrk() arguments.
- Kill a process if it page-faults on a virtual memory address higher than any allocated with sbrk().
- Handle the parent-to-child memory copy in fork() correctly.
- Handle the case in which a process passes a valid address from sbrk() to a system call such as read or write, but the memory for that address has not yet been allocated.
- Handle out-of-memory correctly: if kalloc() fails in the page fault handler, kill the current process.
- Handle faults on the invalid page below the user stack.

要求中的第二条，第五条已经满足，接下来我们处理其它问题。

关于负的sbrk参数，这是进程想要缩减内存时做的，原来的`growproc`调用了`uvmdealloc`，最终间接调用了`uvmunmap`，现在的`uvmunmap`能处理懒分配的情况了，所以在负数参数时我们直接调用`growproc`，还有，我们也要在`sys_sbrk`中判断增长后的内存是否超出了限制，如果是，不给分配：

```c
uint64
sys_sbrk(void)
{
  int addr;
  int n;

  if(argint(0, &n) < 0)
    return -1;
  struct proc *p = myproc();
  addr = p->sz;
  uint64 newsize = addr + n;
  if (newsize >= TRAPFRAME || newsize < PGROUNDUP(myproc()->trapframe->sp)) {
    return -1;
  }
  if (n < 0 && growproc(n) < 0) {
    return -1;
  } else if (n > 0){
    myproc()->sz += n;
  }
  return addr;
}
```

关于父调用fork时拷贝自己所有的内存到子进程，fork会调用`uvmcopy`，它原来也会校验，若它发现没有被正确映射的内存，它会panic。现在，这个panic也不需要了：

```c
int
uvmcopy(pagetable_t old, pagetable_t new, uint64 sz)
{
  pte_t *pte;
  uint64 pa, i;
  uint flags;
  char *mem;

  for(i = 0; i < sz; i += PGSIZE){
    // 跳过未正确映射的页
    if((pte = walk(old, i, 0)) == 0) continue;
    if((*pte & PTE_V) == 0) continue;
      // panic("uvmcopy: page not present");
    pa = PTE2PA(*pte);
    flags = PTE_FLAGS(*pte);
    if((mem = kalloc()) == 0)
      goto err;
    memmove(mem, (char*)pa, PGSIZE);
    if(mappages(new, i, PGSIZE, (uint64)mem, flags) != 0){
      kfree(mem);
      goto err;
    }
  }
  return 0;

 err:
  uvmunmap(new, 0, i / PGSIZE, 1);
  return -1;
}
```

关于用户将未正确分配但已经被`sbrk`返回的虚拟地址传入到系统调用的情况，我们需要处理`copyin`和`copyout`，在虚拟地址没有实际映射时分配物理页并映射。

先提供一个`uvmallocmap`函数，它用于给虚拟地址分配一个物理页并映射，如果过程失败，返回0，否则返回物理页的物理地址：
```c
uint64
uvmallocmap(pagetable_t pgtbl, uint64 va) {
  void *pa;
  if ((pa=kalloc()) != 0) {
    if(mappages(pgtbl, va, PGSIZE, (uint64)pa, PTE_U | PTE_R | PTE_W) < 0) {
      kfree(pa);
      return 0;
    }
  }
  return (uint64)pa;
}
```

现在可以让`copyin`和`copyout`调用它，还可以让之前的`usertrap`代码调用它：
```c
int
copyout(pagetable_t pagetable, uint64 dstva, char *src, uint64 len)
{
  uint64 n, va0, pa0;

  while(len > 0){
    va0 = PGROUNDDOWN(dstva);
    pa0 = walkaddr(pagetable, va0);
    if(pa0 == 0 && (pa0 = uvmallocmap(pagetable, va0)) == 0)
      return -1;
    // ...
  }
  return 0;
}

int
copyin(pagetable_t pagetable, char *dst, uint64 srcva, uint64 len)
{
  uint64 n, va0, pa0;

  while(len > 0){
    va0 = PGROUNDDOWN(srcva);
    pa0 = walkaddr(pagetable, va0);
    if(pa0 == 0 && (pa0 = uvmallocmap(pagetable, va0)) == 0)
      return -1;
    // ...
  }
  return 0;
}
```

```c
// kernel/trap.c   usertrap()
  } else if(r_scause() == 13 || r_scause() == 15) {
    uint64 page_va = PGROUNDDOWN(r_stval());
    if (page_va >= TRAPFRAME) {
      printf("usertrap(): can not handle load/store page fault above TRAPFRAME.");
      p->killed = 1;
    } else {
      if (uvmallocmap(p->pagetable, page_va) == 0) {
          printf("usertrap(): lazy allocation faild. maybe no more physical memory.");
          p->killed = 1;
      }
    }
  }
```

下一个问题就是处理在用户栈之下的pagefault，用户栈在哪呢？？虽然通常栈都是第三个页面，但是万一程序的静态数据很大，一个页面装不下，这时用户栈应该不一定在第三个页面。我目前的想法是可以通过`p->trapfram->sp`获取：
```c
  } else if(r_scause() == 13 || r_scause() == 15) {
    uint64 page_va = PGROUNDDOWN(r_stval());
    if (page_va >= TRAPFRAME) {
      printf("usertrap(): can not handle load/store page fault above TRAPFRAME.");
      p->killed = 1;
    // 判断是否在栈顶以下
    } else if(page_va < PGROUNDUP(p->trapframe->sp)) {
      printf("usertrap(): can not handle load/store page fault below user stack top.");
      p->killed = 1;
    } else {
      if (uvmallocmap(p->pagetable, page_va) == 0) {
          printf("usertrap(): lazy allocation faild. maybe no more physical memory.");
          p->killed = 1;
      }
    }
  }
```


运行lazytests：
```sh
$ lazytests
lazytests starting
running test lazy alloc
test lazy alloc: OK
running test lazy unmap
panic: freewalk: leaf
```

出现`freewalk: leaf`的原因是结束用户进程时`uvmunmap`会把进程sz范围内的所有页unmap，然后调用`freewalk`来释放页表，`freewalk`期望经过`uvmunmap`的处理，此时没有一个有效的页面了，但它发现了有效的页面。言外之意，我们的程序具有到`proc->sz`范围之外的映射。

经过排查发现`usertrap`中的代码写错了：

```c
  } else if(r_scause() == 13 || r_scause() == 15) {
    uint64 page_va = PGROUNDDOWN(r_stval());
    // 原来，我们判断只要错误页在TRAPFRAME之内，我们就为其分配，但实际上它若超出了进程大小我们就不应该管这件事。
    if (page_va >= p->sz) {
    // if (page_va >= TRAPFRAME) {
      // printf("usertrap(): can not handle load/store page fault above TRAPFRAME.");
      p->killed = 1;
    } else if(page_va < PGROUNDUP(p->trapframe->sp)) {
      // printf("usertrap(): can not handle load/store page fault below user stack top.");
      p->killed = 1;
    } else {
      if (uvmallocmap(p->pagetable, page_va) == 0) {
          printf("usertrap(): lazy allocation faild. maybe no more physical memory.");
          p->killed = 1;
      }
    }
```

重新运行lazytests
```sh
$ lazytests
lazytests starting
running test lazy alloc
test lazy alloc: OK
running test lazy unmap
test lazy unmap: OK
running test out of memory
test out of memory: OK
ALL TESTS PASSED
```

usertests感觉应该过不了，因为我已经发现问题了。`uvmallocmap`函数几乎没有任何保护，负责系统调用中从用户页表拷入拷出数据的`copyin`和`copyout`也依赖于它，如果我们往系统调用中传递一个随意的虚拟地址，它就会帮我们映射那个页，果然在测试copyin的时候就废了：

```sh
test copyin: write(fd, 0x0000000080000000, 8192) returned 8192, not -1
panic: freewalk: leaf
```

删除之前的`uvmallocmap`，改一个`procallocmap`，它把检测的事都做好了：
```c
uint64
procallocmap(pagetable_t pgtbl, uint64 va) {
  void *pa;
  if (va >= myproc()->sz || va < PGROUNDUP(myproc()->trapframe->sp)) 
    return 0;
  if ((pa=kalloc()) != 0) {
    if(mappages(pgtbl, va, PGSIZE, (uint64)pa, PTE_U | PTE_R | PTE_W) < 0) {
      kfree(pa);
      return 0;
    }
  }
  return (uint64)pa;
}
```

然后在`copyin`、`copyout`和`usertrap`里调用：
```c
} else if(r_scause() == 13 || r_scause() == 15) {
  if (procallocmap(p->pagetable, PGROUNDDOWN(r_stval())) == 0) {
    p->killed = 1;
  }  
}
```

```c
while(len > 0){
  va0 = PGROUNDDOWN(srcva);
  pa0 = walkaddr(pagetable, va0);
  if(pa0 == 0 && (pa0 = procallocmap(pagetable, va0)) == 0)
    return -1;
```

全部通过