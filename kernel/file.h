#ifndef FILE_H
#define FILE_H

#include "common.h"

#define SFS_INODE_RATIO     KB(16)
#define SFS_MAGIC           0xB16B00B5B16A5555

/* Diagram of SFS structure:
 *
 * +----------------------------+
 * |       Super Block (4KB)    |
 * +----------------------------+
 * |       Inode Table (4KB)    |
 * +----------------------------+
 * |      Block Bitmap (4KB)    |
 * +----------------------------+
 * |       Data Blocks ...      |
 * +----------------------------+
 */

/**
 * @brief sfs_super_block_t - represents the superblock structure for a simple file system
 * @magic: a magic number to identify the file system type (e.g., 0xB16B00B5B16A5555)
 * @block_size: the size of each block in bytes (e.g., 4096 for 4KB blocks)
 * @fs_size: the total size of the file system in bytes
 * @inode_count: the total number of inodes in the file system
 * @data_block_count: the total number of data blocks available for file storage
 * @inode_table_start: the starting block number of the inode table
 * @inode_table_blocks: the number of blocks allocated for the inode table
 * @bitmap_start: the starting block number of the block bitmap
 * @bitmap_blocks: the number of blocks allocated for the block bitmap
 * @data_start: the starting block number of the data blocks
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
    uint64_t size;           // Size of the file in bytes
    uint64_t block_count;    // Number of blocks allocated to the file
    uint64_t block_ptrs[12]; // Direct block pointers (up to 12 blocks)
} sfs_inode_t;

typedef struct PACKED __sfs_dir_entry {
    uint64_t inode_num;      // Inode number of the directory entry
    char name[28];           // Name of the file or directory (null-terminated string)
} sfs_dir_entry_t;

/**
 * @brief sfs_inode_table_t - represents the inode table structure for a simple file system
 * 
 */
typedef struct PACKED __sfs_inode_table {
    
} sfs_inode_table_t;

typedef struct PACKED __sfs_block_bitmap {
    
} sfs_block_bitmap_t;

void sfs_init(void);
void sfs_mkfs(void);

#endif /* FILE_H */