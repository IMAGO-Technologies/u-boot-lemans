/*
 * Copyright 2016 IMAGO Technologies GmbH
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
#include <fsl_debug_server.h>
#include <fsl-mc/fsl_mc.h>
#include <environment.h>
#include <i2c.h>
#include <asm/arch/soc.h>
#include <fsl_sec.h>
#include "vid.h"

DECLARE_GLOBAL_DATA_PTR;

#define GPIO1_BASE	0x02300000
#define GPIO2_BASE	0x02310000
#define GPIO3_BASE	0x02320000
#define GPIO4_BASE	0x02330000
#define GPIO_DIR	0
#define GPIO_DATA	8

/* I2C Multiplexer */
int select_i2c_ch_pca9547(u8 ch)
{
	int ret;

	ret = i2c_write(I2C_MUX_PCA_ADDR, 0, 1, &ch, 1);
	if (ret) {
		puts("PCA: failed to select proper channel\n");
		return ret;
	}

	return 0;
}

int i2c_multiplexer_select_vid_channel(u8 channel)
{
	return select_i2c_ch_pca9547(channel);
}

int checkboard(void)
{
	u32 regValue;
	int i;

	printf("Board: VisionBox LeMans\n");

//	printf("Board version: %c", ...);

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
		uint8_t val;
		int ret;
		
		select_i2c_ch_pca9547(I2C_MUX_CH_XFI1 + i);
		/* Write PCA9538 output register: */
		val = 0x40;	// RATE = 1, TXDISABLE = 0
		ret = i2c_write(I2C_XFI_GPIOMUX_ADDR, 1, 1, &val, 1);
		if (ret) {
			printf("I2C: failed to configure GPIO-Multiplexer (%u)\n", i);
			continue;
		}
		/* Write PCA9538 configuration register: 0 is OUT, 1 is IN */
		val = ~0x95;
		ret = i2c_write(I2C_XFI_GPIOMUX_ADDR, 1, 1, &val, 1);
		if (ret) {
			printf("I2C: failed to configure GPIO-Multiplexer (%u)\n", i);
			continue;
		}
	}

	return 0;
}

int board_init(void)
{
#ifdef CONFIG_ENV_IS_NOWHERE
	gd->env_addr = (ulong)&default_environment[0];
#endif
	select_i2c_ch_pca9547(I2C_MUX_CH_DEFAULT);

	/* invert AQR405 IRQ pins polarity */
//	out_le32(irq_ccsr + IRQCR_OFFSET / 4, AQR405_IRQ_MASK);

	return 0;
}

#define SCFG_QSPICLKCTRL_DIV_20	(5 << 27)

int board_early_init_f(void)
{
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

//#define TESTVAL  (~(1<<(i%32))
//#define TESTVAL  (1<<(i%64))
//#define TESTVAL  (i%2?0x5555555555555555ull:0xaaaaaaaaaaaaaaaaull)
//#define TESTVAL  (0)
//#define TESTVAL  i
#define TESTVAL  ((1ull<<(63-(i%32))) | (1<<(i%32)))

void dram_test(void)
{
	int bank = 0;
	unsigned long long i;
	int ctrl = 0;

#if 0
	printf("\n");

	// Dump DDR4 debug registers
	{
		int *repeatable;
		unsigned long *ticks;
		char *argv[] = {"base", "0"};
		cmd_process(0, 2, argv, &repeatable, &ticks);
	}

	{
		int *repeatable;
		unsigned long *ticks;
		char *argv[] = {"md.l", "0x1080000", "0x400"};
		
//		printf("\nDDR1 register dump (1):\n");
//		cmd_process(0, 3, argv, &repeatable, &ticks);

		printf("*(unsigned int *)0x1080FB0 = 0x10000003;\n");
		*(unsigned int *)0x1080FB0 = 0x10000003;

		printf("\nDDR1 register dump (2):\n");
		cmd_process(0, 3, argv, &repeatable, &ticks);
	}
	{
		int *repeatable;
		unsigned long *ticks;
		char *argv[] = {"md.l", "0x1090000", "0x400"};

//		printf("\nDDR2 register dump (1):\n");
//		cmd_process(0, 3, argv, &repeatable, &ticks);

		printf("*(unsigned int *)0x1080FB0 = 0x10000003;\n");
		*(unsigned int *)0x1090FB0 = 0x10000003;

		printf("\nDDR2 register dump (2):\n");
		cmd_process(0, 3, argv, &repeatable, &ticks);
	}
#endif

#if 1
	unsigned long size = gd->bd->bi_dram[bank].size - (16 << 20);
	unsigned int errors = 0;
	unsigned long long *base = (unsigned long long *)(gd->bd->bi_dram[bank].start);

	printf("\nTesting memory bank %u, %lu MB: ", bank, (size>>20));

	printf("writing...");
	for (i=0; i<(size/8); i++)
	{
		base[i] = TESTVAL;
	}
	printf(", reading...");

	for (i=0; i<(size/8); i++)
	{
		unsigned long long out = TESTVAL;
		if (base[i] != out)
		{
			printf("\nData error: base[%llu]=0x%016llx, xor=0x%016llX", i, base[i], base[i]^out);
			errors++;
			if (errors >= 100)
				break;
		}
	}

	if (errors == 0)
		printf("   OK\n");
	else
		printf("\n   errors: %u\n", errors);

	return;
#endif
	
//	for (bank=1; bank < CONFIG_NR_DRAM_BANKS; bank++)
	while (1)
	{
		unsigned long size = gd->bd->bi_dram[bank].size;
		unsigned int errors = 0;

		size = 2<<10;
		printf("\nTesting bank %u, %lu kB...\n", bank, (size>>10));
#if 1
		unsigned long long *base = (unsigned long long *)(gd->bd->bi_dram[bank].start);

		udelay(1000000);

		printf("writing...\n");
		for (ctrl=0; ctrl<=1; ctrl++)
		{
			for (i=32*ctrl; i<(size/8); i++)
			{
				base[i] = TESTVAL;
				if ((i % 32) == 31)
					i+=32;
			}
		}
		
		flush_dcache_range((unsigned long)base, (unsigned long)&base[i]);
		invalidate_dcache_range((unsigned long)base, (unsigned long)&base[i]);

#if 1
		int readcount;
		for (readcount=0; readcount<4; readcount++)
		{
			errors = 0;
			for (ctrl=0; ctrl<=1; ctrl++)
			{
				printf("reading controller %u...\n", ctrl);
				for (i=32*ctrl; i<64; i++)
				{
					unsigned long long out = TESTVAL;
					if (base[i] != out)
					{
						printf("Data error: base[%04llu]=0x%016llx, xor=0x%016llX\n", i, base[i], base[i]^out);
						errors++;
					}
		/*			if ((i%1024) == 0)
					{
						printf("Errors: %u\n", errors);
						errors = 0;
					}
					if ((i % 32) == 31)
						i+=32;*/
		/*			if ((i%(1024+32)) == 0)
					{
						printf("Errors: %u\n", errors);
						errors = 0;
					}*/
					if ((i % 32) == 31)
						i+=32;
		//			if (errors >= 100)
		//				break;
					udelay(25000);
				}
				if (errors == 0)
					printf("   OK\n");
				else
					printf("   controller %u errors: %u\n", ctrl, errors);
				errors = 0;
				udelay(1000000);
			}
			invalidate_dcache_range((unsigned long)base, (unsigned long)&base[i]);
		}
#elif 1
		for (ctrl=0; ctrl<=1; ctrl++)
		{
			printf("reading controller %u...\n", ctrl);
			for (i=32*ctrl; i<(size/8); i++)
			{
				unsigned long long out = TESTVAL;
				if (base[i] != out)
				{
					printf("Data error: base[%04lu]=0x%016llx, xor=0x%016llX\n", i, base[i], base[i]^out);
					errors++;
				}
	/*			if ((i%1024) == 0)
				{
					printf("Errors: %u\n", errors);
					errors = 0;
				}
				if ((i % 32) == 31)
					i+=32;*/
	/*			if ((i%(1024+32)) == 0)
				{
					printf("Errors: %u\n", errors);
					errors = 0;
				}*/
				if ((i % 32) == 31)
					i+=32;
	//			if (errors >= 100)
	//				break;
	//			udelay(1);
			}
			if (errors == 0)
				printf("   OK\n");
			else
				printf("   controller %u errors: %u\n", ctrl, errors);
			errors = 0;
		}
		invalidate_dcache_range((unsigned long)base, (unsigned long)&base[i]);
#endif
#else
		unsigned int *base = (unsigned int *)(gd->bd->bi_dram[bank].start);
		printf("base = %p\n", base);
	
/*		printf("reading...\n");
		for (i=0; i<(size/4); i++)
		{
			printf("0x%08x\n", base[i]);
		}*/

		printf("writing...\n");
		for (i=0; i<(size/4); i++)
		{
//			printf("writing %u\n", i);
			base[i] = TESTVAL;
//			udelay(1);
		}

		flush_dcache_range((unsigned long)base, (unsigned long)&base[i]);
		invalidate_dcache_range((unsigned long)base, (unsigned long)&base[i]);
		
		printf("reading...\n");

		for (i=0; i<(size/4); i++)
		{
			unsigned int out = TESTVAL;
			if (base[i] != out)
			{
				printf("Data error: base[0x%08lx]=0x%08x, soll=0x%08x, xor=0x%08X\n", i, base[i], out, base[i]^out);
				errors++;
			}
//			if (errors >= 100)
//				break;
//			udelay(1);
		}
#endif
	}
}

void detail_board_ddr_info(void)
{
	print_ddr_info(0);
	dram_test();
}

int dram_init(void)
{
	select_i2c_ch_pca9547(I2C_MUX_CH_DDR4);

	gd->ram_size = initdram(0);

	return 0;
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

	if (get_mc_boot_status() == 0)
		fdt_status_okay(fdt, offset);
	else
		fdt_status_fail(fdt, offset);
}
#endif

#ifdef CONFIG_OF_BOARD_SETUP
int ft_board_setup(void *blob, bd_t *bd)
{
	int err;
	u64 base[CONFIG_NR_DRAM_BANKS];
	u64 size[CONFIG_NR_DRAM_BANKS];

	ft_cpu_setup(blob, bd);

	/* fixup DT for the two GPP DDR banks */
	base[0] = gd->bd->bi_dram[0].start;
	size[0] = gd->bd->bi_dram[0].size;
	base[1] = gd->bd->bi_dram[1].start;
	size[1] = gd->bd->bi_dram[1].size;

	fdt_fixup_memory_banks(blob, base, size, 2);

#ifdef CONFIG_FSL_MC_ENET
	fdt_fixup_board_enet(blob);
	err = fsl_mc_ldpaa_exit(bd);
	if (err)
		return err;
#endif

	return 0;
}
#endif

void update_spd_address(unsigned int ctrl_num,
			unsigned int slot,
			unsigned int *addr)
{
}

/*
 * DDR4 Reset per GPIO zum FPGA:
 * DDR4 Reset-Timing ist nur bei Registered-DIMMs von Bedeutung, gibt es bei
 * SO-DIMMS aber nicht.
 */

#if 0

int board_need_mem_reset(void)
{
	return 1;
}

void board_assert_mem_reset(void)
{
	out_le32((void *)(GPIO3_BASE+GPIO_DATA), 1<<(31-27));
	out_le32((void *)(GPIO3_BASE+GPIO_DIR), 1<<(31-27));
}

void board_deassert_mem_reset(void)
{
	out_le32((void *)(GPIO3_BASE+GPIO_DATA), 0);
}
#endif
