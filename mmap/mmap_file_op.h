// mmap_file_operate文件映射操作

// 文件映射的对象作为原材料

#ifndef WXN_LARGEFILE_MMAPFILE_OP_H
#define WXN_LARGEFILE_MMAPFILE_OP_H

#include "common.h"
#include "file_op.h"
#include "mmap_file.h"

namespace wxn
{
    namespace largefile
    {
        class MMapFileOperation : public FileOperation // 文件映射操作类
        {
        public:
            // MMapFileOperation构造函数,用父类构造函数去初始化file_name与open_flags
            MMapFileOperation(const std::string &file_name,
                              const int open_flags = O_CREAT | O_RDWR | O_LARGEFILE) : FileOperation(file_name, open_flags),
                                                                                       map_file_(NULL), is_mapped_(false)
            {
            }
            // 析构函数
            ~MMapFileOperation()
            {
                if (map_file_)
                {
                    delete (map_file_);
                    map_file_ = NULL;
                }
            }
            // 重新实现
            int pread_file(char *buf, const int32_t size, const int64_t offset);
            int pwrite_file(const char *buf, const int32_t size, const int64_t offset);

            int mmap_file(const MMapOption &mmap_option);
            int munmap_file();

            void *get_map_data() const;
            int flush_file();

        private:
            MMapFile *map_file_; // 映射文件
            bool is_mapped_;     // 是否已经映射
        };
    }
}

#endif // WXN_LARGEFILE_MMAPFILE_OP_H