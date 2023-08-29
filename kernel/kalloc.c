// Physical memory allocator, for user processes,
// kernel stacks, page-table pages,
// and pipe buffers. Allocates whole 4096-byte pages.

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"

void freerange(void *pa_start, void *pa_end);
void kfree_hart(void *pa, int hartid, int lock);
void* kalloc_hart(int hartid, int lock);

extern char end[]; // first address after kernel.
                   // defined by kernel.ld.

struct run {
  struct run *next;
};

struct kmem {
  struct spinlock lock;
  struct run *freelist;
  char lockname[8];
} kmems[NCPU];

void
kinit()
{
  for (int i=0; i<NCPU; i++) {
    snprintf(kmems[i].lockname, 8, "kmem-%d", i);
    initlock(&kmems[i].lock, kmems[i].lockname);
  }
  freerange(end, (void*)PHYSTOP);
}

// only call once on startup
void
freerange(void *pa_start, void *pa_end)
{
  push_off();
  int hartid = cpuid();
  pop_off();
  char *p;
  p = (char*)PGROUNDUP((uint64)pa_start);
  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE) {
    kfree_hart(p, hartid, 1);
  }
}

void kfree_hart(void *pa, int hartid, int lock) {
  struct run *r;

  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP) {
    panic("kfree");
  }

  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);

  r = (struct run*)pa;

  struct kmem *k = &kmems[hartid];

  if (lock) acquire(&k->lock);
  r->next = k->freelist;
  k->freelist = r;
  if (lock) release(&k->lock);
}
// Free the page of physical memory pointed at by v,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void
kfree(void *pa)
{
  push_off();
  int hartid = cpuid();
  pop_off();
  kfree_hart(pa, hartid, 1);
}

// 在hartid下分配失败，去别的cpu偷袭
void* kalloc_hart_steal(int hartid) {
  struct run *r;
  for (int i=0; i<NCPU; i++) {
    // 尽量可着前面的偷
    hartid = (hartid - 1 + NCPU) % NCPU;
    r = kalloc_hart(hartid, 1);
    if (r) break;
  }
  return (void*)r;
}

void* kalloc_hart(int hartid, int lock) {
  struct run *r;

  struct kmem *k = &kmems[hartid];
  if (lock) acquire(&k->lock);
  r = k->freelist;
  if(r)
    k->freelist = r->next;
  if (lock) release(&k->lock);

  if(r)
    memset((char*)r, 5, PGSIZE); // fill with junk
  return (void*)r;
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
  push_off();
  int hartid = cpuid();
  pop_off();
  void *p = kalloc_hart(hartid, 1);
  if (!p) {
    p = kalloc_hart_steal(hartid);
  }
  return p;
}
