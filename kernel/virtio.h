#ifndef VIRTIO_H
#define VIRTIO_H

#include "common.h"
#include "memory.h"

#define SECTOR_SIZE       512
#define VIRTQ_ENTRY_NUM   16
#define VIRTIO_DEVICE_BLK 2
#define VIRTIO_BLK_PADDR  0x10001000
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

/**
 * @brief virtq_desc_t - represents a descriptor in the virtqueue
 * @addr: the physical address of the buffer for this descriptor
 * @len: the length of the buffer for this descriptor
 * @flags: flags for the descriptor (e.g., VIRTQ_DESC_F_NEXT, VIRTQ_DESC_F_WRITE)
 * @next: the index of the next descriptor in the chain (if VIRTQ_DESC_F_NEXT is set)
 */
typedef struct PACKED __virtq_desc {
    uint64_t addr;
    uint32_t len;
    uint16_t flags;
    uint16_t next;
} virtq_desc_t;

/**
 * @brief virtq_avail_t - represents the available ring for a virtqueue
 * @flags: flags for the available ring (e.g., VIRTQ_AVAIL_F_NO_INTERRUPT)
 * @index: the index of the next available descriptor (updated by the driver)
 * @ring: the array of available descriptor indices that the driver has filled (size is VIRTQ_ENTRY_NUM)
 */
typedef struct PACKED __virtq_avail {
    uint16_t flags;
    uint16_t index;
    uint16_t ring[VIRTQ_ENTRY_NUM];
} virtq_avail_t;

/**
 * @brief virtq_used_elem_t - represents an element in the used ring of a virtqueue
 * @id: the index of the head descriptor of the used request (matches the index in the descriptor table)
 * @len: the length of the data that was processed by the device for
 * this request (set by the device, can be used by the driver to check how much data was read/written)
 */
typedef struct PACKED __virtq_used_elem {
    uint32_t id;
    uint32_t len;
} virtq_used_elem_t;

/**
 * @brief virtq_used_t - represents the used ring for a virtqueue
 * @flags: flags for the used ring (e.g., VIRTQ_AVAIL_F_NO_INTERRUPT)
 * @index: the index of the next used element (updated by the device)
 * @ring: the array of used elements (virtq_used_elem_t) that the device has processed (size is VIRTQ_ENTRY_NUM)
 */
typedef struct PACKED __virtq_used {
    uint16_t flags;
    uint16_t index;
    virtq_used_elem_t ring[VIRTQ_ENTRY_NUM];
} virtq_used_t;

/**
 * @brief virtio_virtq_t - represents a virtqueue for the virtio device
 * @descs: the descriptor table for the virtqueue (array of virtq_desc_t)
 * @avail: the available ring for the virtqueue (virtq_avail_t)
 * @used: the used ring for the virtqueue (virtq_used_t, aligned to PAGE_SIZE)
 * @queue_index: the index of the virtqueue (used for selecting the queue in the device)
 * @used_index: a pointer to the used index in the used ring (volatile uint16_t *)
 * @last_used_index: the last used index that the driver has processed (tracking used elements have been processed)
 */
typedef struct PACKED __virtio_virtq {
    virtq_desc_t descs[VIRTQ_ENTRY_NUM];
    virtq_avail_t avail;
    virtq_used_t used ALIGNED(PAGE_SIZE);
    int queue_index;
    volatile uint16_t *used_index;
    uint16_t last_used_index;
} virtio_virtq_t;

/**
 * @brief virtio_blk_req_t - represents a block device request for the virtio-blk device
 * @type: the type of the request (e.g., read or write)
 * @reserved: reserved field (must be set to 0)
 * @sector: the sector number to read from or write to
 * @data: the data buffer for the request (512 bytes, one sector)
 * @status: the status of the request (0 for success, non-zero for error)
 */
typedef struct PACKED __virtio_blk_req {
    uint32_t type;
    uint32_t reserved;
    uint64_t sector;
    uint8_t data[512];
    uint8_t status;
} virtio_blk_req_t;

/**
 * @brief virtio_reg_read32 - reads a 32-bit value from a virtio device register at the given offset
 * @offset: the offset of the register to read from (e.g., VIRTIO_REG_MAGIC, VIRTIO_REG_VERSION)
 * @return: the 32-bit value read from the register
 */
uint32_t virtio_reg_read32(unsigned offset);
/**
 * @brief virtio_reg_read64 - reads a 64-bit value from a virtio device register at the given offset
 * @offset: the offset of the register to read from (e.g., VIRTIO_REG_GUEST_PAGE_SIZE)
 * @return: the 64-bit value read from the register
 */
uint64_t virtio_reg_read64(unsigned offset);
/**
 * @brief virtio_reg_write32 - writes a 32-bit value to a virtio device register at the given offset
 * @offset: the offset of the register to write to (e.g., VIRTIO_REG_QUEUE_SEL, VIRTIO_REG_QUEUE_NUM)
 * @value: the 32-bit value to write to the register
 */
void virtio_reg_write32(unsigned offset, uint32_t value);
/**
 * @brief virtio_reg_fetch_and_or32 - performs an atomic fetch-and-OR operation on a virtio device register
 * @offset: the offset of the register to modify (e.g., VIRTIO_REG_DEVICE_STATUS)
 * @value: the 32-bit value to OR with the current value of the register
 */
void virtio_reg_fetch_and_or32(unsigned offset, uint32_t value);

/**
 * @brief virtq_init - initializes a virtqueue for the virtio device
 * @index: the index of the virtqueue to initialize (e.g., 0 for the first queue)
 * @return: a pointer to the initialized virtio_virtq_t structure representing the virtqueue
 */
virtio_virtq_t *virtq_init(unsigned index);
/**
 * @brief virtio_init - initializes the virtio device
 */
void virtio_init(void);

/**
 * @brief virtq_kick - notifies the virtio device that a new request is available in the virtqueue
 * @vq: a pointer to the virtio_virtq_t structure representing the virtqueue
 * @desc_index: the index of the head descriptor of the new request in the descriptor table
 */
void virtq_kick(virtio_virtq_t *vq, int desc_index);

/**
 * @brief virtq_is_busy - checks if the virtio device is still processing a request in the virtqueue
 * @vq: a pointer to the virtio_virtq_t structure representing the virtqueue
 * @return: true if the device is still processing a request (i.e., the used
 */
bool virtq_is_busy(virtio_virtq_t *vq);

/**
 * @brief read_disk - performs a read operation from the virtio-blk device
 * @buf: a pointer to the buffer where the read data will be stored (must be at least SECTOR_SIZE bytes)
 * @sector: the sector number to read from
 */
void read_disk(void *buf, unsigned sector);

/**
 * @brief write_disk - performs a write operation on the virtio-blk device
 * @buf: a pointer to the buffer containing the data to write (must be at least SECTOR_SIZE bytes)
 * @sector: the sector number to write to
 */
void write_disk(void *buf, unsigned sector);

/**
 * @brief virtio_blk_get_capacity - returns the capacity of the virtio-blk device in bytes
 * @return: total disk capacity in bytes (0 before virtio_init() has run)
 */
uint64_t virtio_blk_get_capacity(void);

#endif /* VIRTIO_H */