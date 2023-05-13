#ifndef WXN_LARGE_FILE_OP_H_
#define WXN_LARGE_FILE_OP_H_

#include "common.h"

namespace wxn
{
    namespace largefile
    {
        // 为了让代码更加方便管理,所以写成这样的命名空间
        class FileOperation
        {
        public:
            // 构造函数 O_LARGEFILE:操作大文件
            FileOperation(const std::string &file_name, const int open_flags = O_RDWR | O_LARGEFILE);
            ~FileOperation();
            // 打开文件
            int open_file();
            // 关闭文件
            void close_file();

            // 把文件立即写入磁盘中
            int flush_file();
            // 把文件删除
            int unlink_file();

            // 内存映射实战值文件操作cpp实现(中)
            //  读(有偏移量)
            virtual int pread_file(char *buf, int32_t nbytes, const int64_t offset);
            // 写(有偏移量)
            virtual int pwrite_file(const char *buf, int32_t nbytes, const int64_t offset);
            // 直接写(无偏移量)
            int write_file(const char *buf, int32_t nbytes);

            // 获取文件的大小
            int64_t get_file_size();
            // 截断文件,文件太大了,我要缩小一下
            int ftruncate_file(const int64_t length);
            // 从文件中进行移动 , offset偏移量
            int seek_file(const int64_t offset);
            // 获取文件描述符,文件句柄
            int get_fd() const
            {
                return fd_;
            }

        protected:
            // 文件描述符,文件句柄
            int fd_;
            // 怎么打开,可读?可写?
            int open_flags_;
            // 文件名
            char *file_name_;

        protected:
            // 允许文件在没有正式开始之前打开
            int check_file();

        protected:
            static const mode_t OPEN_MODE = 0644; // 权限
            static const int MAX_DISK_TIMES = 5;  // 操作系统超过五次失败就不读了
        };
    }
}

#endif