# Lab1: Xv6 and Unix utilities

## sleep (easy) 

> Implement the UNIX program sleep for xv6; your sleep should pause for a user-specified number of ticks. A tick is a notion of time defined by the xv6 kernel, namely the time between two interrupts from the timer chip. Your solution should be in the file user/sleep.c.

[user/sleep.c](./user/sleep.c)

## pingpong (easy)

> Write a program that uses UNIX system calls to ''ping-pong'' a byte between two processes over a pair of pipes, one for each direction. The parent should send a byte to the child; the child should print "<pid>: received ping", where <pid> is its process ID, write the byte on the pipe to the parent, and exit; the parent should read the byte from the child, print "<pid>: received pong", and exit. Your solution should be in the file user/pingpong.c.

Except output：
```sh
$ make qemu
...
init: starting sh
$ pingpong
4: received ping
3: received pong
$
```


阅读文档时发现的一些点：
- **man fork**：The  child  inherits  copies of the parent's set of open file descriptors.  Each file descriptor in the child refers to  the  same  open  file  description  (see open(2))  as  the  corresponding file descriptor in the parent.  This means that the two file descriptors share open file status flags, file offset, and  signal-driven  I/O  attributes  (see  the  description  of F_SETOWN and F_SETSIG in fc‐ntl(2)).

    > 所以当你fork时，关于一个pipe的任何打开的文件描述符都会被复制多个。如果你在pipe()后立即fork()，从操作系统的角度来看，管道就有了两个读端fd（父进程一个子进程一个），两个写端fd。

阅读lab hint时发现的一些点：
- User programs on xv6 have a limited set of library functions available to them. You can see the list in user/user.h; the source (other than for system calls) is in user/ulib.c, user/printf.c, and user/umalloc.c.

    > xv6中的用户程序可以利用系统调用和系统提供的普通库函数来编写代码。`user/umalloc.c`定义了`free`和`malloc`函数，供用户申请和释放内存，其参考了《The C Programming Language》中的8.7节，稍后会看看，毕竟NJU的ppm lab就让我们实现内存分配器。`user/printf.c`定义了一个简单的`printf`函数，没有提供标准`printf`的所有功能，但足够简单易用。`user/ulib.c`提供了一些常见的C标准库中的函数，在NJU的Abstract Machine里几乎都有，早知道当时来这里Copy了（手动狗头）。

[user/pingpong.c](./user/pingpong.c)


## primes (moderate)/(hard)

> Write a concurrent version of prime sieve using pipes. This idea is due to Doug McIlroy, inventor of Unix pipes. The picture halfway down [this page](http://swtch.com/~rsc/thread/) and the surrounding text explain how to do it. Your solution should be in the file user/primes.c.

并发素数筛算法伪代码：
```
p = get a number from left neighbor
print p
loop:
    n = get a number from left neighbor
    if (p does not divide n)
        send n to right neighbor
```

这个提示很重要，因为没仔细阅读这个提示，我的程序发生了死锁：
- `read` returns zero when the write-side of a pipe is closed.

> 我把右侧进程管道的写端的`close`放到`wait`这个进程结束之后了，导致`read`等待写端被关闭，而持有写端的进程等待该进程结束。


[user/primes.c](./user/primes.c)

## find (moderate)

> Write a simple version of the UNIX find program: find all the files in a directory tree with a specific name. Your solution should be in the file user/find.c.

需要注意的点：
- xv6作为一个为教育目的开发的小型系统，它给一个进程可以打开的文件描述符十分有限。我在debug代码时发现`open`调用总是会返回负数的文件描述符，我不明白为啥，明明文件存在，也可以正常读取。后来发现是我的代码里没有关闭文件描述符，导致我的进程不能打开更多的文件。xv6给一个进程的最大文件描述符个数只有16个，在`kernel/param.h`中定义。虽然常见的商用系统不可能如此极端，但是如果不及时close，我们会占用很多个文件描述符。对于内存也一样。


[user/find.c](./user/find.c)

## xargs (moderate)

> Write a simple version of the UNIX xargs program: read lines from the standard input and run a command for each line, supplying the line as arguments to the command. Your solution should be in the file user/xargs.c.

示例：
```sh
$ echo hello too | xargs echo bye
bye hello too
```

To test your solution for xargs, run the shell script xargstest.sh. Your solution is correct if it produces the following output:
```sh
$ make qemu
...
init: starting sh
$ sh < xargstest.sh
$ $ $ $ $ $ hello
hello
hello
$ $   
```


[user/xargs.c](./user/xargs.c)

