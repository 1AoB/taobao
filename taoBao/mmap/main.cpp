#include "common.h"
#include "mmap_file.h"

using namespace std;
using namespace wxn;

static const mode_t OPEN_MODE = 0664;
// 内存映射的参数
const static largefile::MMapOption mmap_option =
    {10240000, 4096, 4096}; // 10兆,一页4KB
/*
// 要映射的最大的内存大小
            int32_t max_map_size_; // 3M
            // 第一次允许他分配的数量
            int32_t first_map_size_; // 4K
            int32_t per_map_size_;   // 4K
*/

// int open( const char * pathname,int flags, mode_t mode);
int open_file(string file_name, int open_flags)
{
    int fd = open(file_name.c_str(), open_flags, OPEN_MODE);
    if (fd < 0)
    {
        // 一般负数代表出错,error是正数
        return -errno; // errno    strerror(errno);
    }
    return fd;
}

int main()
{

    // 打开/创建一个文件,取得文件的句柄 open函数
    const char *file_name = "./mmap_file.txt";
    // O_LARGEFILE代表生成超大文件
    int fd = open(file_name, O_RDWR | O_CREAT | O_LARGEFILE);
    if (fd < 0)
    {
        fprintf(stderr, "open file failed: %s,error desc:%s!\n",
                file_name, strerror(-fd)); //-(-error)==error
        return -1;
    }

    // 对象有了
    largefile::MMapFile *map_file =
        new largefile::MMapFile(mmap_option, fd);
    // 映射
    bool is_mapped = map_file->map_file(true); // 设置可写
    if (is_mapped)                             // 如果映射成功
    {
        map_file->remap_file(); // 重新映射

        memset(map_file->get_data(), '9', map_file->get_size()); // 全部设置为8
        map_file->sync_file();                                   // 同步文件
        map_file->munmap_file();                                 // 解除映射
    }
    else
    { // 映射失败
        fprintf(stderr, "map file failed: %s\n", strerror(errno));
    }
    close(fd);
    return 0;
}
// g++ main.cpp mmap_file.cpp