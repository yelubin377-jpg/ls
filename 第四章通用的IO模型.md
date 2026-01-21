这部分内容聚焦的是UNIX/Linux通用I/O模型（这是本章核心介绍的I/O模型），其核心是通过文件描述符和**4个基础系统调用（open、read、write、close）**实现对所有类型文件（普通文件、管道、终端、socket等）的统一I/O操作。以下是书中对该模型的详细内容覆盖：

一、通用I/O模型的核心基础：文件描述符

1. 定义：文件描述符是一个非负整数（通常是小整数），用于指代进程已打开的文件（包括普通文件、管道、FIFO、socket、终端设备等）。
2. 进程独立性：每个进程的文件描述符是独立的（自成一套）。
3. 标准文件描述符：进程启动时默认继承shell打开的3个标准描述符：
-  0 （STDIN_FILENO）：标准输入，对应 stdin ；
-  1 （STDOUT_FILENO）：标准输出，对应 stdout ；
-  2 （STDERR_FILENO）：标准错误，对应 stderr ；
（shell会根据重定向操作修改这些描述符的指向）

二、通用I/O模型的核心系统调用（构成模型的基础操作）

通用I/O模型通过4个核心系统调用实现对所有文件类型的I/O操作，以下是书中的详细说明：

1. 打开文件： open() 

- 功能：打开已存在的文件，或创建并打开新文件。
- 函数原型： int open(const char *pathname, int flags, ... /* mode_t mode */); 
- 返回值：成功返回文件描述符，失败返回 -1 并设置 errno 。
- 参数详解：
-  pathname ：文件路径（若为符号链接，会自动解引用）；
-  flags ：位掩码，包含访问模式和其他操作标志：
- 访问模式（三选一）： O_RDONLY （只读）、 O_WRONLY （只写）、 O_RDWR （读写）；
- 其他操作标志（可组合）：
-  O_CREAT ：文件不存在则创建；
-  O_TRUNC ：文件存在则截断为0长度；
-  O_APPEND ：写操作追加到文件末尾；
-  O_CLOEXEC ：进程执行 exec() 时自动关闭该描述符；
-  O_DIRECTORY ：若 pathname 不是目录则失败；
-  O_LARGEFILE ：支持大文件（32位系统中）；
-  O_NOATIME ：读文件时不更新 st_atime （需权限）；
-  O_NONBLOCK ：非阻塞模式打开文件；
-  O_DSYNC / O_SYNC ：写操作同步（保证数据/元数据落盘）；
-  mode ：仅当 O_CREAT 被指定时有效，用于设置新文件的权限（如 S_IRUSR | S_IWUSR 表示用户读写），最终权限会受进程 umask 影响。
- 早期替代调用： creat() （已被 open() 替代，等价于 open(pathname, O_WRONLY | O_CREAT | O_TRUNC, mode) ）。

2. 读取文件： read() 

- 功能：从文件描述符 fd 对应的文件中读取数据到缓冲区。
- 函数原型： ssize_t read(int fd, void *buffer, size_t count); 
- 返回值：成功返回实际读取的字节数，遇到EOF返回 0 ，失败返回 -1 。
- 参数详解：
-  fd ：目标文件描述符；
-  buffer ：预先分配的内存缓冲区（系统不负责分配，需调用者准备）；
-  count ：期望读取的最大字节数。
- 关键特性：
- 实际读取字节数可能小于 count ：
- 普通文件：读取位置接近文件尾部；
- 管道/FIFO/socket/终端：数据未准备充足（如终端读取到换行符时结束）；
- 不自动添加字符串终止符：若读取的是文本数据，需手动在缓冲区末尾加 \0 。

3. 写入文件： write() （书中后续内容，本章已铺垫）

- 功能：将缓冲区中的数据写入文件描述符 fd 对应的文件。
- 核心特性：实际写入字节数也可能小于 count （如磁盘空间不足、非阻塞模式下数据未写完）。

4. 关闭文件： close() 

- 功能：关闭文件描述符 fd ，释放对应的内核资源。
- 函数原型： int close(int fd); 
- 返回值：成功返回 0 ，失败返回 -1 。

三、通用I/O模型的“通用性”核心特点

UNIX/Linux通用I/O模型的核心优势是I/O操作的统一性：

- 同一套系统调用（ open/read/write/close ）可用于所有类型的文件（普通文件、管道、FIFO、socket、终端设备等）；
- 内核负责处理不同文件/设备的底层细节，应用程序无需区分文件类型；
- 若需访问设备专有特性，可通过 ioctl() 系统调用（本章后续会介绍）补充。

四、通用I/O模型的代码示例（书中的 copy 程序）

书中提供了一个模拟 cp 命令的程序，完整演示通用I/O模型的使用：

```c
c  
#include <sys/stat.h>
#include <fcntl.h>
#include "tlpi_hdr.h"

#ifndef BUF_SIZE
#define BUF_SIZE 1024
#endif

int main(int argc, char *argv[])
{
    int inputFd, outputFd, openFlags;
    mode_t filePerms;
    ssize_t numRead;
    char buf[BUF_SIZE];

    if (argc != 3 || strcmp(argv[1], "--help") == 0)
        usageErr("%s old-file new-file\n", argv[0]);
    
    /* 打开源文件（只读） */
    inputFd = open(argv[1], O_RDONLY);
    if (inputFd == -1)
        errExit("opening file %s", argv[1]);
    
    /* 打开/创建目标文件（只写、创建、截断，权限0666） */
    openFlags = O_CREAT | O_WRONLY | O_TRUNC;
    filePerms = S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH;
    outputFd = open(argv[2], openFlags, filePerms);
    if (outputFd == -1)
        errExit("opening file %s", argv[2]);
    
    /* 循环读写数据 */
    while ((numRead = read(inputFd, buf, BUF_SIZE)) > 0)
        if (write(outputFd, buf, numRead) != numRead)
            fatal("couldn't write whole buffer");
    if (numRead == -1)
        errExit("read");
    
    /* 关闭文件描述符 */
    if (close(inputFd) == -1)
        errExit("close input");
    if (close(outputFd) == -1)
        errExit("close output");
    
    exit(EXIT_SUCCESS);

}
```


该程序可处理任意文件类型（如普通文件→终端、终端→普通文件、不同终端之间的复制），体现了通用I/O模型的统一性。

总结

本章介绍的UNIX/Linux通用I/O模型，是通过“文件描述符+4个基础系统调用（open/read/write/close）”实现的统一I/O操作框架，其核心是“所有设备/文件都抽象为文件，用同一套接口操作”，这也是UNIX系统“一切皆文件”设计思想的具体体现。

要不要我帮你整理一份通用I/O模型核心知识点的详细清单？