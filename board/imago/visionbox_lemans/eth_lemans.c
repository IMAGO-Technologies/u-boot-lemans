/*
 * Copyright IMAGO Technologies GmbH
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#include <common.h>
#include <command.h>
#include <netdev.h>
#include <malloc.h>
#include <fsl_mdio.h>
#include <miiphy.h>
#include <phy.h>
#include <fm_eth.h>
#include <asm/io.h>
#include <exports.h>
#include <asm/arch/fsl_serdes.h>
#include <fsl-mc/ldpaa_wriop.h>
#include <fsl-mc/fsl_mc.h>

DECLARE_GLOBAL_DATA_PTR;

int select_i2c_ch_pca9547(uint8_t ch);

/*
 * U-Boot command fixup_dpl:
 * dummy function to avoid error when using an old U-Boot environment
 * */
static int do_fixup_dpl(cmd_tbl_t *cmdtp, int flag, int argc, char * const argv[])
{
	return CMD_RET_SUCCESS;
}

U_BOOT_CMD(
	fixup_dpl,  2,  1,   do_fixup_dpl,
	"This command is obsolete",
	"fixup_dpl\n"
);


int board_eth_init(bd_t *bis)
{
#if defined(CONFIG_FSL_MC_ENET)
	int i, interface;
	struct memac_mdio_info mdio_info;
	struct mii_dev *dev;
	struct memac_mdio_controller *reg;

	reg = (struct memac_mdio_controller *)CONFIG_SYS_FSL_WRIOP1_MDIO2;
	mdio_info.regs = reg;
	mdio_info.name = DEFAULT_WRIOP_MDIO2_NAME;

	/* Register the EMI 2 */
	fm_memac_mdio_init(bis, &mdio_info);

	wriop_set_phy_address(WRIOP1_DPMAC13, 0);
	wriop_set_phy_address(WRIOP1_DPMAC14, 1);
	wriop_set_phy_address(WRIOP1_DPMAC15, 2);
	wriop_set_phy_address(WRIOP1_DPMAC16, 3);
	for (i = WRIOP1_DPMAC13; i <= WRIOP1_DPMAC16; i++) {
		interface = wriop_get_enet_if(i);
		switch (interface) {
		case PHY_INTERFACE_MODE_QSGMII:
			dev = miiphy_get_dev_by_name(DEFAULT_WRIOP_MDIO2_NAME);
			wriop_set_mdio(i, dev);
			break;
		default:
			break;
		}
	}

	// Set internal MDIO address for XFI lanes with the SerDes Register XFInCR1.
	// This also enables MDIO access by Linux using the 10GBASE KR driver:
	*(unsigned int *)0x01ea19f4 = 0x11 << 27;	// XFIHCR1: Lane H MDIO address 0x11
	*(unsigned int *)0x01ea19e4 = 0x12 << 27;	// XFIGCR1: Lane G MDIO address 0x12
	
	// Register MDIO bus for XFI MAC1
	reg = (struct memac_mdio_controller *)0x08c07000;
	mdio_info.regs = reg;
	mdio_info.name = "FSL_MDIO2";
	fm_memac_mdio_init(bis, &mdio_info);

	wriop_set_phy_address(WRIOP1_DPMAC1, 0x11);

	dev = miiphy_get_dev_by_name("FSL_MDIO2");
	wriop_set_mdio(WRIOP1_DPMAC1, dev);

	
	// Register MDIO bus for XFI MAC2
	reg = (struct memac_mdio_controller *)0x08c0b000;
	mdio_info.regs = reg;
	mdio_info.name = "FSL_MDIO3";
	fm_memac_mdio_init(bis, &mdio_info);

	wriop_set_phy_address(WRIOP1_DPMAC2, 0x12);

	dev = miiphy_get_dev_by_name("FSL_MDIO3");
	wriop_set_mdio(WRIOP1_DPMAC2, dev);

	
	cpu_eth_init(bis);
#endif /* CONFIG_FSL_MC_ENET */


	/* XFI Retimer: Invert channel A */
	for (i=0; i<2; i++)
	{
		unsigned char regval;
		int ret;

		// Setup I2C Mux:
		select_i2c_ch_pca9547(I2C_MUX_CH_XFI1+i);
		
		// Select XFI channel A
		regval = 0x04;
		ret = i2c_write(I2C_XFI_RETIMER_ADDR, 0xff, 1, &regval, 1);
		if (ret)
			puts("PCA: failed to configure XFI retimer\n");

		// Invert driver polarity
		ret = i2c_read(I2C_XFI_RETIMER_ADDR, 0x1f, 1, &regval, 1);
		if (ret)
			puts("PCA: failed to configure XFI retimer\n");
		regval |= 0x80;
		ret = i2c_write(I2C_XFI_RETIMER_ADDR, 0x1f, 1, &regval, 1);
		if (ret)
			puts("PCA: failed to configure XFI retimer\n");
	}

	return pci_eth_init(bis);
}

#if defined(CONFIG_RESET_PHY_R)
void reset_phy(void)
{
/*	u64 base[CONFIG_NR_DRAM_BANKS];
	u64 size[CONFIG_NR_DRAM_BANKS];

	base[0] = gd->bd->bi_dram[0].start;
	size[0] = gd->bd->bi_dram[0].size;
	base[1] = gd->bd->bi_dram[1].start;
	size[1] = gd->bd->bi_dram[1].size;

	if (gd->arch.resv_ram >= base[1] &&
		 gd->arch.resv_ram < base[1] + size[1])
	{
		u64 len = size[1] - (gd->arch.resv_ram - base[1]);
		printf("Clearing reserved memory at 0x%llx, len=0x%llx...\n", gd->arch.resv_ram, len);
		memset(gd->arch.resv_ram, 0, len);
	}
*/
	mc_env_boot();
}
#endif
