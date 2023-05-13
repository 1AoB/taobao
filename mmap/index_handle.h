// index_hanle.h
#ifndef WXN_LARGEFILE_INDEX_HANDLE_H_
#define WXN_LARGEFILE_INDEX_HANDLE_H_

#include "common.h"
#include "mmap_file_op.h"

namespace wxn
{
    namespace largefile
    {
        // 块索引头部
        struct IndexHeader
        {
        public:
            // 构造函数
            IndexHeader()
            {
                memset(this, 0, sizeof(IndexHeader));
            }

            // 块信息
            BlockInfo block_info;

            // 脏数据状态标记暂时用不到

            // 确定哈希桶的大小
            int32_t bucket_size;

            // 未使用数据起始的偏移量
            int32_t data_file_offset; ///////块数据的偏移,起始是0/////////

            // 索引文件的当前偏移
            int32_t index_file_size;

            // 可重用的链表节点链表的链头,他们指向了文件哈希索引块
            int32_t free_head_offset;
        };

        // 索引处理类
        class IndexHandle
        {
        public:
            IndexHandle(const std::string &base_path, const uint32_t main_block_id);
            ~IndexHandle();

            // remove index:unmmap and unlink file
            int remove(const uint32_t logic_block_id);
            int flush();

            int create(const uint32_t logic_block_id,
                       const int32_t bucket_size,
                       const MMapOption map_option);
            int load(const uint32_t logic_block_id,
                     const int32_t bucket_size,
                     const MMapOption map_option);

            IndexHeader *index_header()
            {
                return reinterpret_cast<IndexHeader *>(file_op_->get_map_data());
            }
            // 更新块信息
            int update_block_info(
                const OperType oper_type,  // 更新的类型
                const uint32_t modify_size // 更新的长度
            );                             // 更新块信息
            // 从file_op_中获取映射信息
            BlockInfo *block_info()
            {
                return reinterpret_cast<BlockInfo *>(file_op_->get_map_data());
            }
            // 返回这个桶这个数组的首节点
            int32_t *bucket_slot()
            {
                // 因为已经映射到内存了
                // 保存到文件的同时,还映射到了内存
                // 在mmap内存映射中，映射到的内存是指系统内存，而不是磁盘内存。
                // 系统内存（也称为随机访问存储器或RAM）和磁盘内存（也称为永久存储器或硬盘）是计算机中两种不同的存储介质
                // 区别:
                // 1.访问速度：系统内存的访问速度比磁盘内存快得多。
                // 2.系统内存用于临时存储和处理数据。

                // 好!
                // 首先拿到映射的数据
                return reinterpret_cast<int32_t *>(reinterpret_cast<char *>(file_op_->get_map_data()) + sizeof(IndexHeader));
            }
            // 得到桶大小
            int32_t bucket_size() const
            {
                return reinterpret_cast<IndexHeader *>(file_op_->get_map_data())->bucket_size;
            }
            // 得到块数据的偏移量
            int32_t get_block_data_offset()
            {
                return reinterpret_cast<IndexHeader *>(file_op_->get_map_data())->data_file_offset;
            }
            void commit_block_data_offset(const int file_size)
            {
                reinterpret_cast<IndexHeader *>(file_op_->get_map_data())->data_file_offset += file_size;
            }
            int32_t free_head_offset() const
            {
                return reinterpret_cast<IndexHeader *>(file_op_->get_map_data())->free_head_offset;
            }
            /*
            既然都写到这了,有必要复习一下reinterpret_cast:
            reinterpret_cast是C++中的一种类型转换操作符，它允许您将一种类型的指针转换为任何其他类型的指针，而不管这些类型是否相关。
这种类型的转换被认为是所有类型转换操作中最强大和最危险的，因为它允许您将一种类型的对象转换为另一种类型的对象，而不管这两种类型是否兼容。
这意味着如果使用不当，reinterpret_cast可能导致未定义的行为，因此应谨慎使用。

            一般使用这种类型的强转,都是自信且强大的程序员,把轻松留给程序,把危险留给自己,为reinterpret_cast点赞!
            */
            // 为响应block_write_test.cpp的号召,我们定义了一个api,专门写入metaInfo的
            int write_segment_meta(const uint64_t key, MetaInfo &meta);

            // 根据文件id读meta_info
            int read_segment_meta(const uint64_t key, MetaInfo &meta);

            // 根据文件id删除meta_info
            int delete_segment_meta(const uint64_t key);

            // 为响应index_handle.cpp的号召,我们定义一个api,从文件哈希表中查找key是否存在
            // 最好能找到当前的偏移量current_offset和之前的偏移量previous_offset
            int hash_find(const uint64_t key, int32_t current_offset, int32_t previous_offset);
            // 搞一个hash_insert(meta,slot,previous_offset)去插入(slot就是他要插入的哪个桶)
            int32_t hash_insert(const uint64_t key, int32_t previous_offset, MetaInfo &meta);

        private:
            bool hash_compare(const uint64_t left_key, const uint64_t right_key)
            {
                return (left_key == right_key);
            }
            MMapFileOperation *file_op_;
            bool is_load_;
        };
    }
}

#endif // WXN_LARGEFILE_INDEX_HANDLE_H_