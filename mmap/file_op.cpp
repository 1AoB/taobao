#include "file_op.h"
#include "common.h"

namespace wxn // 地球村
{
    namespace largefile // 中国
    {
        FileOperation::FileOperation(const std::string &file_name, const int open_flags)
            : fd_(-1), open_flags_(open_flags)
        {
            // strdup重新分配内存,并复制
            this->file_name_ = strdup(file_name.c_str());
        }
        FileOperation::~FileOperation()
        {
            if (fd_ > 0)
            {
                // 有文件,要关闭
                ::close(fd_); //::表示调用全局的库文件,不是命名空间作用域的库文件
            }
            if (NULL != file_name_)
            {
                free(file_name_);
                file_name_ = NULL;
            }
        }
        int FileOperation::open_file()
        {
            if (fd_ > 0)
            {
                // 如果文件已经打开了,就先关闭再打开
                close(fd_);
                fd_ = -1;
            }

            fd_ = open(file_name_, open_flags_, OPEN_MODE);
            if (fd_ < 0)
            {
                return -errno; // 打开失败
            }
            return fd_; // 终于打开成功
        }
        void FileOperation::close_file()
        {
            if (fd_ < 0)
            {
                // 如果已经关闭了,就不用关了
                return;
            }
            ::close(fd_);
            fd_ = -1;
        }
        // 把文件立即写入磁盘中
        int FileOperation::flush_file()
        {
            /*
            如果文件是以同步方式打开的，则在对文件进行写入操作后，
            必须使用flush_file()方法将数据立即写入磁盘中，
            否则数据可能仍然存储在操作系统的缓冲区中而未被写入磁盘。
            因此，在给定的代码中，if (open_flags_ & O_SYNC){}语句检查文件是否已经以同步方式打开，
            如果是，则返回0。
            这表示不需要调用flush_file()方法，因为操作系统会自动处理数据的写入。
            反之，如果文件没有以同步方式打开，则需要手动调用flush_file()方法将数据写入磁盘。
            */
            if (open_flags_ & O_SYNC) // O_SYNC以同步方式打开文件
            {
                return 0;
            }
            int fd = check_file();
            if (fd < 0) // 打不开
            {
                return -fd; //-(-errno)
            }
            return fsync(fd); // fsync（）负责将参数fd所指的文件数据，由系统缓冲区写回磁盘，以确保数据同步。
        }
        // 把文件从磁盘中删除
        int FileOperation::unlink_file()
        {
            close_file(); // 先关闭文件

            return ::unlink(file_name_); // 删除文件
        }
        // 读(有偏移量)
        int FileOperation::pread_file(char *buf, int32_t nbytes, const int64_t offset)
        {
            // ssize_t pread(int fd, void *buf, size_t count, off_t offset);
            // ssize_t pread64(int fd, void *buf, size_t count, off64_t offset);//大文件读
            // buf：是一个指向存储读取数据的缓冲区的指针；
            // offset：表示从文件的哪个位置开始读取数据。
            char *p_tmp = buf;            // p_tmp指针是用于存储读取到的数据的缓冲区起始地址。
            int32_t left = nbytes;        // 剩余的字节数
            int64_t read_offset = offset; // read_offset参数表示从文件的哪个位置开始读取数据。
            int32_t read_len = 0;         // 已读的长度

            int i = 0;

            while (left > 0)
            {
                i++; // 读的次数
                if (i >= MAX_DISK_TIMES)
                {
                    // 超过5次都没读成功,就不再读了
                    break;
                }
                if (check_file() < 0)
                {
                    return -errno;
                }
                read_len = ::pread64(fd_, p_tmp, left, read_offset);
                if (read_len < 0) // 没读到,读取错误
                {
                    read_len = -errno;
                    // 在多线程下,errno在不断的更改,但是read_len不会变
                    if (-read_len == EINTR || EAGAIN == -read_len) // EINTR临时中断,EAGAIN系统让我们继续尝试
                    {
                        continue;
                    }
                    else if (EBADF == -read_len) // 文件描述符已经变坏了
                    {
                        fd_ = -1;
                        return read_len;
                    }
                    else
                    {
                        return read_len;
                    }
                }
                else if (0 == read_len) // 如果文件刚好读到尾部了
                {
                    break; // 已经读完
                }

                // 在每次读取后，需要将缓冲区指针buf重新指向新的缓冲区，并将偏移量offset更新为当前位置
                /*
                虽然这两个参数都涉及到缓冲区和偏移量，但是它们的作用却有所不同。
                buf参数主要用于指向待填充的缓冲区，而offset参数主要用于指定读取数据的起始位置。
                在多次读取时，这两个参数都需要不断更新，以确保在正确的位置读取和存储数据。
                */
                // pread()在每次读完后,需要更新buf和offset,但是read()只需要更新buf
                // read_len = ::pread64(fd_, p_tmp, left, read_offset);
                left -= read_len;         // 剩余的字节数
                p_tmp = p_tmp + read_len; // 文件的起始地址//这行可以注释,在读的时候不必改变文件起始位置
                read_offset += read_len;  // 文件偏移量
            }
            if (0 != left)
            {
                // 没有达到预期
                return EXIT_DISK_OPER_INCOMPLETE;
            }
            return TFS_SUCCESS; // 成功
        }
        // 写(有偏移量)
        int FileOperation::pwrite_file(const char *buf, int32_t nbytes, const int64_t offset)
        {
            // ssize_t pwrite(int fd, const void *buf, size_t count, off_t offset);
            // ssize_t pwrite64(int fd, const void *buf, size_t count, off64_t offset);//大文件写
            const char *p_tmp = buf;       // p_tmp指针是用于存储写到的数据的缓冲区起始地址。
            int32_t left = nbytes;         // 剩余的字节数
            int64_t write_offset = offset; // write_offset参数表示从文件的哪个位置开始写数据。
            int32_t write_len = 0;         // 已写的长度

            int i = 0;

            while (left > 0)
            {
                i++; // 读的次数
                if (i >= MAX_DISK_TIMES)
                {
                    // 超过5次都没读成功,就不再读了
                    break;
                }
                if (check_file() < 0)
                {
                    return -errno;
                }
                write_len = ::pwrite64(fd_, p_tmp, left, write_offset);
                if (write_len < 0) // 没写到,有错误
                {
                    write_len = -errno;
                    // 在多线程下,errno在不断的更改,但是read_len不会变
                    if (-write_len == EINTR || EAGAIN == -write_len) // EINTR临时中断,EAGAIN系统让我们继续尝试
                    {
                        continue;
                    }
                    else if (EBADF == -write_len) // 文件描述符已经变坏了
                    {
                        fd_ = -1;
                        continue;
                    }
                    else
                    {
                        return write_len;
                    }
                }

                left -= write_len;         // 剩余的字节数
                p_tmp = p_tmp + write_len; // 文件的起始地址
                write_offset += write_len; // 文件偏移量
            }                              /*while循环*/
            if (0 != left)
            {
                // 没有达到预期
                return EXIT_DISK_OPER_INCOMPLETE;
            }
            return TFS_SUCCESS; // 成功
        }
        // 直接写(无偏移量)
        int FileOperation::write_file(const char *buf, int32_t nbytes)
        {
            const char *p_tmp = buf; // p_tmp指针是用于存储写到的数据的缓冲区起始地址。
            int32_t left = nbytes;   // 剩余的字节数
            int32_t write_len = 0;   // 已写的长度
            int i = 0;
            while (left > 0)
            {
                ++i;
                if (i >= MAX_DISK_TIMES)
                {
                    break;
                }
                if (check_file() < 0)
                {
                    return -errno;
                }
                write_len = ::write(fd_, p_tmp, left);

                if (write_len < 0) // 没写到,有错误
                {
                    write_len = -errno;
                    // 在多线程下,errno在不断的更改,但是read_len不会变
                    if (-write_len == EINTR || EAGAIN == -write_len) // EINTR临时中断,EAGAIN系统让我们继续尝试
                    {
                        continue;
                    }
                    else if (EBADF == -write_len) // 文件描述符已经变坏了
                    {
                        fd_ = -1;
                        continue;
                    }
                    else
                    {
                        return write_len;
                    }
                }

                left -= write_len;
                p_tmp += write_len;
            }
            if (0 != left)
            {
                // 没有达到预期
                return EXIT_DISK_OPER_INCOMPLETE;
            }
            return TFS_SUCCESS; // 成功
        }

        // 获取文件的大小
        int64_t FileOperation::get_file_size()
        {
            int fd = check_file();
            if (fd < 0)
            {
                return -1;
            }
            // int fstat(int fildes,struct stat *buf); 由文件描述词取得文件状态
            struct stat statusbuf;          // 这个结构体中有文件的大小
            if (fstat(fd, &statusbuf) != 0) // On success, zero is returned.  On error, -1 is returned, and errno is set appropriately.
            {
                // fstat操作失败
                return -1;
            }
            return statusbuf.st_size; // 返回文件大小
        }

        // 截断文件,文件太大了,我要缩小一下
        int FileOperation::ftruncate_file(const int64_t length)
        {
            int fd = check_file();
            if (fd < 0) // 打不开
            {
                return -fd; //-(-errno)
            }

            return ftruncate(fd, length); // 将文件截断到length
            // 如果文件>length,就会删除超出的部分
            // 如果文件<length,就扩容
        }
        // 从文件中进行移动 , offset偏移量
        int FileOperation::seek_file(const int64_t offset)
        {
            int fd = check_file();
            if (fd < 0) // 打不开
            {
                return -fd; //-(-errno)
            }

            return lseek(fd, offset, SEEK_SET); // SEEK_SET参数offset即为新的读写位置。
        }
        int FileOperation::check_file()
        {
            if (fd_ < 0)
            {
                fd_ = open_file();
            }
            return fd_;
        }
    }
}