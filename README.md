# README
MIT6.S081/Fall 2020操作系统实验代码。尽量一天做一个，但是人是扛不住懒的。

每个分支是一个实验：

Lab|直链|概述
:-|:-|:-
#0 Utilities|[utils](https://github.com/YHaoNan/xv6-2020-labs/tree/util)|熟悉xv6及其提供的部分系统调用，你会利用xv6提供的系统调用开发三个程序
#1 System Calls|[syscall](https://github.com/YHaoNan/xv6-2020-labs/tree/syscall)|熟悉如何提供和编写系统调用，你需要为xv6添加两个系统调用，你会了解操作系统如何处理系统调用，如何在用户态和内核态之间拷贝数据等
#2 Page Tables|[pgtbl](https://github.com/YHaoNan/xv6-2020-labs/tree/pgtbl)|深入了解页表，开发页表相关的debug程序，修改xv6的页表逻辑以让xv6变得更好
#3 Traps|[traps](https://github.com/YHaoNan/xv6-2020-labs/tree/traps)|深入了解陷阱，你会利用时钟中断开发一个用于计算密集型任务的alarm系统调用
#5 Lazy Allocation|[lazy](https://github.com/YHaoNan/xv6-2020-labs/tree/lazy)|使用虚拟内存以及陷阱技术实现内存的懒分配，以应对用户进程预先申请内存的情况
#6 Copy On Write|[cow](https://github.com/YHaoNan/xv6-2020-labs/tree/cow)|使用虚拟内存以及陷阱技术实现copy on write，让fork系统调用拥有更好的性能，更加节省内存
#7 Multithreading|[thread](https://github.com/YHaoNan/xv6-2020-labs/tree/syscall)|实现用户级线程（协程），使用pthread库
#8 Lock|lock|
#9 File System|fs|
#10 mmap|mmap|
#11 Network Driver|net|