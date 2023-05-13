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
int32_t block_id = 1;

const int debug = 1;

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

    // 1.创建索引文件
    largefile::IndexHandle *index_handle = new largefile::IndexHandle(".", block_id); // 索引文件句柄

    if (debug)
    {
        fprintf(stderr,
                "init index...\n");
    }

    ret = index_handle->create(block_id, bucket_size,
                               mmap_option); ////////////////
    if (ret != largefile::TFS_SUCCESS)
    {
        fprintf(stderr,
                "create index %d failed.reason: %s\n",
                block_id, strerror(errno));

        // delete mainblock;
        delete index_handle;
        exit(-3);
    }

    // 2.生成主块文件
    std::stringstream tmp_stream;

    tmp_stream << "." << largefile::MAINBLOCK_DIR_PREFIX << block_id;

    tmp_stream >> mainblock_path;

    largefile::FileOperation *mainblock =
        new largefile::FileOperation(mainblock_path,
                                     O_RDWR | O_CREAT | O_LARGEFILE);
    ret = mainblock->ftruncate_file(main_blocksize);
    if (ret != 0)
    {
        fprintf(stderr,
                "create main block %s failed.reason: %s\n",
                mainblock_path.c_str(), strerror(errno));
        delete mainblock;
        index_handle->remove(block_id);
        exit(-2);
    }
    // 其他操作...
    mainblock->close_file();
    index_handle->flush();

    delete mainblock;
    delete index_handle;

    return 0;
}
// g++ -o block_init_test block_init_test.cpp   file_op.cpp  index_handle.cpp  mmap_file.cpp  mmap_file_op.cpp