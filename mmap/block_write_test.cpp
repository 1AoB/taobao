// 初始化块信息
// 创建组块文件
// 创建索引文件

#include "file_op.h"      //文件操作
#include "index_handle.h" //主角
#include <sstream>
#include <iostream>
#include "common.h"

using namespace wxn;
using namespace std;

// 内存映射的参数
const static largefile::MMapOption mmap_option =
    {10240000, 4096, 4096}; // 10兆,一页4KB
// 主块文件的大小
const static uint32_t main_blocksize = 1024 * 1024 * 64; // 64兆
// 哈希桶的大小,块大小
const static uint32_t bucket_size = 1000;

// 块id默认是从1开始的,不能用const,不然无法用cin写入
static int32_t block_id = 1;

static int debug = 1;

int main(int argc, char **argv)
{
    std::string mainblock_path;
    std::string index_path;
    int32_t ret = largefile::TFS_SUCCESS;

    cout << "please type your blockid:" << endl;
    cin >> block_id;

    if (block_id < 1)
    {
        cerr << "Invalid blockid,(min blockid == 1),exit" << endl;
        exit(-1);
    }

    // 1.加载索引文件/////////////////////////////////////////////////////////////////
    largefile::IndexHandle *index_handle = new largefile::IndexHandle(".", block_id); // 索引文件句柄

    if (debug)
    {
        fprintf(stderr,
                "load index...\n");
    }

    ret = index_handle->load(block_id,
                             bucket_size,
                             mmap_option); ////////////////
    if (ret != largefile::TFS_SUCCESS)
    {
        fprintf(stderr,
                "load index %d failed.reason: %s\n",
                block_id, strerror(ret));

        // delete mainblock;
        delete index_handle;
        exit(-2);
    }

    // 2.写入文件到主块文件//////////////////////////////////////////////////
    std::stringstream tmp_stream;

    tmp_stream << "." << largefile::MAINBLOCK_DIR_PREFIX << block_id;

    tmp_stream >> mainblock_path;

    largefile::FileOperation *mainblock =
        new largefile::FileOperation(mainblock_path,
                                     O_RDWR | O_LARGEFILE | O_CREAT);
    // ret = mainblock->ftruncate_file(main_blocksize);
    char buffer[4096];
    memset(buffer, '6', sizeof(buffer));

    // 得到块数据的偏移量
    int32_t data_offset = index_handle->get_block_data_offset();
    // 文件编号
    uint32_t file_no = index_handle->block_info()->seq_no_;
    if ((ret = mainblock->pwrite_file(buffer, sizeof(buffer), data_offset)) != largefile::TFS_SUCCESS)
    {
        fprintf(stderr,
                "write to mainblock failed.ret:%d,reason:%s\n",
                ret,
                strerror(errno));
        mainblock->close_file();
        delete mainblock;
        delete index_handle;
        exit(-3);
    }
    // 好!成功的情况
    // 已经写入文件成功了,这个时候我们写meteinfo
    // 心静自然凉
    // 未使用数据起始的偏移也要增加(如果写成功),写失败了,偏移量不应该动的
    // 3.索引文件写入MetaInfo
    largefile::MetaInfo meta;
    // 文件id,文件偏移量,文件大小
    meta.set_file_id(file_no);
    meta.set_offset(data_offset);
    meta.set_size(sizeof(buffer));
    // 来,我们将他们写到文件哈希索引块(哈希链表),里面存的一个又一个桶
    // 接下来,我们马上要在index_handle写metaInfo(meteInfo)就是相当于哈希链表中的一个桶
    // 这里我们要定义一个api,专门写入metaInfo的

    /////????????????????????????//
    ret = index_handle->write_segment_meta(meta.get_key(), meta);
    if (ret == largefile::TFS_SUCCESS)
    {
        // 1.更新索引头部信息
        index_handle->commit_block_data_offset(sizeof(buffer));
        // 2.更新块信息
        // 实现一个api
        index_handle->update_block_info(
            largefile::C_OPER_INSERT, // 更新的类型
            sizeof(buffer)            // 更新的长度
        );

        // 内存的数据同步到磁盘
        ret = index_handle->flush();
        if (ret != largefile::TFS_SUCCESS)
        {
            fprintf(stderr,
                    "flush mainblock %d failed.file no:%u\n",
                    block_id,
                    file_no);
        }
    }
    else
    {
        fprintf(stderr,
                "write_segement_meta - mainblock %d failed.file no:%u\n",
                block_id,
                file_no);
    }

    if (ret != largefile::TFS_SUCCESS)
    {
        fprintf(stderr,
                "write to mainblock %d failed.file no:%u\n",
                block_id,
                file_no);
    }
    else
    {
        // 成功
        if (debug)
        {
            printf("write successfully! file no:%u block_id:%d\n",
                   file_no,
                   block_id);
        }
    }
    mainblock->close_file();
    delete mainblock;
    delete index_handle;

    return 0;
}
// g++ -o block_write_test block_write_test.cpp   file_op.cpp  index_handle.cpp  mmap_file.cpp  mmap_file_op.cpp