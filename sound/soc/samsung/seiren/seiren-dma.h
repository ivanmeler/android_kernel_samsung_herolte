#ifndef __SEIREN_DMA_H
#define __SEIREN_DMA_H

/* Param */
#define DMA_PARAM_SIZE		(0x0100)

/* Mailbox for firmware */
#define DMA_PERI		(0x0000)
#define DMA_ACK			(0x0004)
#define DMA_STATE		(0x0008)
#define DMA_MODE_1		(0x0010)
#define DMA_MODE_2		(0x0014)
#define DMA_SRC_PA		(0x0018)
#define DMA_DST_PA		(0x001C)
#define DMA_PERIOD		(0x0020)
#define DMA_PERIOD_CNT		(0x0024)

enum SEIREN_DMA_MODE_1 {
	DMA_MODE_NOP = 0,
	DMA_MODE_MEM2DEV,
	DMA_MODE_DEV2MEM,
};

enum SEIREN_DMA_MODE_2 {
	DMA_MODE_ONCE = 0,
	DMA_MODE_LOOP,
};

extern void *samsung_esa_dma_get_ops(void);

#endif /* __SEIREN_DMA_H */
