/*
 * Synopsys Designware PCIe host controller driver
 *
 * Copyright (C) 2013 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * Author: Jingoo Han <jg1.han@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/hardirq.h>
#include <linux/irq.h>
#include <linux/irqdomain.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/msi.h>
#include <linux/of_address.h>
#include <linux/of_pci.h>
#include <linux/pci.h>
#include <linux/pci_regs.h>
#include <linux/platform_device.h>
#include <linux/types.h>
#include <linux/exynos-pci-ctrl.h>

#include "pcie-designware.h"
#include "pci-exynos.h"

/* Synopsis specific PCIE configuration registers */
#define PM_CAP_ID_OFFSET		0x40
#define EXP_CAP_ID_OFFSET		0x70
#define PCI_EXP_LNKCAP_MLW_X1		(0x1 << 4)
#define PCI_EXP_LNKCAP_L1EL_64USEC	(0x7 << 15)
#define PCI_EXP_LNKCTL2_TLS		0xf
#define PCI_EXP_LNKCTL2_TLS_2_5GB	0x1
#define PCIE_LINK_L1SS_CONTROL		0x158
#define PORT_LINK_TCOMMON_32US		(0x20 << 8)
#define PCIE_LINK_L1SS_CONTROL2		0x15C
#define PORT_LINK_L1SS_ENABLE		(0xf << 0)
#define PORT_LINK_TPOWERON_130US	(0x69 << 0)
#define PORT_LINK_TPOWERON_3100US	(0xfa << 0)
#define PCIE_LINK_L1SS_OFF		0xb44
#define PORT_LINK_L1SS_T_PCLKACK	(0x3 << 6)
#define PORT_LINK_L1SS_T_L1_2		(0x4 << 2)
#define PORT_LINK_L1SS_T_POWER_OFF	(0x2 << 0)
#define PCIE_ACK_F_ASPM_CONTROL		0x70C
#define PCIE_PORT_LINK_CONTROL		0x710
#define PORT_LINK_MODE_MASK		(0x3f << 16)
#define PORT_LINK_MODE_1_LANES		(0x1 << 16)
#define PORT_LINK_MODE_2_LANES		(0x3 << 16)
#define PORT_LINK_MODE_4_LANES		(0x7 << 16)

#define PCIE_LINK_WIDTH_SPEED_CONTROL	0x80C
#define PORT_LOGIC_SPEED_CHANGE		(0x1 << 17)
#define PORT_LOGIC_LINK_WIDTH_MASK	(0x1ff << 8)
#define PORT_LOGIC_LINK_WIDTH_1_LANES	(0x1 << 8)
#define PORT_LOGIC_LINK_WIDTH_2_LANES	(0x2 << 8)
#define PORT_LOGIC_LINK_WIDTH_4_LANES	(0x4 << 8)

#define PCIE_MSI_ADDR_LO		0x820
#define PCIE_MSI_ADDR_HI		0x824
#define PCIE_MSI_INTR0_ENABLE		0x828
#define PCIE_MSI_INTR0_MASK		0x82C
#define PCIE_MSI_INTR0_STATUS		0x830

#define PCIE_MISC_CONTROL		0x8BC
#define DBI_RO_WR_EN			0x1

#define PCIE_ATU_VIEWPORT		0x900
#define PCIE_ATU_REGION_INBOUND		(0x1 << 31)
#define PCIE_ATU_REGION_OUTBOUND	(0x0 << 31)
#define PCIE_ATU_REGION_INDEX2		(0x2 << 0)
#define PCIE_ATU_REGION_INDEX1		(0x1 << 0)
#define PCIE_ATU_REGION_INDEX0		(0x0 << 0)
#define PCIE_ATU_CR1			0x904
#define PCIE_ATU_TYPE_MEM		(0x0 << 0)
#define PCIE_ATU_TYPE_IO		(0x2 << 0)
#define PCIE_ATU_TYPE_CFG0		(0x4 << 0)
#define PCIE_ATU_TYPE_CFG1		(0x5 << 0)
#define PCIE_ATU_CR2			0x908
#define PCIE_ATU_ENABLE			(0x1 << 31)
#define PCIE_ATU_BAR_MODE_ENABLE	(0x1 << 30)
#define PCIE_ATU_LOWER_BASE		0x90C
#define PCIE_ATU_UPPER_BASE		0x910
#define PCIE_ATU_LIMIT			0x914
#define PCIE_ATU_LOWER_TARGET		0x918
#define PCIE_ATU_BUS(x)			(((x) & 0xff) << 24)
#define PCIE_ATU_DEV(x)			(((x) & 0x1f) << 19)
#define PCIE_ATU_FUNC(x)		(((x) & 0x7) << 16)
#define PCIE_ATU_UPPER_TARGET		0x91C

#define PCIE_AUX_CLK_FREQ_OFF		0xB40
#define PCIE_AUX_CLK_FREQ_24MHZ		0x18
#define PCIE_AUX_CLK_FREQ_26MHZ		0x1A
#define PCIE_L1_SUBSTATES_OFF		0xB44

static struct pci_ops dw_pcie_ops;

static unsigned long global_io_offset;

int dw_pcie_cfg_read(void __iomem *addr, int where, int size, u32 *val)
{
	*val = readl(addr);

	if (size == 1)
		*val = (*val >> (8 * (where & 3))) & 0xff;
	else if (size == 2)
		*val = (*val >> (8 * (where & 3))) & 0xffff;
	else if (size != 4)
		return PCIBIOS_BAD_REGISTER_NUMBER;

	return PCIBIOS_SUCCESSFUL;
}

int dw_pcie_cfg_write(void __iomem *addr, int where, int size, u32 val)
{
	if (size == 4)
		writel(val, addr);
	else if (size == 2)
		writew(val, addr + (where & 2));
	else if (size == 1)
		writeb(val, addr + (where & 3));
	else
		return PCIBIOS_BAD_REGISTER_NUMBER;

	return PCIBIOS_SUCCESSFUL;
}

static int dw_pcie_rd_own_conf(struct pcie_port *pp, int where, int size,
			       u32 *val)
{
	int ret;

	if (pp->ops->rd_own_conf)
		ret = pp->ops->rd_own_conf(pp, where, size, val);
	else
		ret = dw_pcie_cfg_read(pp->dbi_base + (where & ~0x3), where,
				size, val);

	return ret;
}

static int dw_pcie_wr_own_conf(struct pcie_port *pp, int where, int size,
			       u32 val)
{
	int ret;

	if (pp->ops->wr_own_conf)
		ret = pp->ops->wr_own_conf(pp, where, size, val);
	else
		ret = dw_pcie_cfg_write(pp->dbi_base + (where & ~0x3), where,
				size, val);

	return ret;
}

static struct irq_chip dw_msi_irq_chip = {
	.name = "PCI-MSI",
	.irq_enable = unmask_msi_irq,
	.irq_disable = mask_msi_irq,
	.irq_mask = mask_msi_irq,
	.irq_unmask = unmask_msi_irq,
};

/* MSI int handler */
irqreturn_t dw_handle_msi_irq(struct pcie_port *pp)
{
	unsigned long val;
	unsigned long flags;
	int i, pos, irq;
	irqreturn_t ret = IRQ_NONE;

	for (i = 0; i < MAX_MSI_CTRLS; i++) {
		spin_lock_irqsave(&pp->conf_lock, flags);
		dw_pcie_rd_own_conf(pp, PCIE_MSI_INTR0_STATUS + i * 12, 4,
				(u32 *)&val);
		spin_unlock_irqrestore(&pp->conf_lock, flags);
		if (val) {
			ret = IRQ_HANDLED;
			pos = 0;
			while ((pos = find_next_bit(&val, 32, pos)) != 32) {
				irq = irq_find_mapping(pp->irq_domain,
						i * 32 + pos);
				spin_lock_irqsave(&pp->conf_lock, flags);
				dw_pcie_wr_own_conf(pp,
						PCIE_MSI_INTR0_STATUS + i * 12,
						4, 1 << pos);
				spin_unlock_irqrestore(&pp->conf_lock, flags);
				generic_handle_irq(irq);
				pos++;
			}
		}
	}

	return ret;
}

void dw_pcie_msi_init(struct pcie_port *pp)
{
	unsigned long flags;

	pp->msi_data = __get_free_pages(GFP_KERNEL, 0);

	/* program the msi_data */
	spin_lock_irqsave(&pp->conf_lock, flags);
	dw_pcie_wr_own_conf(pp, PCIE_MSI_ADDR_LO, 4,
			virt_to_phys((void *)pp->msi_data));
	dw_pcie_wr_own_conf(pp, PCIE_MSI_ADDR_HI, 4, 0);
	spin_unlock_irqrestore(&pp->conf_lock, flags);
}

static void dw_pcie_msi_clear_irq(struct pcie_port *pp, int irq)
{
	unsigned int res, bit, val;
	unsigned long flags;

	res = (irq / 32) * 12;
	bit = irq % 32;
	spin_lock_irqsave(&pp->conf_lock, flags);
	dw_pcie_rd_own_conf(pp, PCIE_MSI_INTR0_ENABLE + res, 4, &val);
	val &= ~(1 << bit);
	dw_pcie_wr_own_conf(pp, PCIE_MSI_INTR0_ENABLE + res, 4, val);
	spin_unlock_irqrestore(&pp->conf_lock, flags);
}

static void clear_irq_range(struct pcie_port *pp, unsigned int irq_base,
			    unsigned int nvec, unsigned int pos)
{
	unsigned int i;

	for (i = 0; i < nvec; i++) {
		irq_set_msi_desc_off(irq_base, i, NULL);
		/* Disable corresponding interrupt on MSI controller */
		if (pp->ops->msi_clear_irq)
			pp->ops->msi_clear_irq(pp, pos + i);
		else
			dw_pcie_msi_clear_irq(pp, pos + i);
	}

	bitmap_release_region(pp->msi_irq_in_use, pos, order_base_2(nvec));
}

static void dw_pcie_msi_set_irq(struct pcie_port *pp, int irq)
{
	unsigned int res, bit, val;
	unsigned long flags;

	res = (irq / 32) * 12;
	bit = irq % 32;
	spin_lock_irqsave(&pp->conf_lock, flags);
	dw_pcie_rd_own_conf(pp, PCIE_MSI_INTR0_ENABLE + res, 4, &val);
	val |= 1 << bit;
	dw_pcie_wr_own_conf(pp, PCIE_MSI_INTR0_ENABLE + res, 4, val);
	spin_unlock_irqrestore(&pp->conf_lock, flags);
}

static int assign_irq(int no_irqs, struct msi_desc *desc, int *pos)
{
	int irq, pos0, i;
	struct pcie_port *pp = desc->dev->bus->sysdata;

	pos0 = bitmap_find_free_region(pp->msi_irq_in_use, MAX_MSI_IRQS,
				       order_base_2(no_irqs));
	if (pos0 < 0)
		goto no_valid_irq;

	irq = irq_find_mapping(pp->irq_domain, pos0);
	if (!irq)
		goto no_valid_irq;

	/*
	 * irq_create_mapping (called from dw_pcie_host_init) pre-allocates
	 * descs so there is no need to allocate descs here. We can therefore
	 * assume that if irq_find_mapping above returns non-zero, then the
	 * descs are also successfully allocated.
	 */

	for (i = 0; i < no_irqs; i++) {
		if (irq_set_msi_desc_off(irq, i, desc) != 0) {
			clear_irq_range(pp, irq, i, pos0);
			goto no_valid_irq;
		}
		/*Enable corresponding interrupt in MSI interrupt controller */
		if (pp->ops->msi_set_irq)
			pp->ops->msi_set_irq(pp, pos0 + i);
		else
			dw_pcie_msi_set_irq(pp, pos0 + i);
	}

	*pos = pos0;
	return irq;

no_valid_irq:
	*pos = pos0;
	return -ENOSPC;
}

static int dw_msi_setup_irq(struct msi_chip *chip, struct pci_dev *pdev,
			struct msi_desc *desc)
{
	int irq, pos;
	struct msi_msg msg;
	struct pcie_port *pp = pdev->bus->sysdata;

	if (desc->msi_attrib.is_msix)
		return -EINVAL;

	if (desc->msi_attrib.is_msix)
		return -EINVAL;

	irq = assign_irq(1, desc, &pos);
	if (irq < 0)
		return irq;

	if (pp->ops->get_msi_addr)
		msg.address_lo = pp->ops->get_msi_addr(pp);
	else
		msg.address_lo = virt_to_phys((void *)pp->msi_data);
	msg.address_hi = 0x0;

	if (pp->ops->get_msi_data)
		msg.data = pp->ops->get_msi_data(pp, pos);
	else
		msg.data = pos;

#ifdef CONFIG_PCI_MSI
	write_msi_msg(irq, &msg);
#endif

	return 0;
}

static void dw_msi_teardown_irq(struct msi_chip *chip, unsigned int irq)
{
	struct irq_data *data = irq_get_irq_data(irq);
	struct msi_desc *msi = irq_data_get_msi(data);
	struct pcie_port *pp = msi->dev->bus->sysdata;

	clear_irq_range(pp, irq, 1, data->hwirq);
}

static struct msi_chip dw_pcie_msi_chip = {
	.setup_irq = dw_msi_setup_irq,
	.teardown_irq = dw_msi_teardown_irq,
};

int dw_pcie_link_up(struct pcie_port *pp)
{
	if (pp == NULL)
		return 0;
	if (pp->ops == NULL)
		return 0;
	if (pp->ops->link_up)
		return pp->ops->link_up(pp);
	else
		return 0;
}

void dw_pcie_config_l1ss(struct pcie_port *pp)
{
	struct exynos_pcie *exynos_pcie = to_exynos_pcie(pp);
	u32 val;
	void __iomem *ep_dbi_base = pp->va_cfg0_base;
	u32 exp_cap_off = EXP_CAP_ID_OFFSET;

	if(exynos_pcie->l1ss_ctrl_id_state == 0) { 
		/* Enable L1SS on Root Complex */
		val = readl(ep_dbi_base + 0xbc);
		val &= ~0x3;
		val |= 0x142;
		writel(val, ep_dbi_base + 0xBC);
		val = readl(ep_dbi_base + 0x248);
		writel(val | 0xa0f, ep_dbi_base + 0x248);
		dev_err(pp->dev, "l1ss enabled(0x%x)\n", exynos_pcie->l1ss_ctrl_id_state);
	} else { 
		/* disable L1SS on Root Complex */
		val = readl(ep_dbi_base + 0xbc);
		writel(val & ~0x3, ep_dbi_base + 0xBC);
		val = readl(ep_dbi_base + 0x248);
		writel(val & ~0xf, ep_dbi_base + 0x248);
		dev_err(pp->dev, "l1ss disabled(0x%x)\n", exynos_pcie->l1ss_ctrl_id_state);
	}

	writel(PORT_LINK_TPOWERON_130US, ep_dbi_base + 0x24C);
	writel(0x10031003, ep_dbi_base + 0x1B4);
	val = readl(ep_dbi_base + 0xD4);
	writel(val | (1 << 10), ep_dbi_base + 0xD4);

	dw_pcie_rd_own_conf(pp, PCIE_LINK_L1SS_CONTROL, 4, &val);
	val |= PORT_LINK_TCOMMON_32US | PORT_LINK_L1SS_ENABLE;
	dw_pcie_wr_own_conf(pp, PCIE_LINK_L1SS_CONTROL, 4, val);
	dw_pcie_wr_own_conf(pp, PCIE_LINK_L1SS_CONTROL2, 4, PORT_LINK_TPOWERON_130US);
	val = PORT_LINK_L1SS_T_PCLKACK | PORT_LINK_L1SS_T_L1_2 |
	      PORT_LINK_L1SS_T_POWER_OFF;
	dw_pcie_wr_own_conf(pp, PCIE_LINK_L1SS_OFF, 4, val);

	dw_pcie_rd_own_conf(pp, exp_cap_off + PCI_EXP_LNKCTL, 4, &val);
	val &= ~PCI_EXP_LNKCTL_ASPMC;
	val |= PCI_EXP_LNKCTL_CCC | PCI_EXP_LNKCTL_ASPM_L1;
	dw_pcie_wr_own_conf(pp, exp_cap_off + PCI_EXP_LNKCTL, 4, val);
	dw_pcie_wr_own_conf(pp, exp_cap_off + PCI_EXP_DEVCTL2, 4, PCI_EXP_DEVCTL2_LTR_EN);
}

static int dw_pcie_msi_map(struct irq_domain *domain, unsigned int irq,
			irq_hw_number_t hwirq)
{
	irq_set_chip_and_handler(irq, &dw_msi_irq_chip, handle_simple_irq);
	irq_set_chip_data(irq, domain->host_data);
	set_irq_flags(irq, IRQF_VALID);

	return 0;
}

static const struct irq_domain_ops msi_domain_ops = {
	.map = dw_pcie_msi_map,
};

int dw_pcie_host_init(struct pcie_port *pp)
{
	struct device_node *np = pp->dev->of_node;
	struct platform_device *pdev = to_platform_device(pp->dev);
	struct of_pci_range range;
	struct of_pci_range_parser parser;
	struct resource *cfg_res;
	u32 na, ns;
	const __be32 *addrp;
	int i, index, ret;

	/* Find the address cell size and the number of cells in order to get
	 * the untranslated address.
	 */
	of_property_read_u32(np, "#address-cells", &na);
	ns = of_n_size_cells(np);

	cfg_res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "config");
	if (cfg_res) {
		pp->cfg0_size = resource_size(cfg_res)/2;
		pp->cfg1_size = resource_size(cfg_res)/2;
		pp->cfg0_base = cfg_res->start;
		pp->cfg1_base = cfg_res->start + pp->cfg0_size;

		/* Find the untranslated configuration space address */
		index = of_property_match_string(np, "reg-names", "config");
		addrp = of_get_address(np, index, NULL, NULL);
		pp->cfg0_mod_base = of_read_number(addrp, of_n_addr_cells(np));
		pp->cfg1_mod_base = pp->cfg0_mod_base + pp->cfg0_size;
	} else {
		dev_err(pp->dev, "missing *config* reg space\n");
	}

	if (of_pci_range_parser_init(&parser, np)) {
		dev_err(pp->dev, "missing ranges property\n");
		return -EINVAL;
	}

	/* Get the I/O and memory ranges from DT */
	for_each_of_pci_range(&parser, &range) {
		unsigned long restype = range.flags & IORESOURCE_TYPE_BITS;
		if (restype == IORESOURCE_IO) {
			of_pci_range_to_resource(&range, np, &pp->io);
			pp->io.name = "I/O";
			pp->io.start = max_t(resource_size_t,
					     PCIBIOS_MIN_IO,
					     range.pci_addr + global_io_offset);
			pp->io.end = min_t(resource_size_t,
					   IO_SPACE_LIMIT,
					   range.pci_addr + range.size
					   + global_io_offset - 1);
			pp->io_size = resource_size(&pp->io);
			pp->io_bus_addr = range.pci_addr;
			pp->io_base = range.cpu_addr;

			/* Find the untranslated IO space address */
			pp->io_mod_base = of_read_number(parser.range -
							 parser.np + na,
							 of_n_addr_cells(np));
		}
		if (restype == IORESOURCE_MEM) {
			of_pci_range_to_resource(&range, np, &pp->mem);
			pp->mem.name = "MEM";
			pp->mem_size = resource_size(&pp->mem);
			pp->mem_bus_addr = range.pci_addr;

			/* Find the untranslated MEM space address */
			pp->mem_mod_base = of_read_number(parser.range -
							  parser.np + na,
							  of_n_addr_cells(np));
		}
		if (restype == 0) {
			of_pci_range_to_resource(&range, np, &pp->cfg);
			pp->cfg0_size = resource_size(&pp->cfg)/2;
			pp->cfg1_size = resource_size(&pp->cfg)/2;
			pp->cfg0_base = pp->cfg.start;
			pp->cfg1_base = pp->cfg.start + pp->cfg0_size;

			/* Find the untranslated configuration space address */
			pp->cfg0_mod_base = of_read_number(parser.range -
							   parser.np + na, ns);
			pp->cfg1_mod_base = pp->cfg0_mod_base +
					    pp->cfg0_size;
		}
	}

	ret = of_pci_parse_bus_range(np, &pp->busn);
	if (ret < 0) {
		pp->busn.name = np->name;
		pp->busn.start = 0;
		pp->busn.end = 0xff;
		pp->busn.flags = IORESOURCE_BUS;
		dev_dbg(pp->dev, "failed to parse bus-range property: %d, using default %pR\n",
			ret, &pp->busn);
	}

	if (!pp->dbi_base) {
		pp->dbi_base = devm_ioremap(pp->dev, pp->cfg.start,
					resource_size(&pp->cfg));
		if (!pp->dbi_base) {
			dev_err(pp->dev, "error with ioremap\n");
			return -ENOMEM;
		}
	}

	pp->mem_base = pp->mem.start;

	if (!pp->va_cfg0_base) {
		pp->va_cfg0_base = devm_ioremap(pp->dev, pp->cfg0_base,
						pp->cfg0_size);
		if (!pp->va_cfg0_base) {
			dev_err(pp->dev, "error with ioremap in function\n");
			return -ENOMEM;
		}
	}

	if (!pp->va_cfg1_base) {
		pp->va_cfg1_base = devm_ioremap(pp->dev, pp->cfg1_base,
						pp->cfg1_size);
		if (!pp->va_cfg1_base) {
			dev_err(pp->dev, "error with ioremap\n");
			return -ENOMEM;
		}
	}

	if (of_property_read_u32(np, "num-lanes", &pp->lanes)) {
		dev_err(pp->dev, "Failed to parse the number of lanes\n");
		return -EINVAL;
	}

	if (IS_ENABLED(CONFIG_PCI_MSI)) {
		if (!pp->ops->msi_host_init) {
			pp->irq_domain = irq_domain_add_linear(pp->dev->of_node,
						MAX_MSI_IRQS, &msi_domain_ops,
						&dw_pcie_msi_chip);
			if (!pp->irq_domain) {
				dev_err(pp->dev, "irq domain init failed\n");
				return -ENXIO;
			}

			for (i = 0; i < MAX_MSI_IRQS; i++)
				irq_create_mapping(pp->irq_domain, i);
		} else {
			ret = pp->ops->msi_host_init(pp, &dw_pcie_msi_chip);
			if (ret < 0)
				return ret;
		}
	}

	return 0;
}

int dw_pcie_scan(struct pcie_port *pp)
{
	struct pci_bus *bus;
	LIST_HEAD(res);
	struct device_node *np = pp->dev->of_node;
	u32 val;
	int ret;

	dw_pcie_wr_own_conf(pp, PCI_BASE_ADDRESS_0, 4, 0);

	/* program correct class for RC */
	dw_pcie_wr_own_conf(pp, PCI_CLASS_DEVICE, 2, PCI_CLASS_BRIDGE_PCI);

	dw_pcie_rd_own_conf(pp, PCIE_LINK_WIDTH_SPEED_CONTROL, 4, &val);
	val |= PORT_LOGIC_SPEED_CHANGE;
	dw_pcie_wr_own_conf(pp, PCIE_LINK_WIDTH_SPEED_CONTROL, 4, val);

#ifdef CONFIG_ARM
	/*
	 * FIXME: we should really be able to use
	 * of_pci_get_host_bridge_resources on arm32 as well,
	 * but the conversion needs some more testing
	 */
	if (global_io_offset < SZ_1M && pp->io_size > 0) {
		pci_ioremap_io(global_io_offset, pp->io_base);
		global_io_offset += SZ_64K;
		pci_add_resource_offset(&res, &pp->io,
					global_io_offset - pp->io_bus_addr);
	}
	pci_add_resource_offset(&res, &pp->mem,
				pp->mem.start - pp->mem_bus_addr);
	pci_add_resource(&res, &pp->busn);
#else
	ret = of_pci_get_host_bridge_resources(np, 0, 0xff, &res, &pp->io_base);
	if (ret)
		return ret;
#endif

	bus = pci_create_root_bus(pp->dev, pp->root_bus_nr, &dw_pcie_ops,
			      pp, &res);
	if (!bus)
		return -ENOMEM;

#ifdef CONFIG_GENERIC_MSI_IRQ_DOMAIN
	bus->msi = container_of(&pp->irq_domain, struct msi_controller, domain);
#else
	bus->msi = &dw_pcie_msi_chip;
#endif

	pci_scan_child_bus(bus);
	if (pp->ops->scan_bus)
		pp->ops->scan_bus(pp);

#ifdef CONFIG_ARM
	/* support old dtbs that incorrectly describe IRQs */
	pci_fixup_irqs(pci_common_swizzle, of_irq_parse_and_map_pci);
#endif

	pci_assign_unassigned_bus_resources(bus);
	pci_bus_add_devices(bus);

	dw_pcie_config_l1ss(pp);

	return 0;
}

void dw_pcie_prog_viewport_cfg0(struct pcie_port *pp, u32 busdev)
{
	/* Program viewport 0 : OUTBOUND : CFG0 */
	dw_pcie_wr_own_conf(pp, PCIE_ATU_VIEWPORT, 4,
			    PCIE_ATU_REGION_OUTBOUND | PCIE_ATU_REGION_INDEX0);
	dw_pcie_wr_own_conf(pp, PCIE_ATU_LOWER_BASE, 4, pp->cfg0_mod_base);
	dw_pcie_wr_own_conf(pp, PCIE_ATU_UPPER_BASE, 4, (pp->cfg0_mod_base >> 32));
	dw_pcie_wr_own_conf(pp, PCIE_ATU_LIMIT, 4, pp->cfg0_mod_base + pp->cfg0_size - 1);
	dw_pcie_wr_own_conf(pp, PCIE_ATU_LOWER_TARGET, 4, busdev);
	dw_pcie_wr_own_conf(pp, PCIE_ATU_UPPER_TARGET, 4, 0);
	dw_pcie_wr_own_conf(pp, PCIE_ATU_CR1, 4, PCIE_ATU_TYPE_CFG0);
	dw_pcie_wr_own_conf(pp, PCIE_ATU_CR2, 4, PCIE_ATU_ENABLE);
}

static void dw_pcie_prog_viewport_cfg1(struct pcie_port *pp, u32 busdev)
{
	/* Program viewport 1 : OUTBOUND : CFG1 */
	dw_pcie_wr_own_conf(pp, PCIE_ATU_VIEWPORT, 4,
			    PCIE_ATU_REGION_OUTBOUND | PCIE_ATU_REGION_INDEX2);
	dw_pcie_wr_own_conf(pp, PCIE_ATU_CR1, 4, PCIE_ATU_TYPE_CFG1);
	dw_pcie_wr_own_conf(pp, PCIE_ATU_LOWER_BASE, 4, pp->cfg1_mod_base);
	dw_pcie_wr_own_conf(pp, PCIE_ATU_UPPER_BASE, 4, (pp->cfg1_mod_base >> 32));
	dw_pcie_wr_own_conf(pp, PCIE_ATU_LIMIT, 4, pp->cfg1_mod_base + pp->cfg1_size - 1);
	dw_pcie_wr_own_conf(pp, PCIE_ATU_LOWER_TARGET, 4, busdev);
	dw_pcie_wr_own_conf(pp, PCIE_ATU_UPPER_TARGET, 4, 0);
	dw_pcie_wr_own_conf(pp, PCIE_ATU_CR2, 4, PCIE_ATU_ENABLE);
}

void dw_pcie_prog_viewport_mem_outbound(struct pcie_port *pp)
{
	/* Program viewport 0 : OUTBOUND : MEM */
	dw_pcie_wr_own_conf(pp, PCIE_ATU_VIEWPORT, 4,
			    PCIE_ATU_REGION_OUTBOUND | PCIE_ATU_REGION_INDEX1);
	dw_pcie_wr_own_conf(pp, PCIE_ATU_CR1, 4, PCIE_ATU_TYPE_MEM);
	dw_pcie_wr_own_conf(pp, PCIE_ATU_LOWER_BASE, 4, pp->mem_mod_base);
	dw_pcie_wr_own_conf(pp, PCIE_ATU_UPPER_BASE, 4, (pp->mem_mod_base >> 32));
	dw_pcie_wr_own_conf(pp, PCIE_ATU_LIMIT, 4, pp->mem_mod_base + pp->mem_size - 1);
	dw_pcie_wr_own_conf(pp, PCIE_ATU_LOWER_TARGET, 4, pp->mem_bus_addr);
	dw_pcie_wr_own_conf(pp, PCIE_ATU_UPPER_TARGET, 4, upper_32_bits(pp->mem_bus_addr));
	dw_pcie_wr_own_conf(pp, PCIE_ATU_CR2, 4, PCIE_ATU_ENABLE);
}

static void dw_pcie_prog_viewport_io_outbound(struct pcie_port *pp)
{
	/* Program viewport 1 : OUTBOUND : IO */
	dw_pcie_wr_own_conf(pp, PCIE_ATU_VIEWPORT, 4,
			    PCIE_ATU_REGION_OUTBOUND | PCIE_ATU_REGION_INDEX2);
	dw_pcie_wr_own_conf(pp, PCIE_ATU_CR1, 4, PCIE_ATU_TYPE_IO);
	dw_pcie_wr_own_conf(pp, PCIE_ATU_LOWER_BASE, 4, pp->io_mod_base);
	dw_pcie_wr_own_conf(pp, PCIE_ATU_UPPER_BASE, 4, (pp->io_mod_base >> 32));
	dw_pcie_wr_own_conf(pp, PCIE_ATU_LIMIT, 4, pp->io_mod_base + pp->io_size - 1);
	dw_pcie_wr_own_conf(pp, PCIE_ATU_LOWER_TARGET, 4, pp->io_bus_addr);
	dw_pcie_wr_own_conf(pp, PCIE_ATU_UPPER_TARGET, 4, upper_32_bits(pp->io_bus_addr));
	dw_pcie_wr_own_conf(pp, PCIE_ATU_CR2, 4, PCIE_ATU_ENABLE);
}

static int dw_pcie_rd_other_conf(struct pcie_port *pp, struct pci_bus *bus,
		u32 devfn, int where, int size, u32 *val)
{
	int ret = PCIBIOS_SUCCESSFUL;
	u32 address, busdev;

	busdev = PCIE_ATU_BUS(bus->number) | PCIE_ATU_DEV(PCI_SLOT(devfn)) |
		 PCIE_ATU_FUNC(PCI_FUNC(devfn));
	address = where & ~0x3;

	if (bus->parent->number == pp->root_bus_nr) {
		dw_pcie_prog_viewport_cfg0(pp, busdev);
		ret = dw_pcie_cfg_read(pp->va_cfg0_base + address, where, size,
				val);
		dw_pcie_prog_viewport_mem_outbound(pp);
	} else {
		dw_pcie_prog_viewport_cfg1(pp, busdev);
		ret = dw_pcie_cfg_read(pp->va_cfg1_base + address, where, size,
				val);
		dw_pcie_prog_viewport_io_outbound(pp);
	}

	return ret;
}

static int dw_pcie_wr_other_conf(struct pcie_port *pp, struct pci_bus *bus,
		u32 devfn, int where, int size, u32 val)
{
	int ret = PCIBIOS_SUCCESSFUL;
	u32 address, busdev;

	busdev = PCIE_ATU_BUS(bus->number) | PCIE_ATU_DEV(PCI_SLOT(devfn)) |
		 PCIE_ATU_FUNC(PCI_FUNC(devfn));
	address = where & ~0x3;

	if (bus->parent->number == pp->root_bus_nr) {
		dw_pcie_prog_viewport_cfg0(pp, busdev);
		ret = dw_pcie_cfg_write(pp->va_cfg0_base + address, where, size,
				val);
		dw_pcie_prog_viewport_mem_outbound(pp);
	} else {
		dw_pcie_prog_viewport_cfg1(pp, busdev);
		ret = dw_pcie_cfg_write(pp->va_cfg1_base + address, where, size,
				val);
		dw_pcie_prog_viewport_io_outbound(pp);
	}

	return ret;
}

static int dw_pcie_valid_config(struct pcie_port *pp,
				struct pci_bus *bus, int dev)
{
	struct exynos_pcie *exynos_pcie = to_exynos_pcie(pp);

	if (exynos_pcie->state != STATE_LINK_UP)
		return 0;

	/* If there is no link, then there is no device */
	if (bus->number != pp->root_bus_nr) {
		if (!dw_pcie_link_up(pp))
			return 0;
	}

	/* access only one slot on each root port */
	if (bus->number == pp->root_bus_nr && dev > 0)
		return 0;

	/*
	 * do not read more than one device on the bus directly attached
	 * to RC's (Virtual Bridge's) DS side.
	 */
	if (bus->primary == pp->root_bus_nr && dev > 0)
		return 0;

	return 1;
}

static int dw_pcie_rd_conf(struct pci_bus *bus, u32 devfn, int where,
			int size, u32 *val)
{
	struct pcie_port *pp = bus->sysdata;
	struct exynos_pcie *exynos_pcie = to_exynos_pcie(pp);
	unsigned long flags;
	int ret;

	if (exynos_pcie->state == STATE_LINK_DOWN){
		if( (where > (long unsigned int)(pp->io_base - pp->cfg0_base)) && (where < pp->io_size)) {
			dev_err(pp->dev, "%s: PCIe link state is %d\n", __func__, exynos_pcie->state);
			dev_err(pp->dev, "%s: Can not access in this state\n", __func__);
		}
		return PCIBIOS_DEVICE_NOT_FOUND;
	}

	if (where > pp->cfg0_size) {
		dev_err(pp->dev, "%s: where 0x%x value exceeds!\n", __func__, where);
		dev_err(pp->dev, "%s: bus->number: 0x%x, pp->root_bus_nr: 0x%x\n",
			__func__, bus->number, pp->root_bus_nr);
		return PCIBIOS_DEVICE_NOT_FOUND;
	}

	spin_lock_irqsave(&pp->conf_lock, flags);
	if (dw_pcie_valid_config(pp, bus, PCI_SLOT(devfn)) == 0) {
		spin_unlock_irqrestore(&pp->conf_lock, flags);
		*val = 0xffffffff;
		return PCIBIOS_DEVICE_NOT_FOUND;
	}

	if (bus->number != pp->root_bus_nr)
		if (pp->ops->rd_other_conf)
			ret = pp->ops->rd_other_conf(pp, bus, devfn,
						where, size, val);
		else
			ret = dw_pcie_rd_other_conf(pp, bus, devfn,
						where, size, val);
	else
		ret = dw_pcie_rd_own_conf(pp, where, size, val);
	spin_unlock_irqrestore(&pp->conf_lock, flags);

	return ret;
}

static int dw_pcie_wr_conf(struct pci_bus *bus, u32 devfn,
			int where, int size, u32 val)
{
	struct pcie_port *pp = bus->sysdata;
	struct exynos_pcie *exynos_pcie = to_exynos_pcie(pp);
	unsigned long flags;
	int ret;

	if (exynos_pcie->state == STATE_LINK_DOWN) {
		if( (where > (long unsigned int)(pp->io_base - pp->cfg0_base)) && (where < pp->io_size)) {
			dev_err(pp->dev, "%s: PCIe link state is %d\n", __func__, exynos_pcie->state);
			dev_err(pp->dev, "%s: Can not access in this state\n", __func__);
		}
		return PCIBIOS_DEVICE_NOT_FOUND;
	}

	if (where > pp->cfg0_size) {
		dev_err(pp->dev, "%s: where 0x%x value exceeds!\n", __func__, where);
		dev_err(pp->dev, "%s: bus->number: 0x%x, pp->root_bus_nr: 0x%x\n",
			__func__, bus->number, pp->root_bus_nr);
		return PCIBIOS_DEVICE_NOT_FOUND;
	}

	spin_lock_irqsave(&pp->conf_lock, flags);
	if (dw_pcie_valid_config(pp, bus, PCI_SLOT(devfn)) == 0) {
		spin_unlock_irqrestore(&pp->conf_lock, flags);
		return PCIBIOS_DEVICE_NOT_FOUND;
	}

	if (bus->number != pp->root_bus_nr)
		if (pp->ops->wr_other_conf)
			ret = pp->ops->wr_other_conf(pp, bus, devfn,
						where, size, val);
		else
			ret = dw_pcie_wr_other_conf(pp, bus, devfn,
						where, size, val);
	else
		ret = dw_pcie_wr_own_conf(pp, where, size, val);
	spin_unlock_irqrestore(&pp->conf_lock, flags);

	return ret;
}

static struct pci_ops dw_pcie_ops = {
	.read = dw_pcie_rd_conf,
	.write = dw_pcie_wr_conf,
};

void dw_pcie_setup_rc(struct pcie_port *pp)
{
	struct exynos_pcie *exynos_pcie = to_exynos_pcie(pp);
	u32 val;
	u32 membase;
	u32 memlimit;
	u32 exp_cap_off = EXP_CAP_ID_OFFSET;
	u32 pm_cap_off = PM_CAP_ID_OFFSET;

	/* enable writing to DBI read-only registers */
	dw_pcie_wr_own_conf(pp, PCIE_MISC_CONTROL, 4, DBI_RO_WR_EN);

	/* change vendor ID and device ID for PCIe */
	dw_pcie_wr_own_conf(pp, PCI_VENDOR_ID, 2, PCI_VENDOR_ID_SAMSUNG);
	dw_pcie_wr_own_conf(pp, PCI_DEVICE_ID, 2,
			    PCI_DEVICE_ID_EXYNOS + exynos_pcie->ch_num);

	/* set the number of lanes */
	dw_pcie_rd_own_conf(pp, PCIE_PORT_LINK_CONTROL, 4, &val);
	val &= ~PORT_LINK_MODE_MASK;
	switch (pp->lanes) {
	case 1:
		val |= PORT_LINK_MODE_1_LANES;
		break;
	case 2:
		val |= PORT_LINK_MODE_2_LANES;
		break;
	case 4:
		val |= PORT_LINK_MODE_4_LANES;
		break;
	}
	dw_pcie_wr_own_conf(pp, PCIE_PORT_LINK_CONTROL, 4, val);

	/* set link width speed control register */
	dw_pcie_rd_own_conf(pp, PCIE_LINK_WIDTH_SPEED_CONTROL, 4, &val);
	val &= ~PORT_LOGIC_LINK_WIDTH_MASK;
	switch (pp->lanes) {
	case 1:
		val |= PORT_LOGIC_LINK_WIDTH_1_LANES;
		break;
	case 2:
		val |= PORT_LOGIC_LINK_WIDTH_2_LANES;
		break;
	case 4:
		val |= PORT_LOGIC_LINK_WIDTH_4_LANES;
		break;
	}
	dw_pcie_wr_own_conf(pp, PCIE_LINK_WIDTH_SPEED_CONTROL, 4, val);

	/* set max link width & speed : Gen2, Lane1 */
	dw_pcie_rd_own_conf(pp, exp_cap_off + PCI_EXP_LNKCAP, 4, &val);
	val &= ~(PCI_EXP_LNKCAP_L1EL|PCI_EXP_LNKCAP_MLW|PCI_EXP_LNKCAP_SLS);
	val |= PCI_EXP_LNKCAP_L1EL_64USEC|PCI_EXP_LNKCAP_MLW_X1|PCI_EXP_LNKCAP_SLS_5_0GB;
	dw_pcie_wr_own_conf(pp, exp_cap_off + PCI_EXP_LNKCAP, 4, val);

	/* set auxiliary clock frequency: 26MHz */
	dw_pcie_wr_own_conf(pp, PCIE_AUX_CLK_FREQ_OFF, 4, PCIE_AUX_CLK_FREQ_26MHZ);

	/* set duration of L1.2 & L1.2.Entry */
	dw_pcie_wr_own_conf(pp, PCIE_L1_SUBSTATES_OFF, 4, 0xD2);

	/* clear power management control and status register */
	dw_pcie_wr_own_conf(pp, pm_cap_off + PCI_PM_CTRL, 4, 0x0);

	/* setup RC BARs */
	dw_pcie_wr_own_conf(pp, PCI_BASE_ADDRESS_0, 4, 0x00000004);
	dw_pcie_wr_own_conf(pp, PCI_BASE_ADDRESS_1, 4, 0x00000000);

	/* setup bus numbers */
	dw_pcie_rd_own_conf(pp, PCI_PRIMARY_BUS, 4, &val);
	val &= 0xff000000;
	val |= 0x00010100;
	dw_pcie_wr_own_conf(pp, PCI_PRIMARY_BUS, 4, val);

	/* setup memory base, memory limit */
	membase = ((u32)pp->mem_base & 0xfff00000) >> 16;
	memlimit = (pp->mem_size + (u32)pp->mem_base) & 0xfff00000;
	val = memlimit | membase;
	dw_pcie_wr_own_conf(pp, PCI_MEMORY_BASE, 4, val);

	/* setup command register */
	dw_pcie_rd_own_conf(pp, PCI_COMMAND, 4, &val);
	val &= 0xffff0000;
	val |= PCI_COMMAND_IO | PCI_COMMAND_MEMORY |
		PCI_COMMAND_MASTER | PCI_COMMAND_SERR;
	dw_pcie_wr_own_conf(pp, PCI_COMMAND, 4, val);

	/* initiate link retraining */
	dw_pcie_rd_own_conf(pp, exp_cap_off + PCI_EXP_LNKCTL, 4, &val);
	val |= PCI_EXP_LNKCTL_RL;
	dw_pcie_wr_own_conf(pp, exp_cap_off + PCI_EXP_LNKCTL, 4, val);

	/* set target speed to GEN1 only */
	dw_pcie_rd_own_conf(pp, exp_cap_off + PCI_EXP_LNKCTL2, 4, &val);
	val &= ~PCI_EXP_LNKCTL2_TLS;
	val |= PCI_EXP_LNKCTL2_TLS_2_5GB;
	dw_pcie_wr_own_conf(pp, exp_cap_off + PCI_EXP_LNKCTL2, 4, val);
}

MODULE_AUTHOR("Jingoo Han <jg1.han@samsung.com>");
MODULE_DESCRIPTION("Designware PCIe host controller driver");
MODULE_LICENSE("GPL v2");
