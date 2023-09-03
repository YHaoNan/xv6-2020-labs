# README
MIT6.S081/Fall 2020操作系统实验代码。尽量一天做一个，但是人是扛不住懒的。

每个分支是一个实验：
> 注：从Lock实验开始迁移到Fall 2021的实验题目和代码，因为2020的代码需要的tool chain比较老，我实在不想在新电脑上装那些老tool chain。

Lab|直链|概述
:-|:-|:-
#0 Utilities|[utils](https://github.com/YHaoNan/xv6-2020-labs/tree/util)|熟悉xv6及其提供的部分系统调用，你会利用xv6提供的系统调用开发三个程序
#1 System Calls|[syscall](https://github.com/YHaoNan/xv6-2020-labs/tree/syscall)|熟悉如何提供和编写系统调用，你需要为xv6添加两个系统调用，你会了解操作系统如何处理系统调用，如何在用户态和内核态之间拷贝数据等
#2 Page Tables|[pgtbl](https://github.com/YHaoNan/xv6-2020-labs/tree/pgtbl)|深入了解页表，开发页表相关的debug程序，修改xv6的页表逻辑以让xv6变得更好
#3 Traps|[traps](https://github.com/YHaoNan/xv6-2020-labs/tree/traps)|深入了解陷阱，你会利用时钟中断开发一个用于计算密集型任务的alarm系统调用
#5 Lazy Allocation|[lazy](https://github.com/YHaoNan/xv6-2020-labs/tree/lazy)|使用虚拟内存以及陷阱技术实现内存的懒分配，以应对用户进程预先申请内存的情况
#6 Copy On Write|[cow](https://github.com/YHaoNan/xv6-2020-labs/tree/cow)|使用虚拟内存以及陷阱技术实现copy on write，让fork系统调用拥有更好的性能，更加节省内存
#7 Multithreading|[thread](https://github.com/YHaoNan/xv6-2020-labs/tree/thread)|实现用户级线程（协程），使用pthread库
#8 Lock|[lock](https://github.com/YHaoNan/xv6-2020-labs/tree/lock)|xv6为了简单，很多地方使用了大内核锁设计，在极端情况下锁竞争会很激烈，这个Lab要修改内存分配器和Buffer Cache中的锁设计，降低锁竞争，提高并发度
#9 File System|[fs](https://github.com/YHaoNan/xv6-2020-labs/tree/fs)|给xv6 fs的inode添加二级间接块，支持大文件，让xv6支持符号链接功能，添加一个系统调用创建符号连接
#10 mmap|mmap|
#11 Network Driver|net|
