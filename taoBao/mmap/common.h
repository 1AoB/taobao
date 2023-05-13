#ifndef _COMMON_H_INCLUDED_
#define _COMMON_H_INCLUDED_

#include <iostream>
#include <stdint.h>
#include <errno.h>
#include <error.h>
#include <stdio.h> //在C语言中，标准错误输出流（stderr）是通过stdio.h头文件中的全局变量定义的。

// open头文件
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <string>
#include <string.h>

// close 函数
#include <unistd.h>

// fstat
#include <sys/stat.h>

// exit()
#include <stdlib.h>

//__PRI64_PREFIX
#include <inttypes.h>

// assert
#include <assert.h>

// std::stringstream
#include <sstream>

namespace wxn
{
    namespace largefile
    {
        // 操作成功
        const int32_t TFS_SUCCESS = 0;
        // 操作失败,错误
        const int32_t TFS_ERROR = -1;
        // 操作未完成
        const int32_t EXIT_DISK_OPER_INCOMPLETE = -8012; // read or write length is less than required.
        // 已经load的报错 index is loaded when create or load
        const int32_t EXIT_INDEX_ALREADY_LOADED_ERROR = -8013;
        // meta found in index when insert
        const int32_t EXIT_META_UNEXPECT_FOUND_ERROR = -8014;
        // index is corrupt.
        const int32_t EXIT_INDEX_CORRUPT_ERROR = -8015;
        // 块id冲突
        const int32_t EXIT_BLOCKID_CONFLICF_ERROR = -8016;
        // 桶大小错误
        const int32_t EXIT_BUCKET_CONFIGURE_ERROR = -8017;
        // key不存在
        const int32_t EXIT_META_NOT_FOUND_ERROR = -8018;
        const int32_t EXIT_BLOCKID_ZERO_ERROR = -8019;
        //
        static const std::string MAINBLOCK_DIR_PREFIX = "/mainblock/";
        static const std::string INDEX_DIR_PREFIX = "/index/";
        static const mode_t DIR_MODE = 0755; // mode :权限 ,dir:目录

        // 更新的类型
        enum OperType
        {
            C_OPER_INSERT = 1, // 从1开始
            C_OPER_DELETE
        };
        struct MMapOption // 扩容
        {
            // 要映射的最大的内存大小
            int32_t max_map_size_; // 3M
            // 第一次允许他分配的数量
            int32_t first_map_size_; // 4K
            // 每次扩容增加的尺寸
            int32_t per_map_size_; // 4K  per是每的一次,表示每次扩容增加的尺寸
        };

        // 块的信息
        //  当删除的文件达到一定比例时,淘宝就会在夜间人少时,整理块,把之前已经删掉的文件清掉,把文件排列整齐
        struct BlockInfo
        {
            uint32_t block_id_;      // 块编号   1 ......2^32-1  TFS = NameServer + DataServer
            int32_t version_;        // 块当前版本号
            int32_t file_count_;     // 当前已保存文件总数
            int32_t size_t_;         // 当前已保存文件数据总大小
            int32_t del_file_count_; // 已删除的文件数量
            int32_t del_size_;       // 已删除的文件数据总大小
            uint32_t seq_no_;        // 下一个可分配的文件编号  1 ...2^64-1
            // 构造函数,全部清零
            BlockInfo()
            {
                memset(this, 0, sizeof(BlockInfo));
            }
            inline bool operator==(const BlockInfo &rhs) const
            {
                // 所有的成员都相等,则两个块相等
                return block_id_ == rhs.block_id_ &&
                       version_ == rhs.version_ &&
                       file_count_ == rhs.file_count_ &&
                       size_t_ == rhs.size_t_ &&
                       del_file_count_ == rhs.del_file_count_ &&
                       seq_no_ == rhs.seq_no_;
            }
        };

        // 淘宝核心存储引擎之索引文件核心头文件-MetaInfo
        struct MetaInfo
        {
        public:
            // 无参构造函数胡
            MetaInfo()
            {
                init();
            }

            // 有参构造函数
            MetaInfo(const uint64_t fileid,
                     const int32_t in_offset,
                     const int32_t file_size,
                     const int32_t next_meta_offset)
            {
                fileid_ = fileid;
                location_.inner_offset_ = in_offset;
                location_.size_ = file_size;
                next_meta_offset_ = next_meta_offset;
            }

            // 拷贝构造函数
            MetaInfo(const MetaInfo &meta_info)
            {
                memcpy(this, &meta_info, sizeof(MetaInfo)); // 这就搞定了???
            }

            // 防止浅拷贝,重载复制运算符
            MetaInfo &operator=(const MetaInfo &meta_info)
            {
                // 如果是自己赋给自己,就返回自己
                if (this == &meta_info)
                {
                    return *this;
                }
                fileid_ = meta_info.fileid_;
                location_.inner_offset_ = meta_info.location_.inner_offset_;
                location_.size_ = meta_info.location_.size_;
                next_meta_offset_ = meta_info.next_meta_offset_;
            }

            // 克隆
            MetaInfo &clone(const MetaInfo &meta_info)
            {
                assert(this != &meta_info); // 断言,自己克隆自己,直接让程序挂掉
                fileid_ = meta_info.fileid_;
                location_.inner_offset_ = meta_info.location_.inner_offset_;
                location_.size_ = meta_info.location_.size_;
                next_meta_offset_ = meta_info.next_meta_offset_;
                return *this;
            }

            // 重载==运算符
            bool operator==(const MetaInfo &rhs) const
            {
                return fileid_ == rhs.fileid_ &&
                       location_.inner_offset_ == rhs.location_.inner_offset_ &&
                       location_.size_ == rhs.location_.size_ &&
                       next_meta_offset_ == rhs.next_meta_offset_;
            }

            // 得到key
            uint64_t get_key() const ////////fileid_作为key//////////
            {
                return fileid_;
            }
            // 设置key
            uint64_t set_key(const uint64_t key) /////fileid_作为key/////
            {
                fileid_ = key;
            }
            // 得到文件编号
            uint16_t get_file_id() const //////file_id//////////
            {
                return fileid_;
            }
            // 设置文件编号
            uint64_t set_file_id(const uint64_t file_id) //////file_id/////
            {
                fileid_ = file_id;
            }
            // 得到文件在块内部的偏移量
            int32_t get_offset() const
            {
                return location_.inner_offset_;
            }
            // 设置文件在块内部的偏移量
            void set_offset(const int32_t file_id)
            {
                location_.inner_offset_ = file_id;
            }
            // 得到文件大小
            int32_t get_size() const
            {
                return location_.size_;
            }
            // 设置文件大小
            void set_size(const int32_t file_size)
            {
                location_.size_ = file_size;
            }
            // 得到当前哈希链下一个节点在索引文件中的偏移量
            int32_t get_next_meta_offset() const
            {
                return next_meta_offset_;
            }
            // 设置当前哈希链下一个节点在索引文件中的偏移量
            void set_next_meta_offset(const int32_t offset)
            {
                next_meta_offset_ = offset;
            }

        private:              // 后面加_表示私有变量
            uint64_t fileid_; // 文件编号
            struct            // 文件元数据
            {
                int32_t inner_offset_; // 文件在块内部的偏移量
                int32_t size_;         // 文件大小
            } location_;
            int32_t next_meta_offset_; // 当前哈希链下一个节点在索引文件中的偏移量

        private:
            // 初始化
            void init()
            {
                fileid_ = 0;
                location_.inner_offset_ = 0;
                location_.size_ = 0;
                next_meta_offset_ = 0;
            }
        };
    }
}

#endif // _COMMON_H_INCLUD