// SPDX-License-Identifier: GPL-2.0

/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/cpu_pm.h>
#include <linux/spinlock.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/irq.h>
#include <linux/irqdesc.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#include <linux/io.h>

#include "mtk_spm.h"
#include "mtk_spm_irq.h"
#include "mtk_spm_internal.h"

#define GICD_CTLR		0x0000
#define GICD_CTLR_RWP		BIT(31)
#define GICD_TYPER		0x0004
#define GICD_ISENABLER		0x0100
#define GICD_ICENABLER		0x0180
#define GICD_ISPENDR		0x0200
#define GICD_ICFGR		0x0c00
#define GIC_MAX_EN_REGS		16

#define CIRQ_STA_BASE		0x000
#define CIRQ_ACK_BASE		0x040
#define CIRQ_MASK_SET_BASE	0x0c0
#define CIRQ_MASK_CLR_BASE	0x100
#define CIRQ_SENS_SET_BASE	0x180
#define CIRQ_SENS_CLR_BASE	0x1c0
#define CIRQ_POL_SET_BASE	0x240
#define CIRQ_POL_CLR_BASE	0x280
#define CIRQ_CON		0x300
#define CIRQ_CON_EN		BIT(0)
#define CIRQ_CON_BITS_MASK	0x7

static inline unsigned int virq_to_hwirq(unsigned int virq)
{
	struct irq_desc *desc;
	unsigned int hwirq;

	desc = irq_to_desc(virq);
	WARN_ON(!desc);
	hwirq = desc ? desc->irq_data.hwirq : 0;
	return hwirq;
}

char __attribute__((weak)) *spm_vcorefs_dump_dvfs_regs(char *p)
{
	return NULL;
}

/***************************************************
 * spm edge trigger irq backup/restore
 ***************************************************/

/* edge_trigger_irq_list is defined in header file 'mtk_spm_irq_edge.h' */
#include <mtk_spm_irq_edge.h>

#define IRQ_NUMBER (sizeof(list)/sizeof(struct edge_trigger_irq_list))
static u32 edge_trig_irqs[IRQ_NUMBER];
static u32 edge_trig_hwirqs[IRQ_NUMBER];

static void __iomem *gicd_base;
static void __iomem *sysirq_base;
static void __iomem *cirq_base;
static u32 gicd_en_regs;
static u32 gicd_saved_isenabler[GIC_MAX_EN_REGS];
static u32 cirq_irq_num = 232;
static u32 cirq_spi_start = 64;
static u32 cirq_ctrl_regs = 8;

static void gicd_wait_for_rwp(void)
{
	int count = 1000000;

	if (!gicd_base)
		return;

	while (readl_relaxed(gicd_base + GICD_CTLR) & GICD_CTLR_RWP) {
		if (!--count) {
			printk_deferred("[name:spm&][SPM] GICD RWP timeout\n");
			break;
		}
		cpu_relax();
	}
}

static void mtk_gic_unmask_hwirq(u32 hwirq)
{
	if (!gicd_base || hwirq < 32 || (hwirq / 32) >= gicd_en_regs)
		return;

	writel_relaxed(BIT(hwirq % 32),
		       gicd_base + GICD_ISENABLER + (hwirq / 32) * 4);
}

static void mtk_gic_set_pending_hwirq(u32 hwirq)
{
	if (!gicd_base || hwirq < 32 || (hwirq / 32) >= gicd_en_regs)
		return;

	writel_relaxed(BIT(hwirq % 32),
		       gicd_base + GICD_ISPENDR + (hwirq / 32) * 4);
}

static void mtk_gic_mask_all(void)
{
	u32 i;

	if (!gicd_base)
		return;

	for (i = 1; i < gicd_en_regs; i++) {
		gicd_saved_isenabler[i] =
			readl_relaxed(gicd_base + GICD_ISENABLER + i * 4);
		writel_relaxed(0xffffffffU, gicd_base + GICD_ICENABLER + i * 4);
	}
	gicd_wait_for_rwp();
}

static void mtk_gic_mask_restore(void)
{
	u32 i;

	if (!gicd_base)
		return;

	for (i = 1; i < gicd_en_regs; i++) {
		writel_relaxed(0xffffffffU, gicd_base + GICD_ICENABLER + i * 4);
		writel_relaxed(gicd_saved_isenabler[i],
			       gicd_base + GICD_ISENABLER + i * 4);
	}
	gicd_wait_for_rwp();
}

static void mtk_cirq_ack_all(void)
{
	u32 i;

	if (!cirq_base)
		return;

	for (i = 0; i < cirq_ctrl_regs; i++)
		writel_relaxed(0xffffffffU, cirq_base + CIRQ_ACK_BASE + i * 4);
	mb();
}

static void mtk_cirq_mask_all(void)
{
	u32 i;

	if (!cirq_base)
		return;

	for (i = 0; i < cirq_ctrl_regs; i++)
		writel_relaxed(0xffffffffU,
			       cirq_base + CIRQ_MASK_SET_BASE + i * 4);
	mb();
}

static void mtk_cirq_clone_and_enable(void)
{
	int i;
	u32 st;

	if (!cirq_base || !gicd_base)
		return;

	for (i = 0; i < IRQ_NUMBER; i++) {
		u32 hwirq = edge_trig_hwirqs[i];
		u32 spi_base = 32 + cirq_spi_start;
		u32 cirq_id, reg_off, bit, cfg, pol;

		if (hwirq < spi_base || hwirq >= spi_base + cirq_irq_num)
			continue;

		cirq_id = hwirq - spi_base;
		reg_off = (cirq_id / 32) * 4;
		bit = BIT(cirq_id % 32);

		cfg = readl_relaxed(gicd_base + GICD_ICFGR + (hwirq / 16) * 4);
		if (cfg & (0x2U << ((hwirq % 16) * 2)))
			writel_relaxed(bit, cirq_base + CIRQ_SENS_CLR_BASE + reg_off);
		else
			writel_relaxed(bit, cirq_base + CIRQ_SENS_SET_BASE + reg_off);

		if (sysirq_base) {
			pol = readl_relaxed(sysirq_base + ((hwirq - 32) / 32) * 4);
			if (pol & BIT((hwirq - 32) % 32))
				writel_relaxed(bit, cirq_base + CIRQ_POL_CLR_BASE + reg_off);
			else
				writel_relaxed(bit, cirq_base + CIRQ_POL_SET_BASE + reg_off);
		}

		if (gicd_saved_isenabler[hwirq / 32] & BIT(hwirq % 32))
			writel_relaxed(bit, cirq_base + CIRQ_MASK_CLR_BASE + reg_off);
		else
			writel_relaxed(bit, cirq_base + CIRQ_MASK_SET_BASE + reg_off);
	}

	mtk_cirq_ack_all();
	st = readl_relaxed(cirq_base + CIRQ_CON);
	st |= CIRQ_CON_EN;
	writel_relaxed(st & CIRQ_CON_BITS_MASK, cirq_base + CIRQ_CON);
	mb();
}

static void mtk_cirq_flush_and_disable(void)
{
	int i;
	u32 st;

	if (!cirq_base)
		return;

	for (i = 0; i < IRQ_NUMBER; i++) {
		u32 hwirq = edge_trig_hwirqs[i];
		u32 spi_base = 32 + cirq_spi_start;
		u32 cirq_id, reg_off, sta;

		if (hwirq < spi_base || hwirq >= spi_base + cirq_irq_num)
			continue;

		cirq_id = hwirq - spi_base;
		reg_off = (cirq_id / 32) * 4;
		sta = readl_relaxed(cirq_base + CIRQ_STA_BASE + reg_off);
		if (sta & BIT(cirq_id % 32))
			mtk_gic_set_pending_hwirq(hwirq);
	}

	st = readl_relaxed(cirq_base + CIRQ_CON);
	st &= ~CIRQ_CON_EN;
	writel_relaxed(st & CIRQ_CON_BITS_MASK, cirq_base + CIRQ_CON);
	mtk_cirq_mask_all();
	mtk_cirq_ack_all();
}

static void mtk_spm_get_edge_trigger_irq(void)
{
	int i;
	struct device_node *node;
	unsigned int irq_type;

	pr_info("[SPM] edge trigger irqs:\n");
	for (i = 0; i < IRQ_NUMBER; i++) {
		node = of_find_compatible_node(NULL, NULL, list[i].name);
		if (!node) {
			pr_info("[SPM] find '%s' node failed\n",
				list[i].name);
			continue;
		}

		edge_trig_irqs[i] =
			irq_of_parse_and_map(node, list[i].order);
		of_node_put(node);

		if (!edge_trig_irqs[i]) {
			pr_info("[SPM] get '%s' failed\n",
				list[i].name);
			continue;
		}

		/* Note: Initialize irq type to avoid pending irqs */
		irq_type = irq_get_trigger_type(edge_trig_irqs[i]);
		irq_set_irq_type(edge_trig_irqs[i], irq_type);
		edge_trig_hwirqs[i] = virq_to_hwirq(edge_trig_irqs[i]);

		pr_info("[SPM] '%s', irq=%d, hwirq=%u, type=%d\n",
			list[i].name, edge_trig_irqs[i],
			edge_trig_hwirqs[i], irq_type);
	}
}

static bool spm_in_idle;
static int cpu_pm_callback_wakeup_src_restore(
	struct notifier_block *self, unsigned long cmd, void *v)
{
	int i;

	/* Note: cmd will be CPU_PM_ENTER/CPU_PM_EXIT/CPU_PM_ENTER_FAILED.
	 * Set edge trigger interrupt pending only in case CPU_PM_EXIT
	 */
	if (cmd == CPU_PM_EXIT && spm_in_idle) {
		for (i = 0; i < IRQ_NUMBER; i++) {
			if (edge_trig_hwirqs[i] &&
			    (spm_read(SPM_SW_RSV_0) & list[i].wakesrc))
				mtk_gic_set_pending_hwirq(edge_trig_hwirqs[i]);
		}
	}

	return NOTIFY_OK;
}

static struct notifier_block mtk_spm_cpu_pm_notifier_block = {
	.notifier_call = cpu_pm_callback_wakeup_src_restore,
};

static unsigned int spm_irq_0;
static unsigned int spm_hwirq_0;

void mtk_spm_irq_backup(void)
{
	int i;

	spm_in_idle = true;
	mtk_gic_mask_all();
	mtk_gic_unmask_hwirq(spm_hwirq_0);
	for (i = 0; i < IRQ_NUMBER; i++)
		mtk_gic_unmask_hwirq(edge_trig_hwirqs[i]);
	gicd_wait_for_rwp();
	mtk_cirq_clone_and_enable();
}

void mtk_spm_irq_restore(void)
{
	mtk_cirq_flush_and_disable();
	mtk_gic_mask_restore();
	spm_in_idle = false;
}

unsigned int mtk_spm_get_irq_0(void)
{
	return spm_irq_0;
}

/********************************************************************
 * spm irq handler and initialize function
 *******************************************************************/

static irqreturn_t spm_irq0_handler(int irq, void *dev_id)
{
	u32 isr;
	unsigned long flags;
	struct twam_cfg twamsig;
	twam_handler_t twam_handler;
	u32 twam_idle_con = 0;
	u32 twam_idle_sel = 0;
	struct twam_select twam_sel;

	spin_lock_irqsave(&__spm_lock, flags);
	/* get ISR status */
	isr = spm_read(SPM_IRQ_STA);
	if (isr & ISRS_TWAM) {
		twamsig.byte[0].id = spm_read(SPM_TWAM_LAST_STA0);
		twamsig.byte[1].id = spm_read(SPM_TWAM_LAST_STA1);
		twamsig.byte[2].id = spm_read(SPM_TWAM_LAST_STA2);
		twamsig.byte[3].id = spm_read(SPM_TWAM_LAST_STA3);
		twam_idle_con = spm_read(SPM_TWAM_CON);
		twam_idle_sel = spm_read(SPM_TWAM_IDLE_SEL);
		twam_sel.signal[0] = (twam_idle_sel & 0x00000003);
		twam_sel.signal[1] = ((twam_idle_sel & 0x0000000C) >> 2);
		twam_sel.signal[2] = ((twam_idle_sel & 0x00000030) >> 4);
		twam_sel.signal[3] = ((twam_idle_sel & 0x000000C0) >> 6);
		twam_sel.id[0] = (twam_idle_con & 0x0001F000) >> 12;
		twam_sel.id[1] = ((twam_idle_con & 0x003E0000) >> 17);
		twam_sel.id[2] = ((twam_idle_con & 0x07C00000) >> 22);
		twam_sel.id[3] = ((twam_idle_con & 0xF8000000) >> 27);
		udelay(40); /* delay 1T @ 32K */
	}
	/* clean ISR status */
	SMC_CALL(IRQ0_HANDLER, isr, 0, 0);
	spin_unlock_irqrestore(&__spm_lock, flags);

	if (isr & (ISRS_SW_INT1)) {
		pr_info("[SPM] IRQ0 (ISRS_SW_INT1) HANDLER SHOULD NOT BE EXECUTED (0x%x)\n",
			isr);
		#if !IS_ENABLED(CONFIG_FPGA_EARLY_PORTING)
		spm_vcorefs_dump_dvfs_regs(NULL);
		#endif
		return IRQ_HANDLED;
	}

	twam_handler = spm_twam_handler_get();
	if ((isr & ISRS_TWAM) && twam_handler)
		twam_handler(&twamsig, &twam_sel);

	if (isr & (ISRS_SW_INT0 | ISRS_PCM_RETURN))
		pr_info("[SPM] IRQ0 HANDLER SHOULD NOT BE EXECUTED (0x%x)\n",
			isr);

	return IRQ_HANDLED;
}

struct spm_irq_desc {
	unsigned int irq;
	irq_handler_t handler;
};

int mtk_spm_irq_register(unsigned int spmirq0)
{
	struct device_node *node;
	int i, err, r = 0;

	struct spm_irq_desc irqdesc[] = {
		{.irq = 0, .handler = spm_irq0_handler,}
	};
	irqdesc[0].irq = spmirq0;
	for (i = 0; i < ARRAY_SIZE(irqdesc); i++) {
		if (cpu_present(i)) {
			pr_info("[SPM] REQUEST IRQ (%d)\n", i);
			err = request_irq(irqdesc[i].irq, irqdesc[i].handler,
				IRQF_NO_SUSPEND |
				IRQF_PERCPU,
				"SPM", NULL);
			if (err) {
				pr_info("[SPM] FAILED TO REQUEST IRQ%d (%d)\n",
					i, err);
				r = -EPERM;
			}
		}
	}

	/* Assing local spm_irq_0 */
	spm_irq_0 = spmirq0;
	spm_hwirq_0 = virq_to_hwirq(spm_irq_0);

	node = of_find_compatible_node(NULL, NULL, "arm,gic-v3");
	if (node) {
		gicd_base = of_iomap(node, 0);
		of_node_put(node);
		if (gicd_base) {
			u32 typer = readl_relaxed(gicd_base + GICD_TYPER);

			gicd_en_regs = min_t(u32, (typer & 0x1f) + 1,
					     GIC_MAX_EN_REGS);
		}
	}

	node = of_find_compatible_node(NULL, NULL, "mediatek,mt6577-sysirq");
	if (node) {
		sysirq_base = of_iomap(node, 0);
		of_node_put(node);
	}

	node = of_find_compatible_node(NULL, NULL, "mediatek,sys_cirq");
	if (node) {
		cirq_base = of_iomap(node, 0);
		of_property_read_u32(node, "mediatek,cirq_num", &cirq_irq_num);
		of_property_read_u32(node, "mediatek,spi_start_offset",
				     &cirq_spi_start);
		of_node_put(node);
		cirq_ctrl_regs = min_t(u32, (cirq_irq_num + 31) / 32, 16);
		mtk_cirq_mask_all();
	}

	mtk_spm_get_edge_trigger_irq();

	pr_info("[SPM] irq0=%u(hwirq=%u) gicd_regs=%u cirq=%s\n",
		spm_irq_0, spm_hwirq_0, gicd_en_regs,
		cirq_base ? "ok" : "none");

	cpu_pm_register_notifier(&mtk_spm_cpu_pm_notifier_block);

	return r;
}
MODULE_LICENSE("GPL");
