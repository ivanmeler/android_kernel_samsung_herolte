/**
 * core.c - DesignWare USB3 DRD Controller Core file
 *
 * Copyright (C) 2010-2011 Texas Instruments Incorporated - http://www.ti.com
 *
 * Authors: Felipe Balbi <balbi@ti.com>,
 *	    Sebastian Andrzej Siewior <bigeasy@linutronix.de>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2  of
 * the License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>
#include <linux/interrupt.h>
#include <linux/ioport.h>
#include <linux/io.h>
#include <linux/list.h>
#include <linux/delay.h>
#include <linux/dma-mapping.h>
#include <linux/of.h>

#include <linux/usb/ch9.h>
#include <linux/usb/gadget.h>
#include <linux/usb/of.h>
#include <linux/usb/otg.h>
#include <linux/phy/phy.h>

#include "platform_data.h"
#include "core.h"
#include "gadget.h"
#include "io.h"

#include "debug.h"

/* -------------------------------------------------------------------------- */

void dwc3_set_mode(struct dwc3 *dwc, u32 mode)
{
	u32 reg;

	reg = dwc3_readl(dwc->regs, DWC3_GCTL);
	reg &= ~(DWC3_GCTL_PRTCAPDIR(DWC3_GCTL_PRTCAP_OTG));
	reg |= DWC3_GCTL_PRTCAPDIR(mode);
	dwc3_writel(dwc->regs, DWC3_GCTL, reg);
}

/**
 * dwc3_core_soft_reset - Issues core soft reset and PHY reset
 * @dwc: pointer to our context structure
 */
static int dwc3_core_soft_reset(struct dwc3 *dwc)
{
	u32		reg;
	int		ret;

	/* Before Resetting PHY, put Core in Reset */
	reg = dwc3_readl(dwc->regs, DWC3_GCTL);
	reg |= DWC3_GCTL_CORESOFTRESET;
	dwc3_writel(dwc->regs, DWC3_GCTL, reg);

	/* Assert USB3 PHY reset */
	reg = dwc3_readl(dwc->regs, DWC3_GUSB3PIPECTL(0));
	reg |= DWC3_GUSB3PIPECTL_PHYSOFTRST;
	dwc3_writel(dwc->regs, DWC3_GUSB3PIPECTL(0), reg);

	/* Assert USB2 PHY reset */
	reg = dwc3_readl(dwc->regs, DWC3_GUSB2PHYCFG(0));
	reg |= DWC3_GUSB2PHYCFG_PHYSOFTRST;
	dwc3_writel(dwc->regs, DWC3_GUSB2PHYCFG(0), reg);

	ret = phy_power_on(dwc->usb3_generic_phy);
	if (ret < 0)
		return ret;
	ret = phy_power_on(dwc->usb2_generic_phy);
	if (ret < 0)
		goto err_usb3phy_power;
	usb_phy_init(dwc->usb3_phy);
	usb_phy_init(dwc->usb2_phy);
	ret = phy_init(dwc->usb3_generic_phy);
	if (ret < 0)
		goto err_usb2phy_power;
	ret = phy_init(dwc->usb2_generic_phy);
	if (ret < 0)
		goto err_usb3phy_init;

	mdelay(100);

	/* Clear USB3 PHY reset */
	reg = dwc3_readl(dwc->regs, DWC3_GUSB3PIPECTL(0));
	reg &= ~DWC3_GUSB3PIPECTL_PHYSOFTRST;
	dwc3_writel(dwc->regs, DWC3_GUSB3PIPECTL(0), reg);

	/* Clear USB2 PHY reset */
	reg = dwc3_readl(dwc->regs, DWC3_GUSB2PHYCFG(0));
	reg &= ~DWC3_GUSB2PHYCFG_PHYSOFTRST;
	dwc3_writel(dwc->regs, DWC3_GUSB2PHYCFG(0), reg);

	mdelay(100);

	/* After PHYs are stable we can take Core out of reset state */
	reg = dwc3_readl(dwc->regs, DWC3_GCTL);
	reg &= ~DWC3_GCTL_CORESOFTRESET;
	dwc3_writel(dwc->regs, DWC3_GCTL, reg);

	return 0;

err_usb3phy_init:
	phy_exit(dwc->usb3_generic_phy);

err_usb2phy_power:
	phy_power_off(dwc->usb2_generic_phy);

err_usb3phy_power:
	phy_power_off(dwc->usb3_generic_phy);

	return ret;
}

void dwc3_core_config(struct dwc3 *dwc)
{
	u32 reg;

	/* AHB bus configuration */
	reg = dwc3_readl(dwc->regs, DWC3_GSBUSCFG0);
	reg |= (DWC3_GSBUSCFG0_INCRBRSTEN | DWC3_GSBUSCFG0_INCR16BRSTEN);
	/**
	 * AXI Bus' cache type configuration for DMA transfer.
	 * By below setting, cache type was set to Cacheable/Modifiable.
	 * From DWC USB3.0 Link version 2.20A, this cache type could be set.
	 */
	if (dwc->revision >= DWC3_REVISION_220A)
		reg |= (DWC3_GSBUSCFG0_DESWRREQINFO |
			DWC3_GSBUSCFG0_DATWRREQINFO |
			DWC3_GSBUSCFG0_DESRDREQINFO |
			DWC3_GSBUSCFG0_DATRDREQINFO);
	dwc3_writel(dwc->regs, DWC3_GSBUSCFG0, reg);

	reg = dwc3_readl(dwc->regs, DWC3_GSBUSCFG1);
	reg |= (DWC3_GSBUSCFG1_BREQLIMIT(3));
	dwc3_writel(dwc->regs, DWC3_GSBUSCFG1, reg);

	/*
	 * WORKAROUND:
	 * For ss bulk-in data packet, when the host detects
	 * a DPP error or the internal buffer becomes full,
	 * it retries with an ACK TP Retry=1. Under the following
	 * conditions, the Retry=1 is falsely carried over to the next
	 * DWC3_GUCTL_USBHSTINAUTORETRYEN should be set to a one
	 * regardless of revision
	 * - There is only single active asynchronous SS EP at the time.
	 * - The active asynchronous EP is a Bulk IN EP.
	 * - The burst with the correctly Retry=1 ACK TP and
	 *   the next burst belong to the same transfer.
	 */
	reg = dwc3_readl(dwc->regs, DWC3_GUCTL);
	reg |= (DWC3_GUCTL_USBHSTINAUTORETRYEN);
	if (dwc->adj_sof_accuracy) {
		reg &= ~DWC3_GUCTL_REFCLKPER_MASK;
		reg |= DWC3_GUCTL_REFCLKPER(0x29);
	}
	reg |= DWC3_GUCTL_SPRSCTRLTRANSEN;
	dwc3_writel(dwc->regs, DWC3_GUCTL, reg);
	if (dwc->revision >= DWC3_REVISION_190A &&
		dwc->revision <= DWC3_REVISION_210A) {
		reg = dwc3_readl(dwc->regs, DWC3_GRXTHRCFG);
		reg &= ~(DWC3_GRXTHRCFG_USBRXPKTCNT_MASK |
			DWC3_GRXTHRCFG_USBMAXRXBURSTSIZE_MASK);
		reg |= (DWC3_GRXTHRCFG_USBRXPKTCNTSEL |
			DWC3_GRXTHRCFG_USBRXPKTCNT(3) |
			DWC3_GRXTHRCFG_USBMAXRXBURSTSIZE(3));
		dwc3_writel(dwc->regs, DWC3_GRXTHRCFG, reg);
	}

	/*
	 * WORKAROUND: DWC3 revisions 2.10a and earlier have a bug
	 * The delay of the entry to a low power state such that
	 * for applications where the link stays in a non-U0 state
	 * for a short duration(< 1 microsecond),
	 * the local PHY does not enter the low power state prior
	 * to receiving a potential LFPS wakeup.
	 * This causes the PHY CDR (Clock and Data Recovery) operation
	 * to be unstable for some Synopsys PHYs.
	 * The proposal now is to change the default and the recommended value
	 * for GUSB3PIPECTL[21:19] in the RTL from 3'b100 to a minimum of 3'b001
	 */
	if (dwc->revision <= DWC3_REVISION_210A) {
		reg = dwc3_readl(dwc->regs, DWC3_GUSB3PIPECTL(0));
		reg &= ~(DWC3_GUSB3PIPECTL_DELAY_P1P2P3_MASK);
		reg |= (DWC3_GUSB3PIPECTL_DELAY_P1P2P3(1));
		dwc3_writel(dwc->regs, DWC3_GUSB3PIPECTL(0), reg);
	}

	if (dwc->revision >= DWC3_REVISION_250A) {
		reg = dwc3_readl(dwc->regs, DWC3_GUSB3PIPECTL(0));
		reg |= DWC3_GUSB3PIPECTL_DIS_RXDETP3;
		dwc3_writel(dwc->regs, DWC3_GUSB3PIPECTL(0), reg);
	}

	/*
	 * WORKAROUND: DWC3 revisions 2.10a and earlier have a bug
	 * Race Condition in PORTSC Write Followed by Read
	 * If the software quickly does a read to the PORTSC,
	 * some fields (port status change related fields
	 * like OCC, etc.) may not have correct value
	 * due to the current way of handling these bits.
	 * After clearing the status register (for example, OCC) bit
	 * by writing PORTSC tregister, software can insert some delay
	 * (for example, 5 mac2_clk -> UTMI clock = 60 MHz ->
	 * (16.66 ns x 5 = 84ns)) before reading the PORTSC to check status.
	 */
	if (dwc->revision <= DWC3_REVISION_210A) {
		reg = dwc3_readl(dwc->regs, DWC3_GUSB2PHYCFG(0));
		reg |= (DWC3_GUSB2PHYCFG_PHYIF);
		dwc3_writel(dwc->regs, DWC3_GUSB2PHYCFG(0), reg);
	}

}

int dwc3_udc_reset(struct dwc3 *dwc)
{
	unsigned long   timeout;
	u32		reg;
	int		ret = 0;

	timeout = jiffies + msecs_to_jiffies(500);
	dwc3_writel(dwc->regs, DWC3_DCTL, DWC3_DCTL_CSFTRST);
	do {
		reg = dwc3_readl(dwc->regs, DWC3_DCTL);
		if (!(reg & DWC3_DCTL_CSFTRST))
			break;

		if (time_after(jiffies, timeout)) {
			dev_err(dwc->dev, "Reset Timed Out\n");
			ret = -ETIMEDOUT;
			break;
		}

		cpu_relax();
	} while (true);

	return ret;
}

/**
 * dwc3_free_one_event_buffer - Frees one event buffer
 * @dwc: Pointer to our controller context structure
 * @evt: Pointer to event buffer to be freed
 */
static void dwc3_free_one_event_buffer(struct dwc3 *dwc,
		struct dwc3_event_buffer *evt)
{
	dma_free_coherent(dwc->dev, evt->length, evt->buf, evt->dma);
}

/**
 * dwc3_alloc_one_event_buffer - Allocates one event buffer structure
 * @dwc: Pointer to our controller context structure
 * @length: size of the event buffer
 *
 * Returns a pointer to the allocated event buffer structure on success
 * otherwise ERR_PTR(errno).
 */
static struct dwc3_event_buffer *dwc3_alloc_one_event_buffer(struct dwc3 *dwc,
		unsigned length)
{
	struct dwc3_event_buffer	*evt;

	evt = devm_kzalloc(dwc->dev, sizeof(*evt), GFP_KERNEL);
	if (!evt)
		return ERR_PTR(-ENOMEM);

	evt->dwc	= dwc;
	evt->length	= length;
	evt->buf	= dma_alloc_coherent(dwc->dev, length,
			&evt->dma, GFP_KERNEL);
	if (!evt->buf)
		return ERR_PTR(-ENOMEM);

	return evt;
}

/**
 * dwc3_free_event_buffers - frees all allocated event buffers
 * @dwc: Pointer to our controller context structure
 */
static void dwc3_free_event_buffers(struct dwc3 *dwc)
{
	struct dwc3_event_buffer	*evt;
	int i;

	for (i = 0; i < dwc->num_event_buffers; i++) {
		evt = dwc->ev_buffs[i];
		if (evt)
			dwc3_free_one_event_buffer(dwc, evt);
	}
}

/**
 * dwc3_alloc_event_buffers - Allocates @num event buffers of size @length
 * @dwc: pointer to our controller context structure
 * @length: size of event buffer
 *
 * Returns 0 on success otherwise negative errno. In the error case, dwc
 * may contain some buffers allocated but not all which were requested.
 */
static int dwc3_alloc_event_buffers(struct dwc3 *dwc, unsigned length)
{
	int			num;
	int			i;

	num = DWC3_NUM_INT(dwc->hwparams.hwparams1);
	dwc->num_event_buffers = num;

	dwc->ev_buffs = devm_kzalloc(dwc->dev, sizeof(*dwc->ev_buffs) * num,
			GFP_KERNEL);
	if (!dwc->ev_buffs)
		return -ENOMEM;

	for (i = 0; i < num; i++) {
		struct dwc3_event_buffer	*evt;

		evt = dwc3_alloc_one_event_buffer(dwc, length);
		if (IS_ERR(evt)) {
			dev_err(dwc->dev, "can't allocate event buffer\n");
			return PTR_ERR(evt);
		}
		dwc->ev_buffs[i] = evt;
	}

	return 0;
}

/**
 * dwc3_event_buffers_setup - setup our allocated event buffers
 * @dwc: pointer to our controller context structure
 *
 * Returns 0 on success otherwise negative errno.
 */
int dwc3_event_buffers_setup(struct dwc3 *dwc)
{
	struct dwc3_event_buffer	*evt;
	int				n;

	for (n = 0; n < dwc->num_event_buffers; n++) {
		evt = dwc->ev_buffs[n];
		dev_info(dwc->dev, "Event buf %p dma %08llx length %d\n",
				evt->buf, (unsigned long long) evt->dma,
				evt->length);

		evt->lpos = 0;

		dwc3_writel(dwc->regs, DWC3_GEVNTADRLO(n),
				lower_32_bits(evt->dma));
		dwc3_writel(dwc->regs, DWC3_GEVNTADRHI(n),
				upper_32_bits(evt->dma));
		dwc3_writel(dwc->regs, DWC3_GEVNTSIZ(n),
				DWC3_GEVNTSIZ_SIZE(evt->length));
		dwc3_writel(dwc->regs, DWC3_GEVNTCOUNT(n), 0);
	}

	return 0;
}

void dwc3_event_buffers_cleanup(struct dwc3 *dwc)
{
	struct dwc3_event_buffer	*evt;
	int				n;

	for (n = 0; n < dwc->num_event_buffers; n++) {
		evt = dwc->ev_buffs[n];

		evt->lpos = 0;

		dwc3_writel(dwc->regs, DWC3_GEVNTSIZ(n), DWC3_GEVNTSIZ_INTMASK
				| DWC3_GEVNTSIZ_SIZE(0));
		dwc3_writel(dwc->regs, DWC3_GEVNTCOUNT(n), 0);
	}
}

static int dwc3_alloc_scratch_buffers(struct dwc3 *dwc)
{
	if (!dwc->has_hibernation)
		return 0;

	if (!dwc->nr_scratch)
		return 0;

	dwc->scratchbuf = kmalloc_array(dwc->nr_scratch,
			DWC3_SCRATCHBUF_SIZE, GFP_KERNEL);
	if (!dwc->scratchbuf)
		return -ENOMEM;

	return 0;
}

static int dwc3_setup_scratch_buffers(struct dwc3 *dwc)
{
	dma_addr_t scratch_addr;
	u32 param;
	int ret;

	if (!dwc->has_hibernation)
		return 0;

	if (!dwc->nr_scratch)
		return 0;

	 /* should never fall here */
	if (!WARN_ON(dwc->scratchbuf))
		return 0;

	scratch_addr = dma_map_single(dwc->dev, dwc->scratchbuf,
			dwc->nr_scratch * DWC3_SCRATCHBUF_SIZE,
			DMA_BIDIRECTIONAL);
	if (dma_mapping_error(dwc->dev, scratch_addr)) {
		dev_err(dwc->dev, "failed to map scratch buffer\n");
		ret = -EFAULT;
		goto err0;
	}

	dwc->scratch_addr = scratch_addr;

	param = lower_32_bits(scratch_addr);

	ret = dwc3_send_gadget_generic_command(dwc,
			DWC3_DGCMD_SET_SCRATCHPAD_ADDR_LO, param);
	if (ret < 0)
		goto err1;

	param = upper_32_bits(scratch_addr);

	ret = dwc3_send_gadget_generic_command(dwc,
			DWC3_DGCMD_SET_SCRATCHPAD_ADDR_HI, param);
	if (ret < 0)
		goto err1;

	return 0;

err1:
	dma_unmap_single(dwc->dev, dwc->scratch_addr, dwc->nr_scratch *
			DWC3_SCRATCHBUF_SIZE, DMA_BIDIRECTIONAL);

err0:
	return ret;
}

static void dwc3_free_scratch_buffers(struct dwc3 *dwc)
{
	if (!dwc->has_hibernation)
		return;

	if (!dwc->nr_scratch)
		return;

	 /* should never fall here */
	if (!WARN_ON(dwc->scratchbuf))
		return;

	dma_unmap_single(dwc->dev, dwc->scratch_addr, dwc->nr_scratch *
			DWC3_SCRATCHBUF_SIZE, DMA_BIDIRECTIONAL);
	kfree(dwc->scratchbuf);
}

static void dwc3_core_num_eps(struct dwc3 *dwc)
{
	struct dwc3_hwparams	*parms = &dwc->hwparams;

	dwc->num_in_eps = DWC3_NUM_IN_EPS(parms);
	dwc->num_out_eps = DWC3_NUM_EPS(parms) - dwc->num_in_eps;

	dev_vdbg(dwc->dev, "found %d IN and %d OUT endpoints\n",
			dwc->num_in_eps, dwc->num_out_eps);
}

static void dwc3_cache_hwparams(struct dwc3 *dwc)
{
	struct dwc3_hwparams	*parms = &dwc->hwparams;

	parms->hwparams0 = dwc3_readl(dwc->regs, DWC3_GHWPARAMS0);
	parms->hwparams1 = dwc3_readl(dwc->regs, DWC3_GHWPARAMS1);
	parms->hwparams2 = dwc3_readl(dwc->regs, DWC3_GHWPARAMS2);
	parms->hwparams3 = dwc3_readl(dwc->regs, DWC3_GHWPARAMS3);
	parms->hwparams4 = dwc3_readl(dwc->regs, DWC3_GHWPARAMS4);
	parms->hwparams5 = dwc3_readl(dwc->regs, DWC3_GHWPARAMS5);
	parms->hwparams6 = dwc3_readl(dwc->regs, DWC3_GHWPARAMS6);
	parms->hwparams7 = dwc3_readl(dwc->regs, DWC3_GHWPARAMS7);
	parms->hwparams8 = dwc3_readl(dwc->regs, DWC3_GHWPARAMS8);
}

/**
 * dwc3_core_init - Low-level initialization of DWC3 Core
 * @dwc: Pointer to our controller context structure
 *
 * Returns 0 on success otherwise negative errno.
 */
int dwc3_core_init(struct dwc3 *dwc)
{
	u32			hwparams4 = dwc->hwparams.hwparams4;
	u32			reg;
	int			ret;

	reg = dwc3_readl(dwc->regs, DWC3_GSNPSID);
	/* This should read as U3 followed by revision number */
	if ((reg & DWC3_GSNPSID_MASK) != 0x55330000) {
		dev_err(dwc->dev, "this is not a DesignWare USB3 DRD Core\n");
		ret = -ENODEV;
		goto err0;
	}
	dwc->revision = reg;

	/* Handle USB2.0-only core configuration */
	if (DWC3_GHWPARAMS3_SSPHY_IFC(dwc->hwparams.hwparams3) ==
			DWC3_GHWPARAMS3_SSPHY_IFC_DIS) {
		if (dwc->maximum_speed == USB_SPEED_SUPER)
			dwc->maximum_speed = USB_SPEED_HIGH;
	}

	/* Adjust SOF accuracy only for revisions >= 2.50a */
	if (dwc->revision < DWC3_REVISION_250A)
		dwc->adj_sof_accuracy = 0;

	/* issue device SoftReset too */
	ret = dwc3_udc_reset(dwc);
	if (ret < 0)
		goto err0;

	ret = dwc3_core_soft_reset(dwc);
	if (ret)
		goto err0;

	if (dwc->usb3_phy) {
		/*
		 * The state of usb phy was set by otg state machine.
		 * Please, refer to the function "dwc3_otg_statemachine".
		 */
		phy_tune(dwc->usb2_generic_phy, dwc->usb3_phy->state);
		phy_tune(dwc->usb3_generic_phy, dwc->usb3_phy->state);
	} else {
		/* There is not any information of the state of usb phy */
		phy_tune(dwc->usb2_generic_phy, 0);
		phy_tune(dwc->usb3_generic_phy, 0);
	}

	reg = dwc3_readl(dwc->regs, DWC3_GCTL);
	reg &= ~DWC3_GCTL_SCALEDOWN_MASK;
	reg &= ~DWC3_GCTL_DISSCRAMBLE;

	switch (DWC3_GHWPARAMS1_EN_PWROPT(dwc->hwparams.hwparams1)) {
	case DWC3_GHWPARAMS1_EN_PWROPT_CLK:
		/**
		 * WORKAROUND: DWC3 revisions between 2.10a and 2.50a have an
		 * issue which would cause xHCI compliance tests to fail.
		 *
		 * Because of that we cannot enable clock gating on such
		 * configurations.
		 *
		 * Refers to:
		 *
		 * STAR#9000588375: Clock Gating, SOF Issues when ref_clk-Based
		 * SOF/ITP Mode Used
		 */
		if ((dwc->dr_mode == USB_DR_MODE_HOST ||
				dwc->dr_mode == USB_DR_MODE_OTG) &&
				(dwc->revision >= DWC3_REVISION_210A &&
				dwc->revision <= DWC3_REVISION_250A))
			reg |= DWC3_GCTL_DSBLCLKGTNG | DWC3_GCTL_SOFITPSYNC;
		else
			reg &= ~DWC3_GCTL_DSBLCLKGTNG;
		break;
	case DWC3_GHWPARAMS1_EN_PWROPT_HIB:
		/* enable hibernation here */
		dwc->nr_scratch = DWC3_GHWPARAMS4_HIBER_SCRATCHBUFS(hwparams4);
		break;
	default:
		dev_dbg(dwc->dev, "No power optimization available\n");
	}

	/*
	 * WORKAROUND: DWC3 revisions <1.90a have a bug
	 * where the device can fail to connect at SuperSpeed
	 * and falls back to high-speed mode which causes
	 * the device to enter a Connect/Disconnect loop
	 */
	if (dwc->revision < DWC3_REVISION_190A)
		reg |= DWC3_GCTL_U2RSTECN;

	if (dwc->adj_sof_accuracy) {
		reg &= ~DWC3_GCTL_SOFITPSYNC;
		reg |= DWC3_GCTL_DSBLCLKGTNG;
	}

	dwc3_core_num_eps(dwc);

	if (dwc->suspend_clk_freq) {
		reg &= ~DWC3_GCTL_PWRDNSCALE_MASK;
		reg |= DWC3_GCTL_PWRDNSCALE(dwc->suspend_clk_freq/(16*1000));
	}
	dwc3_writel(dwc->regs, DWC3_GCTL, reg);

	dev_info(dwc->dev, "%s: max speed:%d, hibernation:%d, nr_scratch:%d\n",
			__func__, dwc->maximum_speed, dwc->has_hibernation,
			dwc->nr_scratch);

	ret = dwc3_alloc_scratch_buffers(dwc);
	if (ret)
		goto err1;

	ret = dwc3_setup_scratch_buffers(dwc);
	if (ret)
		goto err2;

	dwc3_core_config(dwc);

	reg = dwc3_readl(dwc->regs, DWC3_GUSB2PHYCFG(0));
	reg |= DWC3_GUSB2PHYCFG_SUSPHY;
	if (dwc->adj_sof_accuracy)
		reg &= ~DWC3_GUSB2PHYCFG_U2_FREECLK_EXISTS;
	dwc3_writel(dwc->regs, DWC3_GUSB2PHYCFG(0), reg);

	reg = dwc3_readl(dwc->regs, DWC3_GUSB3PIPECTL(0));
	reg |= DWC3_GUSB3PIPECTL_SUSPHY;
	dwc3_writel(dwc->regs, DWC3_GUSB3PIPECTL(0), reg);

	if (dwc->adj_sof_accuracy) {
		reg = dwc3_readl(dwc->regs, DWC3_GFLADJ);
		reg &= ~DWC3_GFLADJ_REFCLK_240MHZDECR_PLS1;
		reg &= ~DWC3_GFLADJ_REFCLK_240MHZ_DECR_MASK;
		reg |= DWC3_GFLADJ_REFCLK_240MHZ_DECR(0xA);
		reg |= DWC3_GFLADJ_REFCLK_LPM_SEL;
		reg &= ~DWC3_GFLADJ_REFCLK_FLADJ_MASK;
		reg |= DWC3_GFLADJ_REFCLK_FLADJ(0x7F0);
		dwc3_writel(dwc->regs, DWC3_GFLADJ, reg);
	}

	return 0;

err2:
	dwc3_free_scratch_buffers(dwc);

err1:
	usb_phy_shutdown(dwc->usb2_phy);
	usb_phy_shutdown(dwc->usb3_phy);
	phy_exit(dwc->usb2_generic_phy);
	phy_exit(dwc->usb3_generic_phy);
	phy_power_off(dwc->usb2_generic_phy);
	phy_power_off(dwc->usb3_generic_phy);

err0:
	return ret;
}

void dwc3_core_exit(struct dwc3 *dwc)
{
	dwc3_free_scratch_buffers(dwc);
	usb_phy_shutdown(dwc->usb2_phy);
	usb_phy_shutdown(dwc->usb3_phy);
	phy_exit(dwc->usb2_generic_phy);
	phy_exit(dwc->usb3_generic_phy);
	phy_power_off(dwc->usb2_generic_phy);
	phy_power_off(dwc->usb3_generic_phy);
}

static int dwc3_core_get_phy(struct dwc3 *dwc)
{
	struct device		*dev = dwc->dev;
	struct device_node	*node = dev->of_node;
	int ret;

	if (node) {
		dwc->usb2_phy = devm_usb_get_phy_by_phandle(dev, "usb-phy", 0);
		dwc->usb3_phy = devm_usb_get_phy_by_phandle(dev, "usb-phy", 1);
	} else {
		dwc->usb2_phy = devm_usb_get_phy(dev, USB_PHY_TYPE_USB2);
		dwc->usb3_phy = devm_usb_get_phy(dev, USB_PHY_TYPE_USB3);
	}

	if (IS_ERR(dwc->usb2_phy)) {
		ret = PTR_ERR(dwc->usb2_phy);
		if (ret == -ENXIO || ret == -ENODEV) {
			dwc->usb2_phy = NULL;
		} else if (ret == -EPROBE_DEFER) {
			return ret;
		} else {
			dev_err(dev, "no usb2 phy configured\n");
			return ret;
		}
	}

	if (IS_ERR(dwc->usb3_phy)) {
		ret = PTR_ERR(dwc->usb3_phy);
		if (ret == -ENXIO || ret == -ENODEV) {
			if (IS_ENABLED(CONFIG_PHY_EXYNOS_USBDRD))
				dwc->usb3_phy = devm_kzalloc(dev,
					sizeof(struct usb_phy), GFP_KERNEL);
			else
				dwc->usb3_phy = NULL;
		} else if (ret == -EPROBE_DEFER) {
			return ret;
		} else {
			dev_err(dev, "no usb3 phy configured\n");
			return ret;
		}
	}

	dwc->usb2_generic_phy = devm_phy_get(dev, "usb2-phy");
	if (IS_ERR(dwc->usb2_generic_phy)) {
		ret = PTR_ERR(dwc->usb2_generic_phy);
		if (ret == -ENOSYS || ret == -ENODEV) {
			dwc->usb2_generic_phy = NULL;
		} else if (ret == -EPROBE_DEFER) {
			return ret;
		} else {
			dev_err(dev, "no usb2 phy configured\n");
			return ret;
		}
	}

	dwc->usb3_generic_phy = devm_phy_get(dev, "usb3-phy");
	if (IS_ERR(dwc->usb3_generic_phy)) {
		ret = PTR_ERR(dwc->usb3_generic_phy);
		if (ret == -ENOSYS || ret == -ENODEV) {
			dwc->usb3_generic_phy = NULL;
		} else if (ret == -EPROBE_DEFER) {
			return ret;
		} else {
			dev_err(dev, "no usb3 phy configured\n");
			return ret;
		}
	}

	return 0;
}

static int dwc3_core_init_mode(struct dwc3 *dwc)
{
	struct device *dev = dwc->dev;
	int ret;

	switch (dwc->dr_mode) {
	case USB_DR_MODE_PERIPHERAL:
		dwc3_set_mode(dwc, DWC3_GCTL_PRTCAP_DEVICE);
		ret = dwc3_gadget_init(dwc);
		if (ret) {
			dev_err(dev, "failed to initialize gadget\n");
			return ret;
		}
		break;
	case USB_DR_MODE_HOST:
		dwc3_set_mode(dwc, DWC3_GCTL_PRTCAP_HOST);
		ret = dwc3_host_init(dwc);
		if (ret) {
			dev_err(dev, "failed to initialize host\n");
			return ret;
		}
		break;
	case USB_DR_MODE_OTG:
		dwc3_set_mode(dwc, DWC3_GCTL_PRTCAP_OTG);
		ret = dwc3_otg_init(dwc);
		if (ret) {
			dev_err(dev, "failed to initialize otg\n");
			return ret;
		}

		/* turn off PHYs to save power */
		dwc3_core_exit(dwc);

		ret = dwc3_host_init(dwc);
		if (ret) {
			dev_err(dev, "failed to initialize host\n");
			dwc3_otg_exit(dwc);
			return ret;
		}

		ret = dwc3_gadget_init(dwc);
		if (ret) {
			dev_err(dev, "failed to initialize gadget\n");
			dwc3_host_exit(dwc);
			dwc3_otg_exit(dwc);
			return ret;
		}

		/* Now we are ready to start OTG */
		ret = dwc3_otg_start(dwc);
		if (ret) {
			dev_err(dev, "failed to start otg\n");
			dwc3_host_exit(dwc);
			dwc3_gadget_exit(dwc);
			dwc3_otg_exit(dwc);
			return ret;
		}

		/* Unblock runtime PM for OTG */
		pm_runtime_put_sync(dev);

		break;
	default:
		dev_err(dev, "Unsupported mode of operation %d\n", dwc->dr_mode);
		return -EINVAL;
	}

	return 0;
}

static void dwc3_core_exit_mode(struct dwc3 *dwc)
{
	switch (dwc->dr_mode) {
	case USB_DR_MODE_PERIPHERAL:
		dwc3_gadget_exit(dwc);
		break;
	case USB_DR_MODE_HOST:
		dwc3_host_exit(dwc);
		break;
	case USB_DR_MODE_OTG:
		dwc3_otg_stop(dwc);
		dwc3_host_exit(dwc);
		dwc3_gadget_exit(dwc);
		dwc3_otg_exit(dwc);
		break;
	default:
		/* do nothing */
		break;
	}
}

#define DWC3_ALIGN_MASK		(16 - 1)

static int dwc3_probe(struct platform_device *pdev)
{
	struct device		*dev = &pdev->dev;
	struct dwc3_platform_data *pdata = dev_get_platdata(dev);
	struct device_node	*node = dev->of_node;
	struct resource		*res;
	struct dwc3		*dwc;

	int			ret;

	void __iomem		*regs;
	void			*mem;

	mem = devm_kzalloc(dev, sizeof(*dwc) + DWC3_ALIGN_MASK, GFP_KERNEL);
	if (!mem)
		return -ENOMEM;

	dwc = PTR_ALIGN(mem, DWC3_ALIGN_MASK + 1);
	dwc->mem = mem;
	dwc->dev = dev;

	res = platform_get_resource(pdev, IORESOURCE_IRQ, 0);
	if (!res) {
		dev_err(dev, "missing IRQ\n");
		return -ENODEV;
	}
	dwc->xhci_resources[1].start = res->start;
	dwc->xhci_resources[1].end = res->end;
	dwc->xhci_resources[1].flags = res->flags;
	dwc->xhci_resources[1].name = res->name;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		dev_err(dev, "missing memory resource\n");
		return -ENODEV;
	}

	dwc->xhci_resources[0].start = res->start;
	dwc->xhci_resources[0].end = dwc->xhci_resources[0].start +
					DWC3_XHCI_REGS_END;
	dwc->xhci_resources[0].flags = res->flags;
	dwc->xhci_resources[0].name = res->name;

	res->start += DWC3_GLOBALS_REGS_START;

	/*
	 * Request memory region but exclude xHCI regs,
	 * since it will be requested by the xhci-plat driver.
	 */
	regs = devm_ioremap_resource(dev, res);
	if (IS_ERR(regs))
		return PTR_ERR(regs);

	dwc->regs	= regs;
	dwc->regs_size	= resource_size(res);
	/*
	 * restore res->start back to its original value so that,
	 * in case the probe is deferred, we don't end up getting error in
	 * request the memory region the next time probe is called.
	 */
	res->start -= DWC3_GLOBALS_REGS_START;

	if (node) {
		dwc->maximum_speed = of_usb_get_maximum_speed(node);

		dwc->needs_fifo_resize = of_property_read_bool(node, "tx-fifo-resize");
		dwc->adj_sof_accuracy = of_property_read_bool(node, "adj-sof-accuracy");
		dwc->is_not_vbus_pad = of_property_read_bool(node, "is_not_vbus_pad");
		dwc->dr_mode = of_usb_get_dr_mode(node);
		dwc->suspend_clk_freq = of_usb_get_suspend_clk_freq(node);
	} else if (pdata) {
		dwc->maximum_speed = pdata->maximum_speed;

		dwc->needs_fifo_resize = pdata->tx_fifo_resize;
		dwc->adj_sof_accuracy = pdata->adj_sof_accuracy;
		dwc->is_not_vbus_pad = pdata->is_not_vbus_pad;
		dwc->dr_mode = pdata->dr_mode;
	}

	dev_info(dev, "%s: dr_mode:%d, suspend clock:%dMHz\n", __func__,
			dwc->dr_mode , dwc->suspend_clk_freq/1000000);

	/* default to superspeed if no maximum_speed passed */
	if (dwc->maximum_speed == USB_SPEED_UNKNOWN)
		dwc->maximum_speed = USB_SPEED_SUPER;

	ret = dwc3_core_get_phy(dwc);
	if (ret)
		return ret;

	spin_lock_init(&dwc->lock);
	init_completion(&dwc->disconnect);
	platform_set_drvdata(pdev, dwc);

	dev->dma_mask	= dev->parent->dma_mask;
	dev->dma_parms	= dev->parent->dma_parms;
	dma_set_coherent_mask(dev, dev->parent->coherent_dma_mask);

	pm_runtime_enable(dev);
	pm_runtime_get_sync(dev);
	pm_runtime_forbid(dev);

	dwc3_cache_hwparams(dwc);

	ret = dwc3_alloc_event_buffers(dwc, DWC3_EVENT_BUFFERS_SIZE);
	if (ret) {
		dev_err(dwc->dev, "failed to allocate event buffers\n");
		ret = -ENOMEM;
		goto err0;
	}

	if (IS_ENABLED(CONFIG_USB_DWC3_HOST))
		dwc->dr_mode = USB_DR_MODE_HOST;
	else if (IS_ENABLED(CONFIG_USB_DWC3_GADGET))
		dwc->dr_mode = USB_DR_MODE_PERIPHERAL;

	if (dwc->dr_mode == USB_DR_MODE_UNKNOWN)
		dwc->dr_mode = USB_DR_MODE_OTG;

	ret = dwc3_core_init(dwc);
	if (ret) {
		dev_err(dev, "failed to initialize core\n");
		goto err0;
	}

	usb_phy_set_suspend(dwc->usb2_phy, 0);
	usb_phy_set_suspend(dwc->usb3_phy, 0);

	ret = dwc3_event_buffers_setup(dwc);
	if (ret) {
		dev_err(dwc->dev, "failed to setup event buffers\n");
		goto err1;
	}

	ret = dwc3_core_init_mode(dwc);
	if (ret)
		goto err2;

	ret = dwc3_debugfs_init(dwc);
	if (ret) {
		dev_err(dev, "failed to initialize debugfs\n");
		goto err3;
	}

	pm_runtime_allow(dev);

	return 0;

err3:
	dwc3_core_exit_mode(dwc);

err2:
	dwc3_event_buffers_cleanup(dwc);

err1:
	usb_phy_set_suspend(dwc->usb2_phy, 1);
	usb_phy_set_suspend(dwc->usb3_phy, 1);
	dwc3_core_exit(dwc);

err0:
	dwc3_free_event_buffers(dwc);

	return ret;
}

static int dwc3_remove(struct platform_device *pdev)
{
	struct dwc3	*dwc = platform_get_drvdata(pdev);

	dwc3_debugfs_exit(dwc);
	dwc3_core_exit_mode(dwc);
	dwc3_event_buffers_cleanup(dwc);
	dwc3_free_event_buffers(dwc);

	usb_phy_set_suspend(dwc->usb2_phy, 1);
	usb_phy_set_suspend(dwc->usb3_phy, 1);
	phy_power_off(dwc->usb2_generic_phy);
	phy_power_off(dwc->usb3_generic_phy);

	dwc3_core_exit(dwc);

	if (dwc->dr_mode != USB_DR_MODE_OTG)
		pm_runtime_put(&pdev->dev);
	pm_runtime_disable(&pdev->dev);

	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int dwc3_prepare(struct device *dev)
{
	struct dwc3	*dwc = dev_get_drvdata(dev);
	unsigned long	flags;

	/* bring to full power */
	pm_runtime_get_sync(dev);

	if (dwc->dr_mode == USB_DR_MODE_OTG)
		dwc3_otg_stop(dwc);

	spin_lock_irqsave(&dwc->lock, flags);

	switch (dwc->dr_mode) {
	case USB_DR_MODE_PERIPHERAL:
		dwc3_gadget_prepare(dwc);
		break;
	case USB_DR_MODE_OTG:
		/* FALLTHROUGH */
	case USB_DR_MODE_HOST:
	default:
		dwc3_event_buffers_cleanup(dwc);
		break;
	}

	spin_unlock_irqrestore(&dwc->lock, flags);

	return 0;
}

static void dwc3_complete(struct device *dev)
{
	struct dwc3	*dwc = dev_get_drvdata(dev);
	unsigned long	flags;

	spin_lock_irqsave(&dwc->lock, flags);

	dwc3_event_buffers_setup(dwc);
	switch (dwc->dr_mode) {
	case USB_DR_MODE_PERIPHERAL:
		dwc3_gadget_complete(dwc);
		break;
	case USB_DR_MODE_OTG:
		/* FALLTHROUGH */
	case USB_DR_MODE_HOST:
	default:
		break;
	}

	spin_unlock_irqrestore(&dwc->lock, flags);

	if (dwc->dr_mode == USB_DR_MODE_OTG)
		dwc3_otg_start(dwc);

	/* Compensate usage count incremented during prepare */
	pm_runtime_put_sync(dev);
}

static int dwc3_suspend(struct device *dev)
{
	struct dwc3	*dwc = dev_get_drvdata(dev);
	unsigned long	flags;

	spin_lock_irqsave(&dwc->lock, flags);

	switch (dwc->dr_mode) {
	case USB_DR_MODE_PERIPHERAL:
		dwc3_gadget_suspend(dwc);
		break;
	case USB_DR_MODE_OTG:
		break;
	case USB_DR_MODE_HOST:
		usb_phy_shutdown(dwc->usb3_phy);
		usb_phy_shutdown(dwc->usb2_phy);
		phy_exit(dwc->usb2_generic_phy);
		phy_exit(dwc->usb3_generic_phy);
		phy_power_off(dwc->usb2_generic_phy);
		phy_power_off(dwc->usb3_generic_phy);
	default:
		/* do nothing */
		break;
	}

	/* backup GCTL only in non-OTG modes */
	if (dwc->dr_mode != USB_DR_MODE_OTG)
		dwc->gctl = dwc3_readl(dwc->regs, DWC3_GCTL);

	spin_unlock_irqrestore(&dwc->lock, flags);

	return 0;
}

static int dwc3_resume(struct device *dev)
{
	struct dwc3	*dwc = dev_get_drvdata(dev);
	unsigned long	flags;
	int		ret;

	spin_lock_irqsave(&dwc->lock, flags);

	if (dwc->dr_mode != USB_DR_MODE_OTG)
		dwc3_writel(dwc->regs, DWC3_GCTL, dwc->gctl);

	switch (dwc->dr_mode) {
	case USB_DR_MODE_PERIPHERAL:
		dwc3_gadget_resume(dwc);
		break;
	case USB_DR_MODE_OTG:
		break;
	case USB_DR_MODE_HOST:
		ret = phy_power_on(dwc->usb3_generic_phy);
		if (ret < 0) {
			spin_unlock_irqrestore(&dwc->lock, flags);
			return ret;
		}
		ret = phy_power_on(dwc->usb2_generic_phy);
		if (ret < 0)
			goto err_usb3phy_power;
		usb_phy_init(dwc->usb3_phy);
		usb_phy_init(dwc->usb2_phy);
		ret = phy_init(dwc->usb3_generic_phy);
		if (ret < 0)
			goto err_usb2phy_power;
		ret = phy_init(dwc->usb2_generic_phy);
		if (ret < 0)
			goto err_usb3phy_init;
	default:
		/* do nothing */
		break;
	}

	spin_unlock_irqrestore(&dwc->lock, flags);

	pm_runtime_disable(dev);
	pm_runtime_set_active(dev);
	pm_runtime_enable(dev);

	return 0;

err_usb3phy_init:
	phy_exit(dwc->usb3_generic_phy);

err_usb2phy_power:
	phy_power_off(dwc->usb2_generic_phy);

err_usb3phy_power:
	phy_power_off(dwc->usb3_generic_phy);

	spin_unlock_irqrestore(&dwc->lock, flags);

	return ret;
}

static const struct dev_pm_ops dwc3_dev_pm_ops = {
	.prepare	= dwc3_prepare,
	.complete	= dwc3_complete,

	SET_SYSTEM_SLEEP_PM_OPS(dwc3_suspend, dwc3_resume)
};

#define DWC3_PM_OPS	&(dwc3_dev_pm_ops)
#else
#define DWC3_PM_OPS	NULL
#endif

#ifdef CONFIG_OF
static const struct of_device_id of_dwc3_match[] = {
	{
		.compatible = "snps,dwc3"
	},
	{
		.compatible = "synopsys,dwc3"
	},
	{ },
};
MODULE_DEVICE_TABLE(of, of_dwc3_match);
#endif

static struct platform_driver dwc3_driver = {
	.probe		= dwc3_probe,
	.remove		= dwc3_remove,
	.driver		= {
		.name	= "dwc3",
		.of_match_table	= of_match_ptr(of_dwc3_match),
		.pm	= DWC3_PM_OPS,
	},
};

module_platform_driver(dwc3_driver);

MODULE_ALIAS("platform:dwc3");
MODULE_AUTHOR("Felipe Balbi <balbi@ti.com>");
MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("DesignWare USB3 DRD Controller Driver");
