// mmap_file_op.h
// mmap_file_op.cpp
// mmap_file_op.txt
// mmap_file_op_test.cpp
#include "mmap_file_op.h"

#include "common.h"
using namespace std;
using namespace wxn;

// 内存映射的参数
const static largefile::MMapOption mmap_option =
    {10240000, 4096, 4096}; // 10兆,一页4KB

int main(void)
{
    int ret = 0;
    const char *filename = "mmap_file_op.txt";
    largefile::MMapFileOperation *mmfo = new largefile::MMapFileOperation(filename);
    int fd = mmfo->open_file();
    if (fd < 0)
    {
        fprintf(stderr,
                "open file %s failed. reason:%s\n",
                filename, strerror(-fd));
        exit(-1);
    }
    ret = mmfo->mmap_file(mmap_option);
    if (ret == largefile::TFS_ERROR)
    {
        fprintf(stderr,
                "mmap_file %s failed. reason:%s\n", filename, strerror(errno));
        exit(-1);
    }

    ret = mmfo->mmap_file(mmap_option);
    if (ret == largefile::TFS_ERROR)
    {
        fprintf(stderr,
                "mmap_file %s failed. reason:%s\n", filename, strerror(errno));
        exit(-2);
    }

    char buffer[128 + 1];
    memset(buffer, '6', 128);
    buffer[127] = '\n';

    ret = mmfo->pwrite_file(buffer, 128, 8);
    if (ret < 0)
    {
        if (ret == largefile::EXIT_DISK_OPER_INCOMPLETE)
        {
            fprintf(stderr,
                    "pwrite_file: read length is less than required!\n");
        }
        else
        {
            fprintf(stderr,
                    "pwrite_file %s failed.reason: %s\n",
                    filename, strerror(-ret));
        }
    }

    memset(buffer, 0, 128);
    ret = mmfo->pread_file(buffer, 128, 8);
    if (ret < 0)
    {
        if (ret == largefile::EXIT_DISK_OPER_INCOMPLETE)
        {
            fprintf(stderr,
                    "pwrite_file: read length is less than required!\n");
        }
        else
        {
            fprintf(stderr,
                    "pwrite_file %s failed.reason: %s\n",
                    filename, strerror(-ret));
        }
    }
    else
    {
        buffer[128] = '\0';
        printf("has read :%s\n", buffer);
    }

    // flush是指将缓冲区的内容写入到目标设备（例如磁盘、网络等）中，以确保数据持久化保存。
    ret = mmfo->flush_file();
    if (ret == largefile::TFS_ERROR)
    {
        fprintf(stderr,
                "flush_file failed.reaseon:%s\n", strerror(errno));
    }

    memset(buffer, '8', 128);
    buffer[127] = '\n';
    ret = mmfo->pwrite_file(buffer, 128, 4096); // 从4096开始写起

    mmfo->munmap_file();

    mmfo->close_file();
    return 0;
}
// g++  file_op.cpp mmap_file.cpp  mmap_file_op.cpp  mmap_file_op_test.cpp