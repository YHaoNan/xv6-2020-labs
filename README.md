# Large File
给xv6的inode添加二级间接块指针，支持大文件

- `bmap`函数接受一个`inode`和一个`bn`，意味着获取该`inode`的第bn个数据块（没有则创建）该函数默认包含从直接块指针和一级间接块指针获取的逻辑，现在需要添加二级间接指针的获取逻辑
- `itrunc`函数释放一个`inode`，需要释放所有数据块

# Symbolic Link
给xv6添加一个`symlink`系统调用以实现软连接

- 符号连接也应该被保存为一个inode，可以在它的数据块中写入它指向的全路径（`writei`和`readi`可以直接写inode的数据块）
- `sys_open`系统调用应该被修改，可以递归解析符号链接

