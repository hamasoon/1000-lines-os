#ifndef FILE_H
#define FILE_H

#include "common.h"

#define SFS_INODE_RATIO     KB(16)
#define SFS_MAGIC           0xB16B00B5B16A5555

/* On-disk SFS layout:
 *
 *   +----------------------------+
 *   |       Super Block (4KB)    |
 *   +----------------------------+
 *   |       Inode Table          |
 *   +----------------------------+
 *   |       Block Bitmap         |
 *   +----------------------------+
 *   |       Data Blocks ...      |
 *   +----------------------------+
 */

typedef struct PACKED __sfs_super_block {
    uint64_t magic;
    uint64_t block_size;
    uint64_t fs_size;
    uint64_t inode_count;
    uint64_t data_block_count;
    uint64_t inode_table_start;
    uint64_t inode_table_blocks;
    uint64_t bitmap_start;
    uint64_t bitmap_blocks;
    uint64_t data_start;
} sfs_super_block_t;

typedef struct PACKED __sfs_inode {
    uint64_t size;
    uint64_t block_count;
    uint64_t block_ptrs[12];
} sfs_inode_t;

typedef struct PACKED __sfs_dir_entry {
    uint64_t inode_num;
    char name[28];
} sfs_dir_entry_t;

typedef struct PACKED __sfs_inode_table {
} sfs_inode_table_t;

typedef struct PACKED __sfs_block_bitmap {
} sfs_block_bitmap_t;

void sfs_init(void);
void sfs_mkfs(void);

#endif /* FILE_H */
