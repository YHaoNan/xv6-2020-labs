# Lab: Multithreading

## Uthread: switching between threads (moderate)

In this exercise you will design the context switch mechanism for a user-level threading system, and then implement it. To get you started, your xv6 has two files user/uthread.c and user/uthread_switch.S, and a rule in the Makefile to build a uthread program. uthread.c contains most of a user-level threading package, and code for three simple test threads. The threading package is missing some of the code to create a thread and to switch between threads.

> Your job is to come up with a plan to create threads and save/restore registers to switch between threads, and implement that plan. When you're done, make grade should say that your solution passes the uthread test.


和进程一样，线程是一个状态机，用户级线程大概是这样的：
1. 我们有多个状态机（线程），状态机每一步执行都从一个状态迁移到另一个状态
2. 我们有一个傻了吧唧的硬件，可以驱动状态机前进。它有一个记录状态机如何迁移状态的东西（pc寄存器指向了线程当前的代码），还记录了当前状态机的所有状态（其它寄存器）。但我们只有一份这样的硬件，所以只能运行一个状态机（current_thread）。
3. 我们可以选定要执行哪个状态机（thread_schedule），通过修改当前硬件的pc和其它寄存器，我们可以让硬件开始驱动另一个状态机
4. 我们可以放弃当前状态机的执行，切换到另一个状态机。这就需要将当前状态机的状态持久化，加载另一个状态机的状态到硬件上（uthread_switch.S）
5. 创建新的状态机，给它正确的初始的状态（thread_create）

搞清楚了这些就简单了。我们主要要填写三个地方的代码，第一个在`user/uthread.c`中的`thread_schedule`，该函数负责选择一个状态机，并将硬件切换到上面执行：

```c
// user/uthread.c
void 
thread_schedule(void)
{
  struct thread *t, *next_thread;

  /* Find another runnable thread. */
  next_thread = 0;
  t = current_thread + 1;
  for(int i = 0; i < MAX_THREAD; i++){
    if(t >= all_thread + MAX_THREAD)
      t = all_thread;
    if(t->state == RUNNABLE) {
      next_thread = t;
      break;
    }
    t = t + 1;
  }

  if (next_thread == 0) {
    printf("thread_schedule: no runnable threads\n");
    exit(-1);
  }

  if (current_thread != next_thread) {         /* switch threads?  */
    next_thread->state = RUNNING;
    t = current_thread;
    current_thread = next_thread;
    /* YOUR CODE HERE
     * Invoke thread_switch to switch from t to next_thread:
     * thread_switch(??, ??);
     */
  } else
    next_thread = 0;
}
```

第二个是创建线程时，要给线程的状态机一个初始状态？：
```c
void 
thread_create(void (*func)())
{
  struct thread *t;

  for (t = all_thread; t < all_thread + MAX_THREAD; t++) {
    if (t->state == FREE) break;
  }
  t->state = RUNNABLE;
  // YOUR CODE HERE
}
```

第三个就是切换线程时，要将当前状态机的状态持久化，将另一个线程已经持久化好的状态加载到硬件上：
```asm
	.text

	/*
         * save the old thread's registers,
         * restore the new thread's registers.
         */

	.globl thread_switch
thread_switch:
	/* YOUR CODE HERE */
	ret    /* return to ra */
```

### 先完成线程切换
在这个线程库中，通过`thread_switch`函数来完成线程切换，这是我们需要用汇编代码编写的，它的函数签名如下：
```c
extern void thread_switch(uint64, uint64);
```

若想通过`thread_switch`从当前线程切换到另一个线程，首先，C Calling Convention会将所有需要的Caller Saved寄存器保存在当前线程栈上，当函数调用返回，又会将它们从栈上恢复。所以，我们只需要保存Callee Saved寄存器。并且根据Calling Convention，函数调用的返回语句将会跳转回ra寄存器中的指针，所以我们也无需保存和操作pc寄存器，只需要加载另一个线程的ra即可在函数返回时跳转到另一个线程。

嘶，这代码xv6内核里有啊......直接复制proc.h里的context：

```c
// user/uthread.c
struct ctx {
  uint64 ra;
  uint64 sp;

  // callee-saved
  uint64 s0;
  uint64 s1;
  uint64 s2;
  uint64 s3;
  uint64 s4;
  uint64 s5;
  uint64 s6;
  uint64 s7;
  uint64 s8;
  uint64 s9;
  uint64 s10;
  uint64 s11;
};

struct thread {
  char       stack[STACK_SIZE]; /* the thread's stack */
  int        state;             /* FREE, RUNNING, RUNNABLE */
  struct context ctx;  // 给线程添加一个上下文属性
};
```

然后直接copy paste `kernel/swtch.S`到`user/uthread_switch.S`就行啊：
```asm
// user/uthread_switch.S
	.text

	/*
         * save the old thread's registers,
         * restore the new thread's registers.
         */

	.globl thread_switch
thread_switch:
		sd ra, 0(a0)
        sd sp, 8(a0)
        sd s0, 16(a0)
        sd s1, 24(a0)
        sd s2, 32(a0)
        sd s3, 40(a0)
        sd s4, 48(a0)
        sd s5, 56(a0)
        sd s6, 64(a0)
        sd s7, 72(a0)
        sd s8, 80(a0)
        sd s9, 88(a0)
        sd s10, 96(a0)
        sd s11, 104(a0)

        ld ra, 0(a1)
        ld sp, 8(a1)
        ld s0, 16(a1)
        ld s1, 24(a1)
        ld s2, 32(a1)
        ld s3, 40(a1)
        ld s4, 48(a1)
        ld s5, 56(a1)
        ld s6, 64(a1)
        ld s7, 72(a1)
        ld s8, 80(a1)
        ld s9, 88(a1)
        ld s10, 96(a1)
        ld s11, 104(a1)
	ret    /* return to ra */
```

现在，第一个参数就是老线程的上下文，第二个参数就是新线程的上下文。

### thread_schedule

有了`thread_switch`，直接调用就行了：
```c
/* YOUR CODE HERE
    * Invoke thread_switch to switch from t to next_thread:
    * thread_switch(??, ??);
    */
thread_switch((uint64)&t->ctx, (uint64)&current_thread->ctx);
```

### thread_create
在线程创建时，我们需要给它初始状态，其实也就是存一下ra和sp：

```c
void 
thread_create(void (*func)())
{
  struct thread *t;

  for (t = all_thread; t < all_thread + MAX_THREAD; t++) {
    if (t->state == FREE) break;
  }
  t->state = RUNNABLE;
  // YOUR CODE HERE
  memmove(&t->ctx, 0, sizeof(struct ctx));
  t->ctx.ra = (uint64) func;
  t->ctx.sp = (uint64) &t->stack + (STACK_SIZE - 1);
}
```
> 这里需要注意的是栈是向下增长的，所以栈底实际上是栈的最高地址。

```sh
$ uthread
thread_a started
thread_b started
thread_c started
thread_c 0
thread_a 0
thread_b 0
...
thread_c 99
thread_a 99
thread_b 99
thread_c: exit after 100
thread_a: exit after 100
thread_b: exit after 100
thread_schedule: no runnable threads
```

> NJU操作系统2022的libco实验和这个很像，也是实现一个用户级线程（协程），但那个比这个还要硬核很多。

## Using threads (moderate)
In this assignment you will explore parallel programming with threads and locks using a hash table. You should do this assignment on a real Linux or MacOS computer (not xv6, not qemu) that has multiple cores. Most recent laptops have multicore processors.

This assignment uses the UNIX pthread threading library. You can find information about it from the manual page, with man pthreads, and you can look on the web, for example [here](https://pubs.opengroup.org/onlinepubs/007908799/xsh/pthread_mutex_lock.html), [here](https://pubs.opengroup.org/onlinepubs/007908799/xsh/pthread_mutex_init.html), and [here](https://pubs.opengroup.org/onlinepubs/007908799/xsh/pthread_create.html).

The file notxv6/ph.c contains a simple hash table that is correct if used from a single thread, but incorrect when used from multiple threads. In your main xv6 directory (perhaps ~/xv6-labs-2020), type this:

```sh
$ make ph
$ ./ph 1
```

Note that to build ph the Makefile uses your OS's gcc, not the 6.S081 tools. The argument to ph specifies the number of threads that execute put and get operations on the the hash table. After running for a little while, ph 1 will produce output similar to this:

```sh
100000 puts, 3.991 seconds, 25056 puts/second
0: 0 keys missing
100000 gets, 3.981 seconds, 25118 gets/second
```

The numbers you see may differ from this sample output by a factor of two or more, depending on how fast your computer is, whether it has multiple cores, and whether it's busy doing other things.

ph runs two benchmarks. First it adds lots of keys to the hash table by calling put(), and prints the achieved rate in puts per second. The it fetches keys from the hash table with get(). It prints the number keys that should have been in the hash table as a result of the puts but are missing (zero in this case), and it prints the number of gets per second it achieved.

You can tell ph to use its hash table from multiple threads at the same time by giving it an argument greater than one. Try ph 2:

```sh
$ ./ph 2
100000 puts, 1.885 seconds, 53044 puts/second
1: 16579 keys missing
0: 16579 keys missing
200000 gets, 4.322 seconds, 46274 gets/second
```

The first line of this ph 2 output indicates that when two threads concurrently add entries to the hash table, they achieve a total rate of 53,044 inserts per second. That's about twice the rate of the single thread from running ph 1. That's an excellent "parallel speedup" of about 2x, as much as one could possibly hope for (i.e. twice as many cores yielding twice as much work per unit time).

However, the two lines saying 16579 keys missing indicate that a large number of keys that should have been in the hash table are not there. That is, the puts were supposed to add those keys to the hash table, but something went wrong. Have a look at notxv6/ph.c, particularly at put() and insert().

> Why are there missing keys with 2 threads, but not with 1 thread? Identify a sequence of events with 2 threads that can lead to a key being missing. Submit your sequence with a short explanation in answers-thread.txt
> 
> 因为多个线程同时操作共享的hash表，它们的操作又不是原子的，多个线程一同操作会产生混乱。举个例子，线程A想将entry插入到槽位i，它找到了当前槽位链表的最后一个节点n，线程B也想插入一个entry，它也找到了节点n，它们下一个操作都是将自己的entry作为n的下一个，但只有后面执行的那个线程的entry才会留下，另一个会被覆盖。

> To avoid this sequence of events, insert lock and unlock statements in put and get in notxv6/ph.c so that the number of keys missing is always 0 with two threads. The relevant pthread calls are:
> ```c
> pthread_mutex_t lock;            // declare a lock
> pthread_mutex_init(&lock, NULL); // initialize the lock
> pthread_mutex_lock(&lock);       // acquire lock
> pthread_mutex_unlock(&lock);     // release lock
> ```
> You're done when make grade says that your code passes the ph_safe test, which requires zero missing keys with two threads. It's OK at this point to fail the ph_fast test.

### 干！
实际上每一个hash表的槽位都是独立的内存区域，多个线程若访问不同的槽位是不会产生影响的，所以锁的力度可以以槽位为单位，而不是以整个哈希表为单位：
```c
pthread_mutex_t locks[NBUCKET];

static 
void put(int key, int value)
{
  int i = key % NBUCKET;

  // is the key already present?
  struct entry *e = 0;
  // 锁定槽
  pthread_mutex_lock(&locks[i]);
  for (e = table[i]; e != 0; e = e->next) {
    if (e->key == key)
      break;
  }
  if(e){
    // update the existing key.
    e->value = value;
  } else {
    // the new is new.
    insert(key, value, &table[i], table[i]);
  }
  // 解锁槽
  pthread_mutex_lock(&locks[i]);
  pthread_mutex_unlock(&locks[i]);
}

// 别忘了初始化锁
void init_lock() {
  for (int i=0; i<NBUCKET; i++) {
    pthread_mutex_init(&(locks[i]), NULL);
  }
}

int
main(int argc, char *argv[])
{
  init_lock();
  // ...
}
```

## Barrier(moderate)

In this assignment you'll implement a barrier: a point in an application at which all participating threads must wait until all other participating threads reach that point too. You'll use pthread condition variables, which are a sequence coordination technique similar to xv6's sleep and wakeup.

You should do this assignment on a real computer (not xv6, not qemu).

The file notxv6/barrier.c contains a broken barrier.

```sh
$ make barrier
$ ./barrier 2
barrier: notxv6/barrier.c:42: thread: Assertion `i == t' failed.
```

The 2 specifies the number of threads that synchronize on the barrier ( nthread in barrier.c). Each thread executes a loop. In each loop iteration a thread calls barrier() and then sleeps for a random number of microseconds. The assert triggers, because one thread leaves the barrier before the other thread has reached the barrier. The desired behavior is that each thread blocks in barrier() until all nthreads of them have called barrier().

> Your goal is to achieve the desired barrier behavior. In addition to the lock primitives that you have seen in the ph assignment, you will need the following new pthread primitives; look [here](https://pubs.opengroup.org/onlinepubs/007908799/xsh/pthread_cond_wait.html) and [here](https://pubs.opengroup.org/onlinepubs/007908799/xsh/pthread_cond_broadcast.html) for details.
> ```c
> pthread_cond_wait(&cond, &mutex);  // go to sleep on cond, releasing lock mutex, acquiring upon wake up
> pthread_cond_broadcast(&cond);     // wake up every thread sleeping on cond
> ```
> Make sure your solution passes make grade's barrier test.

```c
static void 
barrier()
{
  pthread_mutex_lock(&bstate.barrier_mutex);
  bstate.nthread++;
  if (bstate.nthread < nthread) {
    pthread_cond_wait(&bstate.barrier_cond, &bstate.barrier_mutex);
  } else {
    bstate.round++;
    bstate.nthread = 0;
    pthread_cond_broadcast(&bstate.barrier_cond);
  }
  pthread_mutex_unlock(&bstate.barrier_mutex);
}
```
