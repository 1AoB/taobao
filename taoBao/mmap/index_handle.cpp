#include "index_handle.h"

static int debug = 1;

namespace wxn
{
    namespace largefile
    {
        IndexHandle::IndexHandle(const std::string &base_path, const uint32_t main_block_id)
        {
            // create file_op_handle object
            std::stringstream tmp_stream;
            tmp_stream << base_path << INDEX_DIR_PREFIX << main_block_id; // root/martin/index/1

            std::string index_path;
            tmp_stream >> index_path;

            file_op_ = new MMapFileOperation(index_path, O_CREAT | O_RDWR | O_LARGEFILE);
            is_load_ = false;
        }
        IndexHandle::~IndexHandle()
        {
            // 析构就是资源的清理
            if (file_op_)
            {
                delete file_op_;
                file_op_ = NULL;
            }
        }

        int IndexHandle::remove(const uint32_t logic_block_id)
        {
            if (is_load_)
            {
                if (logic_block_id != block_info()->block_id_)
                {
                    fprintf(stderr, "block id conflict. bolckid: %d, index blokid: %d\n",
                            logic_block_id,
                            block_info()->block_id_);
                    return EXIT_BLOCKID_CONFLICF_ERROR;
                }
            }

            int ret = file_op_->munmap_file();
            if (TFS_SUCCESS != ret)
            {
                return ret;
            }

            ret = file_op_->unlink_file();
            return ret;
        }

        // 内存数据同步到磁盘
        int IndexHandle::flush()
        {
            int ret = file_op_->flush_file();

            if (TFS_SUCCESS != ret)
            {
                fprintf(stderr, "index flush fail , ret: %d error desc: %s\n",
                        ret,
                        strerror(errno));
            }

            return ret;
        }

        int IndexHandle::create(const uint32_t logic_block_id, const int32_t bucket_size, const MMapOption map_option)
        {
            int ret = TFS_SUCCESS;

            if (debug)
            {
                //%u 是 C/C++ 中的格式化输出符号，用于将无符号整数（unsigned int）格式化为字符串输出
                printf("create index,block id:%u,bucket size:%d,max_mmap_size:%d,first mmap size:%d,per mmap size:%d\n",
                       logic_block_id,
                       bucket_size,
                       map_option.max_map_size_,
                       map_option.first_map_size_,
                       map_option.per_map_size_);
            }

            if (is_load_)
            {
                return EXIT_INDEX_ALREADY_LOADED_ERROR;
            }

            int64_t file_size = file_op_->get_file_size();
            if (file_size < 0)
            {
                return EXIT_INDEX_ALREADY_LOADED_ERROR;
            }
            else if (file_size == 0) // empty file
            {
                IndexHeader i_header;
                i_header.block_info.block_id_ = logic_block_id;
                i_header.block_info.seq_no_ = 1;
                i_header.bucket_size = bucket_size;

                // 总大小=哈希头+哈希桶
                i_header.index_file_size = sizeof(IndexHeader) + bucket_size * sizeof(int32_t);
                std::cout << "debug :" << sizeof(IndexHeader) << "," << bucket_size * sizeof(int32_t) << std::endl;
                std::cout << "debug :" << i_header.index_file_size << std::endl;

                // std::cout << "debug :" << index_header()->index_file_size << std::endl;
                //  index header + total buckets
                char *init_data = new char[i_header.index_file_size];
                memcpy(init_data, &i_header, sizeof(IndexHeader));
                memset(init_data + sizeof(IndexHeader), 0,
                       i_header.index_file_size - sizeof(IndexHeader));

                // write index header and buckets into index file
                ret =
                    file_op_->pwrite_file(
                        init_data,
                        i_header.index_file_size,
                        0);

                delete[] init_data;

                init_data = NULL;

                if (ret != TFS_SUCCESS)
                {
                    return ret;
                }

                ret = file_op_->flush_file();

                if (ret != TFS_SUCCESS)
                {
                    return ret;
                }

            } // empty file
            else
            {
                // file size > 0 ,index already exist
                return EXIT_META_UNEXPECT_FOUND_ERROR;
            }
            // 创建成功后,需要映射
            ret = file_op_->mmap_file(map_option);
            if (ret != TFS_SUCCESS)
            {
                return ret;
            }

            if (debug)
                printf("init blockid:  %d index successful. data file size: %d, index file size: %d, bucket_size: %d, free head offset: %d, seqno: %d, size: %d, filecount: %d, del_size: %d, del_file_count: %d version: %d\n",
                       logic_block_id,
                       index_header()->data_file_offset,
                       index_header()->index_file_size,
                       index_header()->bucket_size,
                       index_header()->free_head_offset,
                       block_info()->seq_no_,
                       block_info()->size_t_,
                       block_info()->file_count_,
                       block_info()->del_size_,
                       block_info()->del_file_count_,
                       block_info()->version_);

            is_load_ = true;
            return TFS_SUCCESS;
        }

        int IndexHandle::load(const uint32_t logic_block_id, const int32_t _bucket_size, const MMapOption map_option)
        {

            int ret = TFS_SUCCESS;
            if (is_load_)
            {
                // 已经加载过了,就无序加载了
                return EXIT_INDEX_ALREADY_LOADED_ERROR;
            }
            std::cout << "=======1=====" << std::endl;
            int64_t file_size = file_op_->get_file_size();
            if (file_size < 0)
            {
                std::cout << "=====1.3=======" << std::endl;
                return file_size;
            }
            else if (file_size == 0)
            {
                std::cout << "=====1.6=======" << std::endl;
                return EXIT_INDEX_CORRUPT_ERROR; // corrupt:有错误的
            }
            std::cout << "=====2=======" << std::endl;
            // 进行内存映射
            MMapOption tmp_map_option = map_option;
            if (file_size > tmp_map_option.first_map_size_ && file_size <= tmp_map_option.max_map_size_)
            {
                tmp_map_option.first_map_size_ = file_size;
            }

            /////////////////////////////主要的语句///////////////////////////////////////////
            ret = file_op_->mmap_file(tmp_map_option);
            if (ret != TFS_SUCCESS)
            {
                return ret;
            }

            if (debug)
            {
                printf("IndexHandle::load - bucket_size(): %d, index_header()->bucket_size_: %d,  block id: %d\n",
                       bucket_size(),
                       index_header()->bucket_size,
                       block_info()->block_id_);
            }
            std::cout << "debug:Successfully!" << std::endl;

            if (0 == _bucket_size ||
                0 == block_info()->block_id_)
            {
                fprintf(stderr, "Index corrupt error . block_id :%u, bucket size :%d\n",
                        block_info()->block_id_,
                        bucket_size());
                return EXIT_INDEX_CORRUPT_ERROR;
            }
            // check file size
            int32_t index_file_size = sizeof(IndexHeader) + bucket_size() * sizeof(int32_t);

            if (file_size < index_file_size)
            {
                fprintf(stderr, "Index corrupt error,blockid: %u, bucket size: %d, file size: %ld, index file size: %d\n",
                        block_info()->block_id_,
                        bucket_size(),
                        file_size,
                        index_file_size);
                return EXIT_INDEX_CORRUPT_ERROR;
            }

            // check bucket size  //bucket桶
            if (logic_block_id != block_info()->block_id_)
            {
                fprintf(stderr,
                        "block id conflic.blockid:%u,index blockid:%u \n",
                        logic_block_id,
                        block_info()->block_id_);
                return EXIT_BLOCKID_CONFLICF_ERROR;
            }
            // check buck_size
            if (_bucket_size != IndexHandle::bucket_size())
            {
                fprintf(stderr,
                        "index configure err, old bucket size:%d,new bucket size:%d\n",
                        bucket_size(), _bucket_size);
                return EXIT_BUCKET_CONFIGURE_ERROR;
            }

            is_load_ = true;
            if (debug)
            {
                printf("load blockid:  %d index successful. data file size: %d, index file size: %d, bucket_size: %d, free head offset: %d, seqno: %d, size: %d, filecount: %d, del_size: %d, del_file_count: %d version: %d\n",
                       logic_block_id,
                       index_header()->data_file_offset,
                       index_header()->index_file_size,
                       index_header()->bucket_size,
                       index_header()->free_head_offset,
                       block_info()->seq_no_,
                       block_info()->size_t_,
                       block_info()->file_count_,
                       block_info()->del_size_,
                       block_info()->del_file_count_,
                       block_info()->version_);
            }
            return TFS_SUCCESS;
        }
        // 更新块信息
        int IndexHandle::update_block_info(
            const OperType oper_type,  // 更新的类型
            const uint32_t modify_size // 更新的长度
        )
        {
            if (block_info()->block_id_ == 0)
            {
                return EXIT_BLOCKID_ZERO_ERROR;
            }
            if (oper_type == C_OPER_INSERT)
            {
                ++block_info()->version_;    // 版本号
                ++block_info()->file_count_; // 文件数量
                ++block_info()->seq_no_;
                block_info()->size_t_ += modify_size; // 增加文件大小
            }
            else if (oper_type == C_OPER_DELETE)
            {
                ++block_info()->version_;               // 版本+1
                --block_info()->file_count_;            // 文件数量-1
                block_info()->size_t_ -= modify_size;   // 大小要减
                ++block_info()->del_file_count_;        // 已删除文件的数量+1
                block_info()->del_size_ += modify_size; // 已删除文件的大小
            }

            if (debug)
                printf("update block info. blockid: %u, version: %u, file count: %u, size: %u, del file count: %u, del size: %u, seq no: %u, oper type: %d\n",
                       block_info()->block_id_,
                       block_info()->version_,
                       block_info()->file_count_,
                       block_info()->size_t_,
                       block_info()->del_file_count_,
                       block_info()->del_size_,
                       block_info()->seq_no_,
                       oper_type);

            return TFS_SUCCESS;
        }

        // 为响应block_write_test.cpp的号召,我们定义了一个api,专门写入metaInfo的
        int IndexHandle::write_segment_meta(const uint64_t key, MetaInfo &meta)
        {
            // 他当前偏移量时哪里,这两个变量对于hash_find来说,就是传入传出参数
            int32_t current_offset = 0, previous_offset = 0; // 他前一个偏移是哪里
            // 偏移量肯定是从0开始查
            // 好,接下来,我们来实现metaInfo的写入
            // 来,啊!写这个meta,也分几步
            // 1.第一步,我们要查这个key存在吗?
            // 存在怎么处理,不存在又要干嘛
            // 存在就不能写了,写就覆盖啦!不存在才可以写
            // 在这里,核心的步骤:你要查找:从文件哈希表中查找key是否存在
            // 到时候,我们写定义一个哈希find(hash_find(key,current_offset,previous_offset))去查存不存在,啊!
            // 2.第二步,如果不存在,就写入meta到哈希表中,
            // 到时候,我们可以搞一个hash_insert(slot, previous_offset, meta);去插入(slot就是他要插入的哪个桶)

            // 接下来,我们就按照这个思路去do it
            // just do it! gogogo!!!

            // 好!诶~

            int ret = hash_find(key, current_offset, previous_offset);
            if (TFS_SUCCESS == ret) // 找到了,就不能写入了,写入了就把原来的给覆盖掉了
            {
                return EXIT_META_UNEXPECT_FOUND_ERROR;
            }
            else if (EXIT_META_NOT_FOUND_ERROR != ret)
            {
                return ret;
            }

            // 2.没找到、不存在就写入meta 到文件哈希表中
            // hash_insert(key, previous_offset, meta);
            ret = hash_insert(key, previous_offset, meta);
            return ret;
        }

        // 根据文件id读meta_info
        int IndexHandle::read_segment_meta(const uint64_t key, MetaInfo &meta)
        {
            int32_t current_offset = 0, previous_offset = 0;

            // 1.确定key 存放的桶(slot)的位置
            // int32_t slot = static_cast<uint32_t>(key)  % bucket_size();

            int32_t ret = hash_find(key, current_offset, previous_offset);

            if (TFS_SUCCESS == ret) // exist
            {
                ret = file_op_->pread_file(reinterpret_cast<char *>(&meta),
                                           sizeof(MetaInfo),
                                           current_offset);
                return ret;
            }
            else
            {
                return ret;
            }
        }

        // 根据文件id删除meta_info
        int IndexHandle::delete_segment_meta(const uint64_t key)
        {
            int32_t current_offset = 0, previous_offset = 0;

            // 人家不在,你删个毛线
            int32_t ret = hash_find(key, current_offset, previous_offset);
            if (ret != TFS_SUCCESS)
            {
                return ret;
            }

            // 它的上一个节点指向它的下一个节点,就把他删掉了
            // 上一个节点已经知道了previous_offset,现在我们要找下一个节点
            MetaInfo meta_info;
            // 读取当前节点
            ret = file_op_->pread_file(reinterpret_cast<char *>(&meta_info),
                                       sizeof(MetaInfo),
                                       current_offset);
            if (TFS_SUCCESS != ret)
            {
                return ret;
            }
            else
            {
                if (debug)
                {
                    std::cout << "读取成功!!!!!!!!!!!!!!!!!!!!!!!!!!!!!" << std::endl;
                }
            }

            // 通过meta_info找到当前节点的下一个节点
            int32_t next_pos = meta_info.get_next_meta_offset();

            if (previous_offset == 0) // 前面没有节点了.它就是首节点,我们要删除首节点
            {
                int32_t slot = static_cast<uint32_t>(key) % bucket_size();
                bucket_slot()[slot] = next_pos; // 找到槽,让槽指向它的下一个节点
            }
            else
            {
                MetaInfo pre_meta_info;
                // 读取当前节点的上一个节点,pre_meta_info是传入传出参数
                ret = file_op_->pread_file(reinterpret_cast<char *>(&pre_meta_info),
                                           sizeof(MetaInfo),
                                           previous_offset);
                if (TFS_SUCCESS != ret)
                {
                    return ret;
                }
                // 上一个节点链上下一个节点
                pre_meta_info.set_next_meta_offset(next_pos);
                // 在更新前一个节点的指针之前，它是存储在文件系统中的。
                // 当需要删除当前节点时，需要先读取当前节点和前一个节点的数据块，
                // 然后将前一个节点的指针更新，最后将更新后的前一个节点数据块重新写回文件系统中。
                // 在这个"过程"中，前一个节点的数据块被"读取到内存"中，并在"内存中进行了修改"，
                // 但是"修改后的数据并没有被写回到文件系统中"。
                // 也就是说,你读出来指的是将其从文件系统读到内存中,你的操作是在内存中的进行的,
                // 你的文件系统没有改变,所以你还要"将你在内存中的操作后的结果"重新写入到文件系统中!
                ret = file_op_->pwrite_file(reinterpret_cast<char *>(&pre_meta_info),
                                            sizeof(MetaInfo),
                                            previous_offset);
                if (TFS_SUCCESS != ret) // 写失败,说明删除失败
                {
                    return ret;
                }
            }

            // 重用
            //  删除并非真的删除,只是标记了该节点不可用了,然后再将该节点找个机会重用(当然,如果可重用节点过多,多到一定比例时,淘宝分布式文件系统会在人流量少时,将这些可重用节点进行删除[不过该功能,我并没实现])
            //  把删除节点加入可重用节点链表(前插法)
            //  1.第一步
            meta_info.set_next_meta_offset(free_head_offset()); // index_header()->free_head_offset_;
            ret = file_op_->pwrite_file(reinterpret_cast<char *>(&meta_info), sizeof(MetaInfo), current_offset);
            if (TFS_SUCCESS != ret)
            {
                return ret;
            }
            // 2.第二步
            index_header()->free_head_offset = current_offset;
            if (debug)
            {
                std::cout << "=========================================================================" << std::endl;
                printf("index_header()->free_head_offset:%d\n", index_header()->free_head_offset);
                std::cout << "///////////////////////////////////////////////////////////" << std::endl;
                std::cout << "current_offset:" << current_offset << ",index_header()->free_head_offset:" << index_header()->free_head_offset << std::endl;
                printf("delete_segment_meta - Reuse metainfo, current_offset: %d\n", current_offset);
                std::cout << "=========================================================================" << std::endl;
            }

            update_block_info(C_OPER_DELETE, meta_info.get_size());

            return TFS_SUCCESS;
        }

        int IndexHandle::hash_find(const uint64_t key,
                                   int32_t current_offset,
                                   int32_t previous_offset)
        {
            int ret = TFS_SUCCESS;
            MetaInfo meta_info;
            // 先清0
            current_offset = previous_offset = 0;

            // 根据键找到哪个桶
            // 存在哪个桶是吧!这个桶也叫slot
            // 1.确定key存放的桶(slot)的位置
            int32_t slot = static_cast<uint32_t>(key) % bucket_size(); // 就这么简单即可知道是哪个桶

            // 这要怎么读?我们定义一个函数int32_t* bucket_slot()返回这个桶这个数组的首节点
            // 来,我们得到第一个节点的位置;
            // 2.读取桶首节点存储的第一个节点的偏移量;如果偏移量为0,直接返回(EXIT_META_NOT_FOUND_ERROR)key不存在
            // 3.再根据偏移量读取存储的metainfo
            // 4.与key比较,是不是我们要的key,相等就设置current_offset与previous_offset并返回TFS_SUCCESS;
            // 否则,继续执行5
            // 5.从metainfo中取得下一个节点在文件中的偏移量;如果偏移量为0,直接返回(EXIT_META_NOT_FOUND_ERROR)key不存在,
            // 否则跳转到3继续执行
            int32_t pos = bucket_slot()[slot]; // 这种写法很高级

            // 因为是数组,你要遍历,这节点的key是不是你要的key
            // 孤独一个写代码...
            for (; pos != 0;)
            {
                // 读
                ret = file_op_->pread_file(reinterpret_cast<char *>(&meta_info), sizeof(MetaInfo), pos);
                if (TFS_SUCCESS != ret)
                {
                    return ret;
                }

                // 哈希比较
                if (hash_compare(key, meta_info.get_key()))
                {
                    current_offset = pos;
                    return TFS_SUCCESS;
                }

                previous_offset = pos;                  // pos变为上一个节点
                pos = meta_info.get_next_meta_offset(); // 读取下一个节点
            }
            return EXIT_META_NOT_FOUND_ERROR;
        }

        int32_t IndexHandle::hash_insert(const uint64_t key, int32_t previous_offset, MetaInfo &meta)
        {
            int ret = TFS_SUCCESS;
            MetaInfo tmp_meta_info; // tmp表示临时变量
            int32_t current_offset = 0;

            // 1.确定key 存放的桶(slot)的位置
            int32_t slot = static_cast<uint32_t>(key) % bucket_size();

            // 2.确定meta 节点存储在文件中的偏移量
            if (free_head_offset() != 0) //  有可重用的节点
            {
                // tmp_meta_info传入传出参数
                ret = file_op_->pread_file(reinterpret_cast<char *>(&tmp_meta_info),
                                           sizeof(MetaInfo),
                                           free_head_offset());
                if (TFS_SUCCESS != ret)
                {
                    return ret;
                }

                current_offset = index_header()->free_head_offset;

                if (debug)
                {
                    std::cout << "///////////////////////////////////////////////////////////" << std::endl;
                    std::cout << "current_offset:" << current_offset << ",index_header()->free_head_offset:" << index_header()->free_head_offset << std::endl;
                    printf("Reuse metainfo, current_offset: %d\n", current_offset);
                }
                index_header()->free_head_offset = tmp_meta_info.get_next_meta_offset();
            }
            else // 没有可重用的节点
            {
                current_offset = index_header()->index_file_size;
                index_header()->index_file_size += sizeof(MetaInfo); // 指向下一个可以希尔MetaInfo的位置
            }

            // 3.将meta 节点写入索引文件中
            meta.set_next_meta_offset(0);
            // 写入
            ret = file_op_->pwrite_file(reinterpret_cast<const char *>(&meta),
                                        sizeof(MetaInfo),
                                        current_offset);
            if (TFS_SUCCESS != ret)
            {
                // 写出错
                // 回滚,index_file_size没有被利用
                index_header()->index_file_size -= sizeof(MetaInfo);
                return ret;
            }

            // 4. 将meta 节点插入到哈希链表中

            // 前一个节点已经存在
            if (0 != previous_offset)
            {
                ret = file_op_->pread_file(reinterpret_cast<char *>(&tmp_meta_info),
                                           sizeof(MetaInfo),
                                           previous_offset);
                if (TFS_SUCCESS != ret)
                {
                    // 读出错
                    // 回滚,index_file_size没有被利用
                    index_header()->index_file_size -= sizeof(MetaInfo);
                    return ret;
                }

                tmp_meta_info.set_next_meta_offset(current_offset);
                ret = file_op_->pwrite_file(reinterpret_cast<const char *>(&tmp_meta_info), sizeof(MetaInfo), previous_offset);
                if (TFS_SUCCESS != ret)
                {
                    // 写出错
                    // 回滚,index_file_size没有被利用
                    index_header()->index_file_size -= sizeof(MetaInfo);
                    return ret;
                }
            }
            else
            { // 不存在前一个节点的情况
                // 直接就是首节点
                bucket_slot()[slot] = current_offset;
            }

            return TFS_SUCCESS;
        }

    }
}
