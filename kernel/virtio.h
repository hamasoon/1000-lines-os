#ifndef VIRTIO_H
#define VIRTIO_H

#include "common.h"
#include "memory.h"

#define SECTOR_SIZE       512
#define VIRTQ_ENTRY_NUM   16
#define VIRTIO_DEVICE_BLK 2

/* UL suffix: on lp64 this becomes 64-bit unsigned long, so pointer casts
 * (VIRTIO_BLK_PADDR + offset) do not trip -Wint-to-pointer-cast. */
#define VIRTIO_BLK_PADDR  0x10001000UL

#define VIRTIO_REG_MAGIC         0x00
#define VIRTIO_REG_VERSION       0x04
#define VIRTIO_REG_DEVICE_ID     0x08
#define VIRTIO_REG_PAGE_SIZE     0x28
#define VIRTIO_REG_QUEUE_SEL     0x30
#define VIRTIO_REG_QUEUE_NUM_MAX 0x34
#define VIRTIO_REG_QUEUE_NUM     0x38
#define VIRTIO_REG_QUEUE_PFN     0x40
#define VIRTIO_REG_QUEUE_READY   0x44
#define VIRTIO_REG_QUEUE_NOTIFY  0x50
#define VIRTIO_REG_DEVICE_STATUS 0x70
#define VIRTIO_REG_DEVICE_CONFIG 0x100

#define VIRTIO_STATUS_ACK       1
#define VIRTIO_STATUS_DRIVER    2
#define VIRTIO_STATUS_DRIVER_OK 4

#define VIRTQ_DESC_F_NEXT          1
#define VIRTQ_DESC_F_WRITE         2
#define VIRTQ_AVAIL_F_NO_INTERRUPT 1

#define VIRTIO_BLK_T_IN  0
#define VIRTIO_BLK_T_OUT 1

typedef struct PACKED __virtq_desc {
    uint64_t addr;
    uint32_t len;
    uint16_t flags;
    uint16_t next;
} virtq_desc_t;

typedef struct PACKED __virtq_avail {
    uint16_t flags;
    uint16_t index;
    uint16_t ring[VIRTQ_ENTRY_NUM];
} virtq_avail_t;

typedef struct PACKED __virtq_used_elem {
    uint32_t id;
    uint32_t len;
} virtq_used_elem_t;

typedef struct PACKED __virtq_used {
    uint16_t flags;
    uint16_t index;
    virtq_used_elem_t ring[VIRTQ_ENTRY_NUM];
} virtq_used_t;

typedef struct PACKED __virtio_virtq {
    virtq_desc_t descs[VIRTQ_ENTRY_NUM];
    virtq_avail_t avail;
    virtq_used_t used ALIGNED(PAGE_SIZE);
    int queue_index;
    volatile uint16_t *used_index;
    uint16_t last_used_index;
} virtio_virtq_t;

typedef struct PACKED __virtio_blk_req {
    uint32_t type;
    uint32_t reserved;
    uint64_t sector;
    uint8_t data[512];
    uint8_t status;
} virtio_blk_req_t;

uint32_t virtio_reg_read32(unsigned offset);
uint64_t virtio_reg_read64(unsigned offset);
void virtio_reg_write32(unsigned offset, uint32_t value);
void virtio_reg_fetch_and_or32(unsigned offset, uint32_t value);

virtio_virtq_t *virtq_init(unsigned index);
void virtio_init(void);
void virtq_kick(virtio_virtq_t *vq, int desc_index);
bool virtq_is_busy(virtio_virtq_t *vq);

void read_disk(void *buf, unsigned sector);
void write_disk(void *buf, unsigned sector);
uint64_t virtio_blk_get_capacity(void);

#endif /* VIRTIO_H */
