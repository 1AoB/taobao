#include "mmap_file.h"
#include <stdio.h>
#include <sys/mman.h>

// strerror(errno)
#include <string.h>
#include <errno.h>

// struct stat s;
#include <sys/stat.h>

static int debug = 1; // debug开关

namespace wxn
{
    namespace largefile
    {
        MMapFile::MMapFile() : size_(0), fd_(-1), data_(NULL)
        {
        }

        MMapFile::MMapFile(const int fd) : size_(0), fd_(fd), data_(NULL)
        {
        }

        MMapFile::MMapFile(const MMapOption &mmap_option, const int fd) : size_(0), fd_(fd), data_(NULL)
        {
            this->mmap_file_option_.max_map_size_ = mmap_option.max_map_size_;
            this->mmap_file_option_.first_map_size_ = mmap_option.first_map_size_;
            this->mmap_file_option_.per_map_size_ = mmap_option.per_map_size_;
        }

        MMapFile::~MMapFile()
        {
            if (data_)
            {
                if (debug)
                    printf("mmap_file_destroy,fd:%d,maped size:%d,data:%p\n", fd_, size_, data_);
                msync(data_, size_, MS_SYNC); // 同步,同步方式选择阻塞同步:MS_SYNC,而MS_ASYNC是异步非阻塞
                munmap(data_, size_);         // 解除映射

                size_ = 0;
                data_ = NULL;
                fd_ = -1;
                mmap_file_option_.max_map_size_ = 0;
                mmap_file_option_.first_map_size_ = 0;
                mmap_file_option_.per_map_size_ = 0;
            }
        }
        // 同步文件
        bool MMapFile::sync_file()
        {
            if (NULL != data_ && size_ > 0)
            {
                return msync(data_, size_, MS_ASYNC) == 0; // 这里采用异步同步
            }
            return true;
        }
        // 内存映射
        bool MMapFile::map_file(const bool write)
        {
            int flags = PROT_READ; // 可读
            if (write)             // 是否可写
            {
                flags |= PROT_WRITE;
            }

            if (fd_ < 0)
            {
                return false;
            }
            if (0 == mmap_file_option_.max_map_size_)
            {
                return false;
            }

            if (size_ < mmap_file_option_.max_map_size_)
            {
                size_ = mmap_file_option_.first_map_size_; // 第一次允许他分配的数量
            }
            else
            {
                size_ = mmap_file_option_.max_map_size_; // 要映射的最大的内存大小
            }
            if (!ensure_file_size(size_))
            {
                fprintf(stderr, "ensure_file_size failed in map_file,size:%d\n",
                        size_);
                return false;
            }

            data_ = mmap(0, size_, flags, MAP_SHARED, fd_, 0); // 共享

            if (data_ == MAP_FAILED)
            {
                fprintf(stderr, "mmap file failed : %s", strerror(errno)); // 输出错误

                size_ = 0;
                fd_ = -1;
                data_ = NULL;

                return false; // 内存映射失败
            }

            if (debug)
            {
                printf("mmap file success ,fd:%d,maped size :%d ,data :%p\n",
                       fd_, size_, data_);
            }
            return true;
        }
        void *MMapFile::get_data() const
        {
            return data_;
        }
        int32_t MMapFile::get_size() const
        {
            return size_;
        }
        bool MMapFile::munmap_file()
        {
            if (munmap(data_, size_) == 0)
            {
                return true;
            }
            else
                return false;
        }
        bool MMapFile::remap_file()
        {
            // 1.防御型编程
            if (fd_ < 0 || data_ == NULL)
            {
                fprintf(stderr, "mremap not mapped yet\n");
                return false;
            }
            // 说明已分配到最大的内存且映射,所以这次映射失败
            if (size_ == mmap_file_option_.max_map_size_)
            {
                fprintf(stderr, "already mapped max size,now size:%d , max size:%d\n",
                        size_, mmap_file_option_.max_map_size_);
                return false;
            }
            // 开始重新映射
            int32_t new_size = size_ + mmap_file_option_.per_map_size_;
            if (new_size > mmap_file_option_.max_map_size_)
            {
                new_size = mmap_file_option_.max_map_size_;
            }
            if (!ensure_file_size(new_size))
            {
                fprintf(stderr, "ensure_file_size failed in remap_file,size:%d\n",
                        new_size);
                return false;
            }
            if (debug)
            {
                printf("mremap start,fd:%d,now size:%d,new size:%d,data:%p\n",
                       fd_, size_, new_size, data_);
            }
            // mremap扩大（或缩小）现有的内存映射
            void *new_map_data = mremap(data_, size_, new_size, MREMAP_MAYMOVE);
            if (MAP_FAILED == new_map_data)
            {
                fprintf(stderr, "mremap failed,new size:%d,error desc: %s\n",
                        new_size, strerror(errno));
                return false;
            }
            else
            {
                if (debug)
                {
                    printf("mremap succeeded,fd:%d,now size:%d,new size:%d,old data:%p,new data:%p\n",
                           fd_, size_, new_size, data_, new_map_data);
                    data_ = new_map_data;
                    size_ = new_size;
                }
            }
            return true;
        }
        bool MMapFile::ensure_file_size(const int32_t new_size)
        {
            struct stat s;
            if (fstat(fd_, &s) < 0)
            {
                // 扩容失败
                fprintf(stderr, "fstat error,error desc :%s\n", strerror(errno));
                return false;
            }
            if (s.st_size < new_size) // 扩容到新大小
            {
                if (ftruncate(fd_, new_size) < 0)
                {
                    fprintf(stderr, "ftruncate error,new_size:%d,error desc :%s\n",
                            new_size, strerror(errno));
                    return false;
                }
            }
            return true;
        }
    }
}
