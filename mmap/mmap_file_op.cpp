#include "mmap_file_op.h"
#include "common.h"
#include "file_op.h"
#include "mmap_file.h"
static int debug = 1;

namespace wxn
{
    namespace largefile
    {

        int MMapFileOperation::pread_file(char *buf, const int32_t size, const int64_t offset)
        {
            // 情况1,内存已经映射
            // 此时一次读不完
            if (is_mapped_ && (offset + size) > map_file_->get_size())
            {
                if (debug)
                {
                    ////////////////出错,原因:__PRI64_PREFIX写错////////////////////////
                    fprintf(stdout, "MMapFileOperation::pread,size=%d,offset=%"__PRI64_PREFIX
                                    "d,map file size:%d.need remap\n",
                            size, offset, map_file_->get_size());
                }
                map_file_->remap_file(); // 扩容
            }
            if (is_mapped_ && (offset + size) <= map_file_->get_size())
            {
                memcpy(buf, (char *)map_file_->get_data() + offset, size);
                return TFS_SUCCESS;
            }

            // 情况2,内存没有映射or要读取的数据映射不全
            return FileOperation::pread_file(buf, size, offset);
        }

        int MMapFileOperation::pwrite_file(const char *buf, const int32_t size, const int64_t offset)
        {
            // 情况1,内存已经映射
            // 此时一次写不完
            if (is_mapped_ && (offset + size) > map_file_->get_size())
            {
                if (debug)
                {
                    ////////////////出错////////////////////////破案了:_PRI_PREFIX不对,应该是__PRI64_PREFIX
                    if (debug)
                        fprintf(stdout, "MMapFileOperation: pwrite,size: %d, offset: %" __PRI64_PREFIX "d,map file size: %d. need remap\n",
                                size, offset, map_file_->get_size());
                    map_file_->remap_file();
                }
                map_file_->remap_file(); // 扩容
            }
            if (is_mapped_ && (offset + size) <= map_file_->get_size())
            {
                memcpy((char *)map_file_->get_data() + offset, buf, size);
                return TFS_SUCCESS;
            }

            // 情况2,内存没有映射or要写的数据映射不全
            return FileOperation::pwrite_file(buf, size, offset);
        }

        // 进行映射
        int MMapFileOperation::mmap_file(const MMapOption &mmap_option)
        {
            if (mmap_option.max_map_size_ < mmap_option.first_map_size_)
            {
                return TFS_ERROR;
            }
            if (mmap_option.max_map_size_ <= 0)
            {
                return TFS_ERROR;
            }
            // 打开文件句柄
            int fd = check_file();
            if (fd < 0)
            {
                // 打开错误
                fprintf(stderr, "MMapFileOperation::mmap_file - check_file failed!\n");
                return TFS_ERROR;
            }
            // 此时已确定文件已打开
            if (!is_mapped_)
            {
                if (map_file_)
                {
                    delete (map_file_);
                }
                map_file_ = new MMapFile(mmap_option, fd);
                is_mapped_ = map_file_->map_file(true); // 可以写
            }
            if (is_mapped_)
            {
                return TFS_SUCCESS;
            }
            else
            {
                return TFS_ERROR;
            }
        }
        // 解除映射
        int MMapFileOperation::munmap_file()
        {
            if (is_mapped_ && map_file_ != NULL)
            {
                delete (map_file_); // 释放map_file_
                is_mapped_ = false; // 是否映射置为假
            }
            return TFS_SUCCESS;
        }

        // 获取映射的数据
        void *MMapFileOperation::get_map_data() const
        {
            if (is_mapped_) // 已经映射成功,才可以得到映射数据
            {
                return map_file_->get_data();
            }

            return NULL;
        }

        // 内存数据同步到磁盘
        int MMapFileOperation::flush_file()
        {
            // 内存数据同步到磁盘
            if (is_mapped_)
            {
                if (map_file_->sync_file())
                {
                    return TFS_SUCCESS;
                }
                else
                {
                    return TFS_ERROR;
                }
            }
            return FileOperation::flush_file();
        }
    }
}