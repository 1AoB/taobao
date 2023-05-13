#include "file_op.h"
#include "common.h"
using namespace std;
using namespace wxn;

int main(void)
{
    const char *filename = "file_op.txt";
    largefile::FileOperation *fileOP =
        new largefile::FileOperation(filename,
                                     O_CREAT | O_RDWR | O_LARGEFILE); // 大文件
    int fd = fileOP->open_file();
    if (fd < 0)
    {
        fprintf(stderr, "open file %s failed.reason: %s\n",
                filename, strerror(-fd));
        exit(-1);
    }

    char buffer[65];
    buffer[65 - 1] = '\0';
    memset(buffer, '8', sizeof(buffer) - 1); //////////8
    // int FileOperation::pwrite_file(const char *buf, int32_t nbytes, const int64_t offset);
    int ret = fileOP->pwrite_file(buffer, sizeof(buffer) - 1, 128); // 将buffer写入fd中,偏移128个位置，写下64个8
    if (ret < 0)
    {
        if (ret == largefile::EXIT_DISK_OPER_INCOMPLETE)
        { // read or write length is less than required.
            fprintf(stderr, "write length is less than required.");
        }
        else
            fprintf(stderr,
                    "pwrite_file %s failed.reason:%s\n", filename, strerror(-ret));
    }

    // buffer清空
    memset(buffer, 0, 64);
    // 从fd中读取,写入到buffer，偏移128个位置开始读取64个字符
    ret = fileOP->pread_file(buffer, sizeof(buffer) - 1, 128);
    if (ret < 0)
    {
        if (ret == largefile::EXIT_DISK_OPER_INCOMPLETE)
        { // read or write length is less than required.
            fprintf(stderr, "read length is less than required.");
        }
        else
            fprintf(stderr, "pread_file %s failed.reason:%s\n",
                    filename, strerror(-ret));
    }
    else
    {
        buffer[sizeof(buffer) - 1] = '\0';
        printf("read:%s\n", buffer);
    }

    // write_file
    memset(buffer, '9', 64); /////////////////////////9
    // 将buffer写入fd中,直接从下标为0的位置开始写64个9
    ret = fileOP->write_file(buffer, sizeof(buffer) - 1);
    if (ret < 0)
    {
        if (ret == largefile::EXIT_DISK_OPER_INCOMPLETE)
        { // read or write length is less than required.
            fprintf(stderr, "write length is less than required.");
        }
        else
            fprintf(stderr, "write_file %s failed.reason:%s\n",
                    filename, strerror(-ret));
    }

    fileOP->close_file();
    return 0;
}
// g++ file_op_test.cpp file_op.cpp