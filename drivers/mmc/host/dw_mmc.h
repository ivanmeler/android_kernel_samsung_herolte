/*
 * Synopsys DesignWare Multimedia Card Interface driver
 *  (Based on NXP driver for lpc 31xx)
 *
 * Copyright (C) 2009 NXP Semiconductors
 * Copyright (C) 2009, 2010 Imagination Technologies Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef _DW_MMC_H_
#define _DW_MMC_H_

#define DW_MMC_240A		0x240a
#define DW_MMC_260A		0x260a

#define SDMMC_CTRL		0x000
#define SDMMC_PWREN		0x004
#define SDMMC_CLKDIV		0x008
#define SDMMC_CLKSRC		0x00c
#define SDMMC_CLKENA		0x010
#define SDMMC_TMOUT		0x014
#define SDMMC_CTYPE		0x018
#define SDMMC_BLKSIZ		0x01c
#define SDMMC_BYTCNT		0x020
#define SDMMC_INTMASK		0x024
#define SDMMC_CMDARG		0x028
#define SDMMC_CMD		0x02c
#define SDMMC_RESP0		0x030
#define SDMMC_RESP1		0x034
#define SDMMC_RESP2		0x038
#define SDMMC_RESP3		0x03c
#define SDMMC_MINTSTS		0x040
#define SDMMC_RINTSTS		0x044
#define SDMMC_STATUS		0x048
#define SDMMC_FIFOTH		0x04c
#define SDMMC_CDETECT		0x050
#define SDMMC_WRTPRT		0x054
#define SDMMC_GPIO		0x058
#define SDMMC_TCBCNT		0x05c
#define SDMMC_TBBCNT		0x060
#define SDMMC_DEBNCE		0x064
#define SDMMC_USRID		0x068
#define SDMMC_VERID		0x06c
#define SDMMC_HCON		0x070
#define SDMMC_UHS_REG		0x074
#define SDMMC_BMOD		0x080
#define SDMMC_PLDMND		0x084
#define SDMMC_DBADDR		0x088
#define SDMMC_IDSTS		0x08c
#define SDMMC_IDINTEN		0x090
#define SDMMC_DSCADDR		0x094
#define SDMMC_BUFADDR		0x098
#define SDMMC_RESP_TAT		0x0AC
#define SDMMC_CDTHRCTL		0x100
#define SDMMC_EMMC_DDR_REG	0x10C
#define SDMMC_DATA(x)		(x)

/*
 * Data offset is difference according to Version
 * Lower than 2.40a : data register offest is 0x100
 */
#define DATA_OFFSET		0x100
#define DATA_240A_OFFSET	0x200

/* shift bit field */
#define _SBF(f, v)		((v) << (f))

/* Control register defines */
#define SDMMC_CTRL_USE_IDMAC		BIT(25)
#define SDMMC_CTRL_CEATA_INT_EN		BIT(11)
#define SDMMC_CTRL_SEND_AS_CCSD		BIT(10)
#define SDMMC_CTRL_SEND_CCSD		BIT(9)
#define SDMMC_CTRL_ABRT_READ_DATA	BIT(8)
#define SDMMC_CTRL_SEND_IRQ_RESP	BIT(7)
#define SDMMC_CTRL_READ_WAIT		BIT(6)
#define SDMMC_CTRL_DMA_ENABLE		BIT(5)
#define SDMMC_CTRL_INT_ENABLE		BIT(4)
#define SDMMC_CTRL_DMA_RESET		BIT(2)
#define SDMMC_CTRL_FIFO_RESET		BIT(1)
#define SDMMC_CTRL_RESET		BIT(0)
/* Clock Enable register defines */
#define SDMMC_CLKEN_LOW_PWR		BIT(16)
#define SDMMC_CLKEN_ENABLE		BIT(0)
/* time-out register defines */
#define SDMMC_TMOUT_DATA(n)		_SBF(8, (n))
#define SDMMC_TMOUT_DATA_MSK		0xFFFFFF00
#define SDMMC_TMOUT_RESP(n)		((n) & 0xFF)
#define SDMMC_TMOUT_RESP_MSK		0xFF
/* card-type register defines */
#define SDMMC_CTYPE_8BIT		BIT(16)
#define SDMMC_CTYPE_4BIT		BIT(0)
#define SDMMC_CTYPE_1BIT		0
/* Interrupt status & mask register defines */
#define SDMMC_INT_SDIO(n)		BIT(16 + (n))
#define SDMMC_INT_EBE			BIT(15)
#define SDMMC_INT_ACD			BIT(14)
#define SDMMC_INT_SBE			BIT(13)
#define SDMMC_INT_HLE			BIT(12)
#define SDMMC_INT_FRUN			BIT(11)
#define SDMMC_INT_HTO			BIT(10)
#define SDMMC_INT_VOLT_SWITCH		BIT(10) /* overloads bit 10! */
#define SDMMC_INT_DRTO			BIT(9)
#define SDMMC_INT_RTO			BIT(8)
#define SDMMC_INT_DCRC			BIT(7)
#define SDMMC_INT_RCRC			BIT(6)
#define SDMMC_INT_RXDR			BIT(5)
#define SDMMC_INT_TXDR			BIT(4)
#define SDMMC_INT_DATA_OVER		BIT(3)
#define SDMMC_INT_CMD_DONE		BIT(2)
#define SDMMC_INT_RESP_ERR		BIT(1)
#define SDMMC_INT_CD			BIT(0)
#define SDMMC_INT_ERROR			0xbfc2
/* Command register defines */
#define SDMMC_CMD_START			BIT(31)
#define SDMMC_CMD_USE_HOLD_REG	BIT(29)
#define SDMMC_CMD_VOLT_SWITCH		BIT(28)
#define SDMMC_CMD_CCS_EXP		BIT(23)
#define SDMMC_CMD_CEATA_RD		BIT(22)
#define SDMMC_CMD_UPD_CLK		BIT(21)
#define SDMMC_CMD_INIT			BIT(15)
#define SDMMC_CMD_STOP			BIT(14)
#define SDMMC_CMD_PRV_DAT_WAIT		BIT(13)
#define SDMMC_CMD_SEND_STOP		BIT(12)
#define SDMMC_CMD_STRM_MODE		BIT(11)
#define SDMMC_CMD_DAT_WR		BIT(10)
#define SDMMC_CMD_DAT_EXP		BIT(9)
#define SDMMC_CMD_RESP_CRC		BIT(8)
#define SDMMC_CMD_RESP_LONG		BIT(7)
#define SDMMC_CMD_RESP_EXP		BIT(6)
#define SDMMC_CMD_INDX(n)		((n) & 0x1F)
/* Status register defines */
#define SDMMC_GET_FCNT(x)		(((x)>>17) & 0x1FFF)
#define SDMMC_STATUS_DMA_REQ		BIT(31)
#define SDMMC_STATUS_BUSY		BIT(9)
/* FIFOTH register defines */
#define SDMMC_SET_FIFOTH(m, r, t)	(((m) & 0x7) << 28 | \
					 ((r) & 0xFFF) << 16 | \
					 ((t) & 0xFFF))

#define SDMMC_FIFOTH_DMA_MULTI_TRANS_SIZE	28
#define SDMMC_FIFOTH_RX_WMARK		16
/* Internal DMAC interrupt defines */
#define SDMMC_IDMAC_INT_AI		BIT(9)
#define SDMMC_IDMAC_INT_NI		BIT(8)
#define SDMMC_IDMAC_INT_CES		BIT(5)
#define SDMMC_IDMAC_INT_DU		BIT(4)
#define SDMMC_IDMAC_INT_FBE		BIT(2)
#define SDMMC_IDMAC_INT_RI		BIT(1)
#define SDMMC_IDMAC_INT_TI		BIT(0)
/* Internal DMAC bus mode bits */
#define SDMMC_IDMAC_ENABLE		BIT(7)
#define SDMMC_IDMAC_FB			BIT(1)
#define SDMMC_IDMAC_SWRESET		BIT(0)
/* Version ID register define */
#define SDMMC_GET_VERID(x)		((x) & 0xFFFF)
/* Card read threshold */
#define SDMMC_SET_RD_THLD(v, x)		(((v) & 0x1FFF) << 16 | (x))
#define SDMMC_UHS_18V			BIT(0)
/* All ctrl reset bits */
#define SDMMC_CTRL_ALL_RESET_FLAGS \
	(SDMMC_CTRL_RESET | SDMMC_CTRL_FIFO_RESET | SDMMC_CTRL_DMA_RESET)

/* Register access macros */
#define mci_readl(dev, reg)			\
	__raw_readl((dev)->regs + SDMMC_##reg)
#define mci_writel(dev, reg, value)			\
	__raw_writel((value), (dev)->regs + SDMMC_##reg)

/* timeout */
#define dw_mci_set_timeout(host, value)	mci_writel(host, TMOUT, value)

/* 16-bit FIFO access macros */
#define mci_readw(dev, reg)			\
	__raw_readw((dev)->regs + SDMMC_##reg)
#define mci_writew(dev, reg, value)			\
	__raw_writew((value), (dev)->regs + SDMMC_##reg)

/* 64-bit FIFO access macros */
#ifdef readq
#ifdef CONFIG_MMC_DW_FORCE_32BIT_SFR_RW
#define mci_readq(dev, reg) ({\
		u64 __ret = 0;\
		u32* ptr = (u32*)&__ret;\
		*ptr++ = __raw_readl((dev)->regs + SDMMC_##reg);\
		*ptr = __raw_readl((dev)->regs + SDMMC_##reg + 0x4);\
		__ret;\
	})
#define mci_writeq(dev, reg, value) ({\
		u32 *ptr = (u32*)&(value);\
		__raw_writel(*ptr++, (dev)->regs + SDMMC_##reg);\
		__raw_writel(*ptr, (dev)->regs + SDMMC_##reg + 0x4);\
	})
#else
#define mci_readq(dev, reg)			\
	__raw_readq((dev)->regs + SDMMC_##reg)
#define mci_writeq(dev, reg, value)			\
	__raw_writeq((value), (dev)->regs + SDMMC_##reg)
#endif /* CONFIG_MMC_DW_FORCE_32BIT_SFR_RW */
#else
/*
 * Dummy readq implementation for architectures that don't define it.
 *
 * We would assume that none of these architectures would configure
 * the IP block with a 64bit FIFO width, so this code will never be
 * executed on those machines. Defining these macros here keeps the
 * rest of the code free from ifdefs.
 */
#define mci_readq(dev, reg)			\
	(*(volatile u64 __force *)((dev)->regs + SDMMC_##reg))
#define mci_writeq(dev, reg, value)			\
	(*(volatile u64 __force *)((dev)->regs + SDMMC_##reg) = (value))
#endif

/*
 * platform-dependent miscellaneous control
 *
 * Input arguments for platform-dependent control may be different
 * for each one, respectively. If we would add functions like them
 * whenever we need to do that, this common header file(dw_mmc.h)
 * will be modified so frequently.
 * The following enumeration type is to minimize an amount of changes
 * of common files.
 */

enum dw_mci_misc_control {
	CTRL_RESTORE_CLKSEL = 0,
	CTRL_REQUEST_EXT_IRQ,
	CTRL_CHECK_CD,
};

#define SDMMC_DATA_TMOUT_SHIFT		11
#define SDMMC_RESP_TMOUT		0xFF
#define SDMMC_DATA_TMOUT_CRT		8
#define SDMMC_DATA_TMOUT_EXT		0x1
#define SDMMC_DATA_TMOUT_EXT_SHIFT	8
#define SDMMC_DATA_TMOUT_MAX_CNT	0x1FFFFF
#define SDMMC_DATA_TMOUT_MAX_EXT_CNT	0xFFFFFF
#define SDMMC_HTO_TMOUT_SHIFT		8

extern u32 dw_mci_calc_timeout(struct dw_mci *host);

extern int dw_mci_probe(struct dw_mci *host);
extern void dw_mci_remove(struct dw_mci *host);
#ifdef CONFIG_PM_SLEEP
extern int dw_mci_suspend(struct dw_mci *host);
extern int dw_mci_resume(struct dw_mci *host);
#endif

/**
 * struct dw_mci_slot - MMC slot state
 * @mmc: The mmc_host representing this slot.
 * @host: The MMC controller this slot is using.
 * @quirks: Slot-level quirks (DW_MCI_SLOT_QUIRK_XXX)
 * @ctype: Card type for this slot.
 * @mrq: mmc_request currently being processed or waiting to be
 *	processed, or NULL when the slot is idle.
 * @queue_node: List node for placing this node in the @queue list of
 *	&struct dw_mci.
 * @clock: Clock rate configured by set_ios(). Protected by host->lock.
 * @__clk_old: The last updated clock with reflecting clock divider.
 *	Keeping track of this helps us to avoid spamming the console
 *	with CONFIG_MMC_CLKGATE.
 * @flags: Random state bits associated with the slot.
 * @id: Number of this slot.
 * @last_detect_state: Most recently observed card detect state.
 */
struct dw_mci_slot {
	struct mmc_host		*mmc;
	struct dw_mci		*host;

	int			quirks;

	u32			ctype;

	struct mmc_request	*mrq;
	struct list_head	queue_node;

	unsigned int		clock;
	unsigned int		__clk_old;

	unsigned long		flags;
#define DW_MMC_CARD_PRESENT	0
#define DW_MMC_CARD_NEED_INIT	1
	int			id;
	int			last_detect_state;
};

/**
 * struct dw_mci_debug_data - DwMMC debugging infomation
 * @host_count: a number of all hosts
 * @info_count: a number of set of debugging information
 * @info_index: index of debugging information for each host
 * @host: pointer of each dw_mci structure
 * @debug_info: debugging information structure
 */

struct dw_mci_cmd_log {
	u64	send_time;
	u64	done_time;
	u8	cmd;
	u32	arg;
	u8	data_size;
	/* no data CMD = CD, data CMD = DTO */
	/*
	 * 0b1000 0000	: new_cmd with without send_cmd
	 * 0b0000 1000	: error occurs
	 * 0b0000 0100	: data_done : DTO(Data Transfer Over)
	 * 0b0000 0010	: resp : CD(Command Done)
	 * 0b0000 0001	: send_cmd : set 1 only start_command
	 */
	u8	seq_status;	/* 0bxxxx xxxx : error data_done resp send */
#define DW_MCI_FLAG_SEND_CMD	BIT(0)
#define DW_MCI_FLAG_CD		BIT(1)
#define DW_MCI_FLAG_DTO		BIT(2)
#define DW_MCI_FLAG_ERROR	BIT(3)
#define DW_MCI_FLAG_NEW_CMD_ERR	BIT(7)

	u16	rint_sts;	/* RINTSTS value in case of error */
	u8	status_count;	/* TBD : It can be changed */
};

enum dw_mci_req_log_state {
	STATE_REQ_START = 0,
	STATE_REQ_CMD_PROCESS,
	STATE_REQ_DATA_PROCESS,
	STATE_REQ_END,
};

struct dw_mci_req_log {
	u64				timestamp;
	u32				info0;
	u32				info1;
	u32				info2;
	u32				info3;
	u32				pending_events;
	u32				completed_events;
	enum dw_mci_state		state;
	enum dw_mci_state		state_cmd;
	enum dw_mci_state		state_dat;
	enum dw_mci_req_log_state	log_state;
};

#define DWMCI_LOG_MAX		0x80
#define DWMCI_REQ_LOG_MAX	0x40
struct dw_mci_debug_info {
	struct dw_mci_cmd_log		cmd_log[DWMCI_LOG_MAX];
	atomic_t			cmd_log_count;
	struct dw_mci_req_log		req_log[DWMCI_REQ_LOG_MAX];
	atomic_t			req_log_count;
	unsigned char			en_logging;
#define DW_MCI_DEBUG_ON_CMD	BIT(0)
#define DW_MCI_DEBUG_ON_REQ	BIT(1)
};

#define DWMCI_DBG_NUM_HOST	3

#define DWMCI_DBG_NUM_INFO	3			/* configurable */
#define DWMCI_DBG_MASK_INFO	(BIT(0) | BIT(1) | BIT(2))	/* configurable */
#define DWMCI_DBG_BIT_HOST(x)	BIT(x)

struct dw_mci_debug_data {
	unsigned char			host_count;
	unsigned char			info_count;
	unsigned char			info_index[DWMCI_DBG_NUM_HOST];
	struct dw_mci			*host[DWMCI_DBG_NUM_HOST];
	struct dw_mci_debug_info	debug_info[DWMCI_DBG_NUM_INFO];
};

struct dw_mci_tuning_data {
	const u8 *blk_pattern;
	unsigned int blksz;
};

/**
 * dw_mci driver data - dw-mshc implementation specific driver data.
 * @caps: mmc subsystem specified capabilities of the controller(s).
 * @init: early implementation specific initialization.
 * @setup_clock: implementation specific clock configuration.
 * @prepare_command: handle CMD register extensions.
 * @set_ios: handle bus specific extensions.
 * @parse_dt: parse implementation specific device tree properties.
 * @execute_tuning: implementation specific tuning procedure.
 * @cfg_smu: to configure security management unit
 *
 * Provide controller implementation specific extensions. The usage of this
 * data structure is fully optional and usage of each member in this structure
 * is optional as well.
 */
struct dw_mci_drv_data {
	unsigned long	*caps;
	int		(*init)(struct dw_mci *host);
	int		(*setup_clock)(struct dw_mci *host);
	void		(*prepare_command)(struct dw_mci *host, u32 *cmdr);
	void		(*set_ios)(struct dw_mci *host, struct mmc_ios *ios);
	int		(*parse_dt)(struct dw_mci *host);
	int		(*execute_tuning)(struct dw_mci_slot *slot, u32 opcode,
					struct dw_mci_tuning_data *tuning_data);
	void            (*cfg_smu)(struct dw_mci *host);
	int             (*misc_control)(struct dw_mci *host,
				enum dw_mci_misc_control control, void *priv);
};

struct dw_mci_sfe_ram_dump {
	u32			contrl;
	u32			pwren;
	u32			clkdiv;
	u32			clkena;
	u32			clksrc;
	u32			tmout;
	u32			ctype;
	u32			blksiz;
	u32			bytcnt;
	u32			intmask;
	u32			cmdarg;
	u32			cmd;
	u32		       	mintsts;
	u32			rintsts;
	u32			status;
	u32			fifoth;
	u32			tcbcnt;
	u32			tbbcnt;
	u32			debnce;
	u32			uhs_reg;
	u32			bmod;
	u32			pldmnd;
	u32			dbaddrl;
	u32			dbaddru;
	u32			dscaddrl;
	u32			dscaddru;
	u32			bufaddru;
	u32			dbaddr;
	u32			dscaddr;
	u32			bufaddr;
	u32			clksel;
	u32			idsts64;
	u32			idinten64;
	u32			force_clk_stop;
	u32			cdthrctl;
	u32			ddr200_rdqs_en;
	u32			ddr200_acync_fifo_ctrl;
	u32			ddr200_dline_ctrl;
	u32			fmp_emmcp_base;
	u32			mpsecurity;
	u32			mpstat;
	u32			mpsbegin;
	u32			mpsend;
	u32			mpsctrl;
	u32			cmd_status;
	u32			data_status;
	u32			pending_events;
	u32			completed_events;
	u32			host_state;
	u32			cmd_index;
	u32			fifo_count;
	u32			data_busy;
	u32			data_3_state;
	u32			fifo_tx_watermark;
	u32			fifo_rx_watermark;
};
#endif /* _DW_MMC_H_ */
