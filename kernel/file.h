#ifndef FILE_H
#define FILE_H

#include "common.h"

/**
 * @brief sfs_super_block_t - represents the superblock structure for a simple file system
 * 
 */
typedef struct PACKED __sfs_super_block {
    uint32_t magic;          // Magic number to identify the file system type
    uint32_t block_size;     // Size of each block in bytes
    uint32_t fs_size;        // Total size of the file system in bytes
    uint32_t inode_count;    // Total number of inodes in the file system
    uint32_t block_count;    // Total number of blocks in the file system
} sfs_super_block_t;

typedef struct PACKED __sfs_inode {
    uint32_t size;           // Size of the file in bytes
    uint32_t block_count;    // Number of blocks allocated to the file
    uint32_t block_ptrs[12]; // Direct block pointers (up to 12 blocks)
} sfs_inode_t;

typedef struct PACKED __sfs_dir_entry {
    uint32_t inode_num;      // Inode number of the directory entry
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
sfs_super_block_t *sfs_read_super_block(void);
void sfs_mkfs(void);

#endif /* FILE_H */