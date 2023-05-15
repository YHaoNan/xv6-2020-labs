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



