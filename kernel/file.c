#include "file.h"
#include "virtio.h"

static sfs_super_block_t super_block;

void sfs_init(void) {
    char buf[SECTOR_SIZE];
    read_disk(buf, 0);
    memcpy(&super_block, buf, sizeof(super_block));

    if (super_block.magic != SFS_MAGIC) {
        sfs_mkfs();
    }
}

void sfs_mkfs(void) {
    uint64_t disk_size = virtio_blk_get_capacity();
    uint64_t block_size = KB(4);
    uint64_t total_blocks = disk_size / block_size;

    if (total_blocks < 4) PANIC("sfs: disk too small");

    uint64_t inodes_per_block = block_size / sizeof(sfs_inode_t);
    uint64_t target_inodes = disk_size / SFS_INODE_RATIO;
    uint64_t inode_table_blocks = CEIL_DIV(target_inodes, inodes_per_block);
    /* Round up to fill the inode table completely. */
    uint64_t inode_count = inode_table_blocks * inodes_per_block;

    uint64_t bitmap_blocks = CEIL_DIV(total_blocks, block_size * 8);

    uint64_t reserved = 1 + inode_table_blocks + bitmap_blocks;
    if (total_blocks <= reserved) PANIC("sfs: disk too small for metadata");
    uint64_t data_block_count = total_blocks - reserved;

    super_block.magic = SFS_MAGIC;
    super_block.block_size = block_size;
    super_block.fs_size = total_blocks * block_size;
    super_block.inode_count = inode_count;
    super_block.data_block_count = data_block_count;
    super_block.inode_table_start = 1;
    super_block.inode_table_blocks = inode_table_blocks;
    super_block.bitmap_start = 1 + inode_table_blocks;
    super_block.bitmap_blocks = bitmap_blocks;
    super_block.data_start = 1 + inode_table_blocks + bitmap_blocks;

    char buf[SECTOR_SIZE];
    memset(buf, 0, SECTOR_SIZE);
    memcpy(buf, &super_block, sizeof(super_block));
    write_disk(buf, 0);
}
