#include "virtio.h"

static virtio_virtq_t *blk_request_vq;
static virtio_blk_req_t *blk_req;
static paddr_t blk_req_paddr;
static uint64_t blk_capacity;

uint32_t virtio_reg_read32(unsigned offset) {
    return *((volatile uint32_t *) (VIRTIO_BLK_PADDR + offset));
}

uint64_t virtio_reg_read64(unsigned offset) {
    return *((volatile uint64_t *) (VIRTIO_BLK_PADDR + offset));
}

void virtio_reg_write32(unsigned offset, uint32_t value) {
    *((volatile uint32_t *) (VIRTIO_BLK_PADDR + offset)) = value;
}

void virtio_reg_fetch_and_or32(unsigned offset, uint32_t value) {
    virtio_reg_write32(offset, virtio_reg_read32(offset) | value);
}

virtio_virtq_t *virtq_init(unsigned index) {
    paddr_t virtq_paddr = alloc_pages(ALIGN_UP(sizeof(virtio_virtq_t), PAGE_SIZE) / PAGE_SIZE);
    virtio_virtq_t *vq = (virtio_virtq_t *) virtq_paddr;
    vq->queue_index = index;
    vq->used_index = (volatile uint16_t *) &vq->used.index;
    // select the queue by writing its index to the QUEUE_SEL register
    virtio_reg_write32(VIRTIO_REG_QUEUE_SEL, index);
    // set the maximum number of entries in the queue (must be <= VIRTQ_ENTRY_NUM)
    virtio_reg_write32(VIRTIO_REG_QUEUE_NUM, VIRTQ_ENTRY_NUM);
    // write the physical address of the virtqueue to the QUEUE_PFN register (shifted right by 12 bits)
    virtio_reg_write32(VIRTIO_REG_QUEUE_PFN, virtq_paddr / PAGE_SIZE);
    return vq;
}

void virtio_init(void) {
    if (virtio_reg_read32(VIRTIO_REG_MAGIC) != 0x74726976)
        PANIC("virtio: invalid magic value");
    if (virtio_reg_read32(VIRTIO_REG_VERSION) != 1)
        PANIC("virtio: invalid version");
    if (virtio_reg_read32(VIRTIO_REG_DEVICE_ID) != VIRTIO_DEVICE_BLK)
        PANIC("virtio: invalid device id");

    // Initialize device: reset the registers by writing 0 to the DEVICE_STATUS register
    virtio_reg_write32(VIRTIO_REG_DEVICE_STATUS, 0);
    // 2. ACKNOWLEDGE 상태 비트 설정: 장치를 발견함
    virtio_reg_fetch_and_or32(VIRTIO_REG_DEVICE_STATUS, VIRTIO_STATUS_ACK);
    // 3. DRIVER 상태 비트 설정: 장치 사용 방법을 알고 있음
    virtio_reg_fetch_and_or32(VIRTIO_REG_DEVICE_STATUS, VIRTIO_STATUS_DRIVER);
    // 페이지 크기 설정: 4KB 페이지 사용. PFN (페이지 프레임 번호) 계산에 사용됨
    virtio_reg_write32(VIRTIO_REG_PAGE_SIZE, PAGE_SIZE);
    // 디스크 읽기/쓰기 요청용 큐 초기화
    blk_request_vq = virtq_init(0);
    // 6. DRIVER_OK 상태 비트 설정: 이제 장치를 사용할 수 있음
    virtio_reg_write32(VIRTIO_REG_DEVICE_STATUS, VIRTIO_STATUS_DRIVER_OK);

    // 디스크 용량을 가져옵니다.
    blk_capacity = virtio_reg_read64(VIRTIO_REG_DEVICE_CONFIG + 0) * SECTOR_SIZE;
    printf("virtio-blk: capacity is %d bytes\n", (int)blk_capacity);

    // 장치에 요청(request)을 저장할 영역을 할당합니다.
    blk_req_paddr = alloc_pages(ALIGN_UP(sizeof(*blk_req), PAGE_SIZE) / PAGE_SIZE);
    blk_req = (virtio_blk_req_t *) blk_req_paddr;
}

// desc_index는 새로운 요청의 디스크립터 체인의 헤드 디스크립터 인덱스입니다.
// 장치에 새로운 요청이 있음을 알립니다.
void virtq_kick(virtio_virtq_t *vq, int desc_index) {
    vq->avail.ring[vq->avail.index % VIRTQ_ENTRY_NUM] = desc_index;
    vq->avail.index++;
    __sync_synchronize();
    virtio_reg_write32(VIRTIO_REG_QUEUE_NOTIFY, vq->queue_index);
    vq->last_used_index++;
}

bool virtq_is_busy(virtio_virtq_t *vq) {
    return vq->last_used_index != *vq->used_index;
}

void read_write_disk(void *buf, unsigned sector, int is_write) {
    if (sector >= blk_capacity / SECTOR_SIZE) {
        printf("virtio: tried to read/write sector=%d, but capacity is %d\n",
              sector, blk_capacity / SECTOR_SIZE);
        return;
    }

    // virtio-blk 사양에 따라 요청을 구성합니다.
    blk_req->sector = sector;
    blk_req->type = is_write ? VIRTIO_BLK_T_OUT : VIRTIO_BLK_T_IN;
    if (is_write)
        memcpy(blk_req->data, buf, SECTOR_SIZE);

    // virtqueue 디스크립터를 구성합니다 (3개의 디스크립터 사용).
    virtio_virtq_t *vq = blk_request_vq;
    vq->descs[0].addr = blk_req_paddr;
    vq->descs[0].len = sizeof(uint32_t) * 2 + sizeof(uint64_t);
    vq->descs[0].flags = VIRTQ_DESC_F_NEXT;
    vq->descs[0].next = 1;

    vq->descs[1].addr = blk_req_paddr + offsetof(virtio_blk_req_t, data);
    vq->descs[1].len = SECTOR_SIZE;
    vq->descs[1].flags = VIRTQ_DESC_F_NEXT | (is_write ? 0 : VIRTQ_DESC_F_WRITE);
    vq->descs[1].next = 2;

    vq->descs[2].addr = blk_req_paddr + offsetof(virtio_blk_req_t, status);
    vq->descs[2].len = sizeof(uint8_t);
    vq->descs[2].flags = VIRTQ_DESC_F_WRITE;

    // 장치에 새로운 요청이 있음을 알림.
    virtq_kick(vq, 0);

    // 장치가 요청 처리를 마칠 때까지 대기(바쁜 대기; busy-wait).
    while (virtq_is_busy(vq))
        ;

    // virtio-blk: 0이 아닌 값이 반환되면 에러입니다.
    if (blk_req->status != 0) {
        printf("virtio: warn: failed to read/write sector=%d status=%d\n",
               sector, blk_req->status);
        return;
    }

    // 읽기 작업의 경우, 데이터를 버퍼에 복사합니다.
    if (!is_write)
        memcpy(buf, blk_req->data, SECTOR_SIZE);
}