/*
 * Copyright IMAGO Technologies GmbH
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */
#include <common.h>
#include <malloc.h>
#include <errno.h>
#include <netdev.h>
#include <fsl_ifc.h>
#include <asm/io.h>
#include <hwconfig.h>
#include <fdt_support.h>
#include <libfdt.h>
#include <fsl-mc/fsl_mc.h>
#include <fsl_ddr_sdram.h>
#include <environment.h>
#include <i2c.h>
#include <asm/arch/soc.h>
#include <asm/arch/immap_lsch3.h>
#include <fsl_sec.h>
#include <ahci.h>
#include "vid.h"

DECLARE_GLOBAL_DATA_PTR;

#define GPIO1_BASE	0x02300000
#define GPIO2_BASE	0x02310000
#define GPIO3_BASE	0x02320000
#define GPIO4_BASE	0x02330000
#define GPIO_DIR	0
#define GPIO_DATA	8


#define WAIT_MS_LINKUP	200

int ahci_link_up(struct ahci_uc_priv *uc_priv, int port)
{
	u32 tmp;
	int j = 0;
	void __iomem *port_mmio = uc_priv->port[port].port_mmio;

	// Limit link speed to 3 Gbps in port SATA control register (PxSCTL)
	tmp = readl(port_mmio + PORT_SCR_CTL);
	tmp &= ~0xf0;
	tmp |= 0x21;	// Gen 2 + hard reset
	writel(tmp, port_mmio + PORT_SCR_CTL);
	udelay(1000);
	tmp &= ~0x1;	// reset off
	writel(tmp, port_mmio + PORT_SCR_CTL);
	
	/*
	 * Bring up SATA link.
	 * SATA link bringup time is usually less than 1 ms; only very
	 * rarely has it taken between 1-2 ms. Never seen it above 2 ms.
	 */
	while (j < WAIT_MS_LINKUP) {
		tmp = readl(port_mmio + PORT_SCR_STAT);
		if ((tmp & PORT_SCR_STAT_DET_MASK) == PORT_SCR_STAT_DET_PHYRDY)
		{
			int speed = (tmp >> 4) & 0xf;
			const char *speed_s;

			if (speed == 1)
				speed_s = "1.5";
			else if (speed == 2)
				speed_s = "3";
			else if (speed == 3)
				speed_s = "6";
			else
				speed_s = "?";
			printf("SATA link speed: %s Gbps\n", speed_s);

			return 0;
		}
		udelay(1000);
		j++;
	}
	
	return 1;
}

/* I2C Multiplexer */
int select_i2c_ch_pca9547(uint8_t ch)
{
	int ret;

	ret = i2c_write(I2C_MUX_PCA_ADDR, 0, 1, &ch, 1);
	if (ret) {
		puts("PCA: failed to select proper channel\n");
		return ret;
	}

	return 0;
}

int i2c_multiplexer_select_vid_channel(uint8_t channel)
{
	return select_i2c_ch_pca9547(channel);
}


int checkboard(void)
{
	uint8_t val;
	int ret;
	uint32_t regValue;
	int i;

	/* 
	 * Configure unconnected GPIO pins as outputs
	 */
	
	/* GPIO2 */
	regValue = in_le32((void *)(GPIO2_BASE+GPIO_DIR));
	/* IFC_RB0,1_B => GPIO2_14,15: */
	regValue |= 1<<(31-14);
	regValue |= 1<<(31-15);
	out_le32((void *)(GPIO2_BASE+GPIO_DIR), regValue);

	/* GPIO3 */
	regValue = in_le32((void *)(GPIO3_BASE+GPIO_DIR));
	/* IRQ04,05 => GPIO3_28,29: */
	regValue |= 1<<(31-28);
	regValue |= 1<<(31-29);
	/* UART1 => GPIO3_08,10: */
	regValue |= 1<<(31-8);
	regValue |= 1<<(31-10);
	/* UART2 => GPIO3_06,07,09,11: */
	regValue |= 1<<(31-6);
	regValue |= 1<<(31-7);
	regValue |= 1<<(31-9);
	regValue |= 1<<(31-11);
	out_le32((void *)(GPIO3_BASE+GPIO_DIR), regValue);

	/* GPIO4 */
	regValue = in_le32((void *)(GPIO4_BASE+GPIO_DIR));
	/* IRQ07,09...11 => GPIO4_05,07...09: */
	regValue |= 1<<(31-5);
	regValue |= 1<<(31-7);
	regValue |= 1<<(31-8);
	regValue |= 1<<(31-9);
	/* IIC3,4 => GPIO4_00,03: */
	regValue |= 1<<(31-0);
	regValue |= 1<<(31-1);
	regValue |= 1<<(31-2);
	regValue |= 1<<(31-3);
	/* IEEE1588 => GPIO4_16,23: */
	regValue |= 1<<(31-16);
	regValue |= 1<<(31-17);
	regValue |= 1<<(31-18);
	regValue |= 1<<(31-19);
	regValue |= 1<<(31-20);
	regValue |= 1<<(31-21);
	regValue |= 1<<(31-22);
	regValue |= 1<<(31-23);
	/* EVT9 => GPIO4_110: */
	regValue |= 1<<(31-10);
	out_le32((void *)(GPIO4_BASE+GPIO_DIR), regValue);

	/* Configure I2C GPIO Expander for both SFP+ channels */
	for (i=0; i<2; i++)
	{
		select_i2c_ch_pca9547(I2C_MUX_CH_XFI1 + i);
		/* Write PCA9538 output register: */
		val = 0x40;	// RS0 = 1, TXDISABLE = 0
		ret = i2c_write(I2C_XFI_GPIOMUX_ADDR, 1, 1, &val, 1);
		if (ret) {
			printf("I2C: failed to configure GPIO-Multiplexer (%u)\n", i);
			continue;
		}
		/* Write PCA9538 configuration register: 0 is OUT, 1 is IN */
		val = (~0x42) & 0xff;
		ret = i2c_write(I2C_XFI_GPIOMUX_ADDR, 3, 1, &val, 1);
		if (ret) {
			printf("I2C: failed to configure GPIO-Multiplexer (%u)\n", i);
			continue;
		}
	}

	/* RTC: turn off clock output during battery supply (BB32kHz = 0) */
	select_i2c_ch_pca9547(I2C_MUX_CH_RTC);
	val = 0x08;
	ret = i2c_write(CONFIG_SYS_I2C_RTC_ADDR, 0xf, 1, &val, 1);
	if (ret)
		printf("I2C: failed to configure RTC\n");

	return 0;
}

int board_init(void)
{
	u32 __iomem *irq_ccsr = (u32 __iomem *)ISC_BASE;
	
#ifdef CONFIG_ENV_IS_NOWHERE
	gd->env_addr = (ulong)&default_environment[0];
#endif

	/* invert IRQ pin polarity
	 * INTFPGA: IRQ00 
	 * Marvell: IRQ01
	 * PCA9538: IRQ02
	 * INTFPGA1:IRQ03
	 * RTC_INT: IRQ06 
	 */
	out_le32(irq_ccsr + IRQCR_OFFSET / 4, 0x4f);

	return 0;
}

#define SCFG_QSPICLKCTRL_DIV_20	(5 << 27)

int board_early_init_f(void)
{
#ifdef CONFIG_SYS_I2C_EARLY_INIT
	i2c_early_init_f();
#endif
	fsl_lsch3_early_init_f();

	/* QSPI Clock: input clk: 1/2 platform clk, output: input/20 */
	out_le32(SCFG_BASE + SCFG_QSPICLKCTLR, SCFG_QSPICLKCTRL_DIV_20);

	return 0;
}

int misc_init_r(void)
{
	if (adjust_vdd(0))
		printf("Warning: Adjusting core voltage failed.\n");

	return 0;
}


void detail_board_ddr_info(void)
{
	print_ddr_info(0);
}

#if defined(CONFIG_ARCH_MISC_INIT)
int arch_misc_init(void)
{
#ifdef CONFIG_FSL_DEBUG_SERVER
	debug_server_init();
#endif
#ifdef CONFIG_FSL_CAAM
	sec_init();
#endif
	return 0;
}
#endif

#ifdef CONFIG_FSL_MC_ENET
void fdt_fixup_board_enet(void *fdt)
{
	int offset;

	offset = fdt_path_offset(fdt, "/fsl-mc");

	if (offset < 0)
		offset = fdt_path_offset(fdt, "/soc/fsl-mc");

	if (offset < 0) {
		printf("%s: ERROR: fsl-mc node not found in device tree (error %d)\n",
		       __func__, offset);
		return;
	}

	if (get_mc_boot_status() == 0 && (get_dpl_apply_status() == 0))
		fdt_status_okay(fdt, offset);
	else
		fdt_status_fail(fdt, offset);
}

void board_quiesce_devices(void)
{
	fsl_mc_ldpaa_exit(gd->bd);
}
#endif

#ifdef CONFIG_OF_BOARD_SETUP
int ft_board_setup(void *blob, bd_t *bd)
{
	u64 base[CONFIG_NR_DRAM_BANKS];
	u64 size[CONFIG_NR_DRAM_BANKS];

	ft_cpu_setup(blob, bd);

	/* fixup DT for the two GPP DDR banks */
	base[0] = gd->bd->bi_dram[0].start;
	size[0] = gd->bd->bi_dram[0].size;
	base[1] = gd->bd->bi_dram[1].start;
	size[1] = gd->bd->bi_dram[1].size;

	fdt_fixup_memory_banks(blob, base, size, 2);

	fdt_fsl_mc_fixup_iommu_map_entry(blob);
	
#ifdef CONFIG_FSL_MC_ENET
	fdt_fixup_board_enet(blob);
#endif

	return 0;
}
#endif

