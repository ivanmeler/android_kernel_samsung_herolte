/* Copyright (c) 2012, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/err.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/clk.h>
#include <linux/bitmap.h>
#include <linux/coresight.h>
#include <linux/coresight-stm.h>
#include <asm/unaligned.h>

#include "of_coresight.h"
#include "coresight-priv.h"

#define stm_writel(drvdata, val, off)	__raw_writel((val), drvdata->base + off)
#define stm_readl(drvdata, off)		__raw_readl(drvdata->base + off)

#define txa_writel(drvdata, val, off)		__raw_writel((val), drvdata->txa.base + off)
#define txa_readl(drvdata, off)		__raw_readl(drvdata->txa.base + off)

#define STM_LOCK(drvdata)						\
do {									\
	mb();								\
	stm_writel(drvdata, 0x0, CORESIGHT_LAR);			\
} while (0)
#define STM_UNLOCK(drvdata)						\
do {									\
	stm_writel(drvdata, CORESIGHT_UNLOCK, CORESIGHT_LAR);		\
	mb();								\
} while (0)

#define STMDMASTARTR			(0xC04)
#define STMDMASTOPR			(0xC08)
#define STMDMASTATR			(0xC0C)
#define STMDMACTLR			(0xC10)
#define STMDMAIDR			(0xCFC)
#define STMHEER				(0xD00)
#define STMHETER			(0xD20)
#define STMHEMCR			(0xD64)
#define STMHEMASTR			(0xDF4)
#define STMHEFEAT1R			(0xDF8)
#define STMHEIDR			(0xDFC)
#define STMSPER				(0xE00)
#define STMSPTER			(0xE20)
#define STMSPSCR			(0xE60)
#define STMSPMSCR			(0xE64)
#define STMSPOVERRIDER			(0xE68)
#define STMSPMOVERRIDER			(0xE6C)
#define STMSPTRIGCSR			(0xE70)
#define STMTCSR				(0xE80)
#define STMTSSTIMR			(0xE84)
#define STMTSFREQR			(0xE8C)
#define STMSYNCR			(0xE90)
#define STMAUXCR			(0xE94)
#define STMSPFEAT1R			(0xEA0)
#define STMSPFEAT2R			(0xEA4)
#define STMSPFEAT3R			(0xEA8)
#define STMITTRIGGER			(0xEE8)
#define STMITATBDATA0			(0xEEC)
#define STMITATBCTR2			(0xEF0)
#define STMITATBID			(0xEF4)
#define STMITATBCTR0			(0xEF8)

#define NR_STM_CHANNEL			(32)
#define BYTES_PER_CHANNEL		(256)
#define STM_TRACE_BUF_SIZE		(4096)
#define STM_USERSPACE_HEADER_SIZE	(8)
#define STM_USERSPACE_MAGIC1_VAL	(0xf0)
#define STM_USERSPACE_MAGIC2_VAL	(0xf1)

#define OST_START_TOKEN			(0x30)
#define OST_VERSION			(0x1)

#define STM_TXOR_CNTRL		(0x0)
#define STM_TXOR_TARGET_BADDR	(0x4)
#define STM_TXOR_EVENT_CONF0	(0x8)
#define STM_TXOR_EVENT_CONF1	(0xC)
#define STM_TXOR_EVENT_CONF2	(0x10)
#define STM_TXOR_EVENT_CONF3	(0x14)
#define STM_TXOR_STATUS0	(0x18)
#define STM_TXOR_STATUS1	(0x1C)
#define STM_TXOR_STATUS2	(0x20)
#define STM_TXOR_STATUS3	(0x24)
#define STM_TXOR_TX_CTR		(0x28)
#define STM_FIFO_LEVEL		(0x2C)

#define STM_B_COUNT_ZERO	(0x34)
#define STM_TXOR_INT_SEL	(0x38)
#define STM_TXOR_TRACE_INT	(0x3C)
#define STM_TXOR_TRACE_FINT	(0x40)

enum stm_pkt_type {
	STM_PKT_TYPE_DATA	= 0x98,
	STM_PKT_TYPE_FLAG	= 0xE8,
	STM_PKT_TYPE_TRIG	= 0xF8,
};

enum {
	STM_OPTION_MARKED	= 0x10,
};

#define stm_channel_addr(drvdata, ch)	(drvdata->chs.base +	\
					(ch * BYTES_PER_CHANNEL))
#define stm_channel_off(type, opts)	(type & ~opts)

#ifdef CONFIG_EXYNOS_CORESIGHT_STM_DEFAULT_ENABLE
static int boot_enable = 1;
#else
static int boot_enable;
#endif

module_param_named(
	boot_enable, boot_enable, int, S_IRUGO
);

static int boot_nr_channel;

module_param_named(
	boot_nr_channel, boot_nr_channel, int, S_IRUGO
);

struct channel_space {
	void __iomem		*base;
	unsigned long		*bitmap;
};

struct txa_space {
	void __iomem		*base;
	unsigned long		*bitmap;
};

struct stm_drvdata {
	void __iomem		*base;
	struct device		*dev;
	struct coresight_device	*csdev;
	struct miscdevice	miscdev;
	struct clk		*clk;
	spinlock_t		spinlock;
	struct channel_space	chs;
	struct txa_space		txa;
	uint32_t hwevt_stm_port;
	uint32_t hwevt_mode;
	bool enable;
	DECLARE_BITMAP(entities, OST_ENTITY_MAX);
};

static struct stm_drvdata *stmdrvdata;

static void __stm_hwevent_enable(struct stm_drvdata *drvdata)
{
	STM_UNLOCK(drvdata);

	/* Transactor enable */
	txa_writel(drvdata, drvdata->hwevt_stm_port, STM_TXOR_TARGET_BADDR);

	/* intial value: interupt group 0, all event mode */
	txa_writel(drvdata, 0x0, STM_TXOR_INT_SEL);
	txa_writel(drvdata, 10, STM_TXOR_TRACE_INT);

	txa_writel(drvdata, 0x1, STM_TXOR_CNTRL);
	STM_LOCK(drvdata);
}

static void __stm_hwevt_mode(struct stm_drvdata *drvdata)
{
	STM_UNLOCK(drvdata);

	txa_writel(drvdata, ((drvdata->hwevt_mode)>>31)&0x1, STM_TXOR_INT_SEL);
	txa_writel(drvdata, drvdata->hwevt_mode, STM_TXOR_TRACE_INT);

	STM_LOCK(drvdata);
}

static unsigned long stm_hwevt_mode_read(struct stm_drvdata *drvdata)
{
	int ret = 0;

	spin_lock(&drvdata->spinlock);
	STM_UNLOCK(drvdata);
	if (drvdata->enable) {
		ret = txa_readl(drvdata, STM_TXOR_INT_SEL) << 31;
		ret |= txa_readl(drvdata, STM_TXOR_TRACE_INT);
	}
	STM_LOCK(drvdata);
	spin_unlock(&drvdata->spinlock);

	return ret;
}

static int stm_hwevt_mode(struct stm_drvdata *drvdata)
{
	int ret = 0;

	spin_lock(&drvdata->spinlock);
	if (drvdata->enable)
		__stm_hwevt_mode(drvdata);
	else
		ret = -EINVAL;
	spin_unlock(&drvdata->spinlock);

	return ret;
}

static int stm_port_isenable(struct stm_drvdata *drvdata)
{
	int ret = 0;

	spin_lock(&drvdata->spinlock);
	STM_UNLOCK(drvdata);
	if (drvdata->enable)
		ret = stm_readl(drvdata, STMSPER) == 0 ? 0 : 1;
	STM_LOCK(drvdata);
	spin_unlock(&drvdata->spinlock);

	return ret;
}

static void __stm_port_enable(struct stm_drvdata *drvdata)
{
	STM_UNLOCK(drvdata);

	stm_writel(drvdata, 0x10, STMSPTRIGCSR);
	stm_writel(drvdata, 0xFFFFFFFF, STMSPER);

	STM_LOCK(drvdata);
}

static int stm_port_enable(struct stm_drvdata *drvdata)
{
	int ret = 0;

	spin_lock(&drvdata->spinlock);
	if (drvdata->enable)
		__stm_port_enable(drvdata);
	else
		ret = -EINVAL;
	spin_unlock(&drvdata->spinlock);

	return ret;
}

static void __stm_enable(struct stm_drvdata *drvdata)
{
	__stm_port_enable(drvdata);

	STM_UNLOCK(drvdata);

	stm_writel(drvdata, 0xFFF, STMSYNCR);
	/* SYNCEN is read-only and HWTEN is not implemented */
	stm_writel(drvdata, 0x100003, STMTCSR);
	__stm_hwevent_enable(drvdata);

	STM_LOCK(drvdata);
}

static int stm_enable(struct coresight_device *csdev)
{
	struct stm_drvdata *drvdata = dev_get_drvdata(csdev->dev.parent);

	exynos_etb_stm();
	spin_lock(&drvdata->spinlock);
	__stm_enable(drvdata);
	drvdata->enable = true;
	spin_unlock(&drvdata->spinlock);

	dev_info(drvdata->dev, "STM tracing enabled\n");
	return 0;
}

static void __stm_hwevent_disable(struct stm_drvdata *drvdata)
{
	STM_UNLOCK(drvdata);

	stm_writel(drvdata, 0x0, STMHEMCR);
	stm_writel(drvdata, 0x0, STMHEER);
	stm_writel(drvdata, 0x0, STMHETER);

	STM_LOCK(drvdata);
}

static void __stm_port_disable(struct stm_drvdata *drvdata)
{
	STM_UNLOCK(drvdata);

	stm_writel(drvdata, 0x0, STMSPER);
	stm_writel(drvdata, 0x0, STMSPTRIGCSR);

	STM_LOCK(drvdata);
}

static void stm_port_disable(struct stm_drvdata *drvdata)
{
	spin_lock(&drvdata->spinlock);
	if (drvdata->enable)
		__stm_port_disable(drvdata);
	spin_unlock(&drvdata->spinlock);
}

static void __stm_disable(struct stm_drvdata *drvdata)
{
	STM_UNLOCK(drvdata);

	stm_writel(drvdata, 0x100000, STMTCSR);

	STM_LOCK(drvdata);

	__stm_hwevent_disable(drvdata);
	__stm_port_disable(drvdata);
	exynos_etb_etm();
}

static void stm_disable(struct coresight_device *csdev)
{
	struct stm_drvdata *drvdata = dev_get_drvdata(csdev->dev.parent);

	spin_lock(&drvdata->spinlock);
	__stm_disable(drvdata);
	drvdata->enable = false;
	spin_unlock(&drvdata->spinlock);

	/* Wait for 100ms so that pending data has been written to HW */
	msleep(100);

	dev_info(drvdata->dev, "STM tracing disabled\n");
}

static const struct coresight_ops_source stm_source_ops = {
	.enable		= stm_enable,
	.disable	= stm_disable,
};

static const struct coresight_ops stm_cs_ops = {
	.source_ops	= &stm_source_ops,
};

static uint32_t stm_channel_alloc(uint32_t off)
{
	struct stm_drvdata *drvdata = stmdrvdata;
	uint32_t ch;

	do {
		ch = find_next_zero_bit(drvdata->chs.bitmap,
					NR_STM_CHANNEL, off);
	} while ((ch < NR_STM_CHANNEL) &&
		 test_and_set_bit(ch, drvdata->chs.bitmap));

	return ch;
}

static void stm_channel_free(uint32_t ch)
{
	struct stm_drvdata *drvdata = stmdrvdata;

	clear_bit(ch, drvdata->chs.bitmap);
}

static int stm_send(void *addr, const void *data, uint32_t size)
{
	uint64_t prepad = 0;
	uint64_t postpad = 0;
	char *pad;
	uint8_t off, endoff;
	uint32_t len = size;

	/*
	 * Only 64bit writes are supported. We rely on the compiler to
	 * generate STRD instruction for the casted 64bit assignments.
	 */

	off = (unsigned long)data & 0x7;

	if (off) {
		endoff = 8 - off;
		pad = (char *)&prepad;
		pad += off;

		while (endoff && size) {
			*pad++ = *(char *)data++;
			endoff--;
			size--;
		}
		*(volatile uint64_t __force *)addr = prepad;
	}

	/* Now we are 64bit aligned */
	while (size >= 8) {
		*(volatile uint64_t __force *)addr = *(uint64_t *)data;
		data += 8;
		size -= 8;
	}

	if (size) {
		pad = (char *)&postpad;

		while (size) {
			*pad++ = *(char *)data++;
			size--;
		}
		*(volatile uint64_t __force *)addr = postpad;
	}

	return roundup(len + off, 8);
}

static int stm_trace_ost_header(unsigned long ch_addr, uint32_t options,
				uint8_t entity_id, uint8_t proto_id,
				const void *payload_data, uint32_t payload_size)
{
	void *addr;
	uint8_t prepad_size;
	uint64_t header;
	char *hdr;

	hdr = (char *)&header;

	hdr[0] = OST_START_TOKEN;
	hdr[1] = OST_VERSION;
	hdr[2] = entity_id;
	hdr[3] = proto_id;
	prepad_size = (unsigned long)payload_data & 0x7;
	*(uint32_t *)(hdr + 4) = (prepad_size << 24) | payload_size;

	/* For 64bit writes, header is expected to be of the D32M, D32M */
	options |= STM_OPTION_MARKED;
	options &= ~STM_OPTION_TIMESTAMPED;
	addr =  (void *)(ch_addr | stm_channel_off(STM_PKT_TYPE_DATA, options));

	return stm_send(addr, &header, sizeof(header));
}

static int stm_trace_data(unsigned long ch_addr, uint32_t options,
			  const void *data, uint32_t size)
{
	void *addr;

	options &= ~STM_OPTION_TIMESTAMPED;
	addr = (void *)(ch_addr | stm_channel_off(STM_PKT_TYPE_DATA, options));

	return stm_send(addr, data, size);
}

static int stm_trace_ost_tail(unsigned long ch_addr, uint32_t options)
{
	void *addr;
	uint64_t tail = 0x0;

	addr = (void *)(ch_addr | stm_channel_off(STM_PKT_TYPE_FLAG, options));

	return stm_send(addr, &tail, sizeof(tail));
}

static inline int __stm_trace(uint32_t options, uint8_t entity_id,
			      uint8_t proto_id, const void *data, uint32_t size)
{
	struct stm_drvdata *drvdata = stmdrvdata;
	int len = 0;
	uint32_t ch;
	unsigned long ch_addr;

	/* Allocate channel and get the channel address */
	ch = stm_channel_alloc(0);
	ch_addr = (unsigned long)stm_channel_addr(drvdata, ch);

	/* Send the ost header */
	len += stm_trace_ost_header(ch_addr, options, entity_id, proto_id, data,
				    size);

	/* Send the payload data */
	len += stm_trace_data(ch_addr, options, data, size);

	/* Send the ost tail */
	len += stm_trace_ost_tail(ch_addr, options);

	/* We are done, free the channel */
	stm_channel_free(ch);

	return len;
}

/**
 * stm_trace - trace the binary or string data through STM
 * @options: tracing options - guaranteed, timestamped, etc
 * @entity_id: entity representing the trace data
 * @proto_id: protocol id to distinguish between different binary formats
 * @data: pointer to binary or string data buffer
 * @size: size of data to send
 *
 * Packetizes the data as the payload to an OST packet and sends it over STM
 *
 * CONTEXT:
 * Can be called from any context.
 *
 * RETURNS:
 * number of bytes transfered over STM
 */
int stm_trace(uint32_t options, uint8_t entity_id, uint8_t proto_id,
			const void *data, uint32_t size)
{
	struct stm_drvdata *drvdata = stmdrvdata;

	/* We don't support sizes more than 24bits (0 to 23) */
	if (!(drvdata && drvdata->enable &&
	      test_bit(entity_id, drvdata->entities) && size &&
	      (size < 0x1000000)))
		return 0;

	return __stm_trace(options, entity_id, proto_id, data, size);
}
EXPORT_SYMBOL_GPL(stm_trace);

static ssize_t stm_write(struct file *file, const char __user *data,
			 size_t size, loff_t *ppos)
{
	struct stm_drvdata *drvdata = container_of(file->private_data,
						   struct stm_drvdata, miscdev);
	char *buf;
	uint8_t entity_id, proto_id;
	uint32_t options;

	if (!drvdata->enable || !size)
		return -EINVAL;

	if (size > STM_TRACE_BUF_SIZE)
		size = STM_TRACE_BUF_SIZE;

	buf = kmalloc(size, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	if (copy_from_user(buf, data, size)) {
		kfree(buf);
		dev_dbg(drvdata->dev, "%s: copy_from_user failed\n", __func__);
		return -EFAULT;
	}

	if (size >= STM_USERSPACE_HEADER_SIZE &&
	    buf[0] == STM_USERSPACE_MAGIC1_VAL &&
	    buf[1] == STM_USERSPACE_MAGIC2_VAL) {

		entity_id = buf[2];
		proto_id = buf[3];
		options = *(uint32_t *)(buf + 4);

		if (!test_bit(entity_id, drvdata->entities) ||
		    !(size - STM_USERSPACE_HEADER_SIZE)) {
			kfree(buf);
			return size;
		}

		__stm_trace(options, entity_id, proto_id,
			    buf + STM_USERSPACE_HEADER_SIZE,
			    size - STM_USERSPACE_HEADER_SIZE);
	} else {
		if (!test_bit(OST_ENTITY_DEV_NODE, drvdata->entities)) {
			kfree(buf);
			return size;
		}

		__stm_trace(STM_OPTION_TIMESTAMPED, OST_ENTITY_DEV_NODE, 0,
			    buf, size);
	}

	kfree(buf);

	return size;
}

static const struct file_operations stm_fops = {
	.owner		= THIS_MODULE,
	.open		= nonseekable_open,
	.write		= stm_write,
	.llseek		= no_llseek,
};

static ssize_t stm_show_stm_en(struct device *dev,
			       struct device_attribute *attr, char *buf)
{
	struct stm_drvdata *drvdata = dev_get_drvdata(dev->parent);
	unsigned long val = stm_port_isenable(drvdata);

	return scnprintf(buf, PAGE_SIZE, "%#lx\n", val);
}

static ssize_t stm_store_stm_en(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t size)
{
	struct stm_drvdata *drvdata = dev_get_drvdata(dev->parent);
	uint32_t val;
	int ret = 0;

	if (sscanf(buf, "%x", &val) != 1)
		return -EINVAL;

	if (val) {
		exynos_etb_stm();
		spin_lock(&drvdata->spinlock);
		__stm_enable(drvdata);
		drvdata->enable = true;
		spin_unlock(&drvdata->spinlock);
	} else {
		spin_lock(&drvdata->spinlock);
		__stm_disable(drvdata);
		drvdata->enable = false;
		spin_unlock(&drvdata->spinlock);

		/* Wait for 100ms so that pending data has been written to HW */
		msleep(100);
	}

	if (ret)
		return ret;
	return size;
}
static DEVICE_ATTR(stm_enable, S_IRUGO | S_IWUSR, stm_show_stm_en,
		   stm_store_stm_en);

static ssize_t stm_show_port_enable(struct device *dev,
				    struct device_attribute *attr, char *buf)
{
	struct stm_drvdata *drvdata = dev_get_drvdata(dev->parent);
	unsigned long val = stm_port_isenable(drvdata);

	return scnprintf(buf, PAGE_SIZE, "%#lx\n", val);
}

static ssize_t stm_store_port_enable(struct device *dev,
				     struct device_attribute *attr,
				     const char *buf, size_t size)
{
	struct stm_drvdata *drvdata = dev_get_drvdata(dev->parent);
	unsigned long val;
	int ret = 0;

	if (sscanf(buf, "%lx", &val) != 1)
		return -EINVAL;

	if (val)
		ret = stm_port_enable(drvdata);
	else
		stm_port_disable(drvdata);

	if (ret)
		return ret;
	return size;
}
static DEVICE_ATTR(port_enable, S_IRUGO | S_IWUSR, stm_show_port_enable,
		   stm_store_port_enable);

static ssize_t stm_show_entities(struct device *dev,
			       struct device_attribute *attr, char *buf)
{
	struct stm_drvdata *drvdata = dev_get_drvdata(dev->parent);
	ssize_t len;

	len = bitmap_scnprintf(buf, PAGE_SIZE, drvdata->entities,
			       OST_ENTITY_MAX);

	if (PAGE_SIZE - len < 2)
		len = -EINVAL;
	else
		len += scnprintf(buf + len, 2, "\n");

	return len;
}

static ssize_t stm_store_entities(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t size)
{
	struct stm_drvdata *drvdata = dev_get_drvdata(dev->parent);
	unsigned long val1, val2;

	if (sscanf(buf, "%lx %lx", &val1, &val2) != 2)
		return -EINVAL;

	if (val1 >= OST_ENTITY_MAX)
		return -EINVAL;

	if (val2)
		__set_bit(val1, drvdata->entities);
	else
		__clear_bit(val1, drvdata->entities);

	return size;
}
static DEVICE_ATTR(entities, S_IRUGO | S_IWUSR, stm_show_entities,
		   stm_store_entities);

static ssize_t stm_show_hwevt_mode(struct device *dev,
				    struct device_attribute *attr, char *buf)
{
	struct stm_drvdata *drvdata = dev_get_drvdata(dev->parent);
	uint32_t val = stm_hwevt_mode_read(drvdata);

	return scnprintf(buf, PAGE_SIZE, "%x\n", val);
}

static ssize_t stm_store_hwevt_mode(struct device *dev,
				     struct device_attribute *attr,
				     const char *buf, size_t size)
{
	struct stm_drvdata *drvdata = dev_get_drvdata(dev->parent);
	uint32_t val;
	int ret = 0;

	sscanf(buf, "%x", &val);
	drvdata->hwevt_mode = val;

	if (val)
		ret = stm_hwevt_mode(drvdata);
	else
		stm_hwevt_mode(drvdata);

	if (ret)
		return ret;
	return size;
}
static DEVICE_ATTR(hwevt_mode, S_IRUGO | S_IWUSR, stm_show_hwevt_mode,
		   stm_store_hwevt_mode);

static struct attribute *stm_attrs[] = {
	&dev_attr_stm_enable.attr,
	&dev_attr_port_enable.attr,
	&dev_attr_entities.attr,
	&dev_attr_hwevt_mode.attr,
	NULL,
};

static struct attribute_group stm_attr_grp = {
	.attrs = stm_attrs,
};

static const struct attribute_group *stm_attr_grps[] = {
	&stm_attr_grp,
	NULL,
};

static int __devinit stm_probe(struct platform_device *pdev)
{
	int ret;
	struct device *dev = &pdev->dev;
	struct coresight_platform_data *pdata;
	struct stm_drvdata *drvdata;
	struct resource *res;
	size_t res_size, bitmap_size;
	struct coresight_desc *desc;

	if (pdev->dev.of_node) {
		pdata = of_get_coresight_platform_data(dev, pdev->dev.of_node);
		if (IS_ERR(pdata))
			return PTR_ERR(pdata);
		pdev->dev.platform_data = pdata;
	}

	drvdata = devm_kzalloc(dev, sizeof(*drvdata), GFP_KERNEL);
	if (!drvdata)
		return -ENOMEM;
	/* Store the driver data pointer for use in exported functions */
	stmdrvdata = drvdata;
	drvdata->dev = &pdev->dev;
	platform_set_drvdata(pdev, drvdata);

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res)
		return -ENODEV;

	drvdata->base = devm_ioremap(dev, res->start, resource_size(res));
	if (!drvdata->base)
		return -ENOMEM;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 1);
	if (!res)
		return -ENODEV;

	if (boot_nr_channel) {
		res_size = min((resource_size_t)(boot_nr_channel *
				  BYTES_PER_CHANNEL), resource_size(res));
		bitmap_size = boot_nr_channel * sizeof(long);
	} else {
		res_size = min((resource_size_t)(NR_STM_CHANNEL *
				 BYTES_PER_CHANNEL), resource_size(res));
		bitmap_size = NR_STM_CHANNEL * sizeof(long);
	}

	drvdata->hwevt_stm_port = res->start + 0x100;

	drvdata->chs.base = devm_ioremap(dev, res->start, res_size);
	if (!drvdata->chs.base)
		return -ENOMEM;
	drvdata->chs.bitmap = devm_kzalloc(dev, bitmap_size, GFP_KERNEL);
	if (!drvdata->chs.bitmap)
		return -ENOMEM;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 2);
	if (!res)
		return -ENODEV;

	drvdata->txa.base = devm_ioremap(dev, res->start, res_size);
	if (!drvdata->txa.base)
		return -ENOMEM;
	drvdata->txa.bitmap = devm_kzalloc(dev, bitmap_size, GFP_KERNEL);
	if (!drvdata->txa.bitmap)
		return -ENOMEM;

	spin_lock_init(&drvdata->spinlock);

	bitmap_fill(drvdata->entities, OST_ENTITY_MAX);

	desc = devm_kzalloc(dev, sizeof(*desc), GFP_KERNEL);
	if (!desc)
		return -ENOMEM;
	desc->type = CORESIGHT_DEV_TYPE_SOURCE;
	desc->subtype.source_subtype = CORESIGHT_DEV_SUBTYPE_SOURCE_SOFTWARE;
	desc->ops = &stm_cs_ops;
	desc->pdata = pdev->dev.platform_data;
	desc->dev = &pdev->dev;
	desc->groups = stm_attr_grps;
	desc->owner = THIS_MODULE;
	drvdata->csdev = coresight_register(desc);
	if (IS_ERR(drvdata->csdev))
		return PTR_ERR(drvdata->csdev);

	drvdata->miscdev.name = ((struct coresight_platform_data *)
				 (pdev->dev.platform_data))->name;
	drvdata->miscdev.minor = MISC_DYNAMIC_MINOR;
	drvdata->miscdev.fops = &stm_fops;
	ret = misc_register(&drvdata->miscdev);
	if (ret)
		goto err;

	dev_info(drvdata->dev, "STM initialized\n");

	if (boot_enable)
		stm_enable(drvdata->csdev);

	return 0;
err:
	coresight_unregister(drvdata->csdev);
	return ret;
}

static int __devexit stm_remove(struct platform_device *pdev)
{
	struct stm_drvdata *drvdata = platform_get_drvdata(pdev);

	misc_deregister(&drvdata->miscdev);
	coresight_unregister(drvdata->csdev);
	return 0;
}

static struct of_device_id stm_match[] = {
	{.compatible = "arm,coresight-stm"},
	{}
};

static struct platform_driver stm_driver = {
	.probe		= stm_probe,
	.remove		= __devexit_p(stm_remove),
	.driver		= {
		.name	= "coresight-stm",
		.owner	= THIS_MODULE,
		.of_match_table = stm_match,
	},
};

static int __init stm_init(void)
{
	return platform_driver_register(&stm_driver);
}
late_initcall(stm_init);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("CoreSight System Trace Macrocell driver");
