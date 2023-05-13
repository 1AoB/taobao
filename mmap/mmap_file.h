// 内存映射
#ifndef MMAP_FILE_H
#define MMAP_FILE_H
#include <unistd.h> //标准库
#include <stdint.h> //int32_t

// 找不到某个类,第一先想是不是没有包含头文件;第二是不是没包含命名空间
#include "common.h"

namespace wxn // 命名空间
{
    namespace largefile // 大文件
    {

        class MMapFile
        {
        public:
            // 无参构造函数
            MMapFile();
            // 防止隐式转换
            explicit MMapFile(const int fd);
            MMapFile(const MMapOption &mmap_options, const int fd);

            ~MMapFile();

            // 同步文件
            bool sync_file();
            // 将文件映射到内存,同时设置访问权限:默认不可写
            bool map_file(const bool write = false);
            // 获取映射到内存数据的首地址
            void *get_data() const;
            // 获取映射数据的大小
            int32_t get_size() const;

            // 解除映射
            bool munmap_file();
            // 重新映射
            bool remap_file();

        private:
            // 调整以后的大小,扩容,(扩容的新大小:new_size)
            bool ensure_file_size(const int32_t new_size);

        private: // 私有后面加_
            int32_t size_;
            int fd_;
            void *data_;

            struct MMapOption mmap_file_option_;
        };

    }
}

#endif