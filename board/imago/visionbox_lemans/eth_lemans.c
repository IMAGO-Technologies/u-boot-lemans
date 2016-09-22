/*
 * Copyright 2015 Freescale Semiconductor, Inc.
 *
 *
 * SPDX-License-Identifier:     GPL-2.0+
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

DECLARE_GLOBAL_DATA_PTR;

#define MC_BOOT_ENV_VAR "mcinitcmd"

int board_eth_init(bd_t *bis)
{
#if defined(CONFIG_FSL_MC_ENET)
	char *mc_boot_env_var;
	int i, interface;
	struct memac_mdio_info mdio_info;
	struct mii_dev *dev;
	struct ccsr_gur *gur = (void *)(CONFIG_SYS_FSL_GUTS_ADDR);
	u32 srds_s1;
	struct memac_mdio_controller *reg;

	srds_s1 = in_le32(&gur->rcwsr[28]) &
				FSL_CHASSIS3_RCWSR28_SRDS1_PRTCL_MASK;
	srds_s1 >>= FSL_CHASSIS3_RCWSR28_SRDS1_PRTCL_SHIFT;

#if 0
	reg = (struct memac_mdio_controller *)CONFIG_SYS_FSL_WRIOP1_MDIO1;
	mdio_info.regs = reg;
	mdio_info.name = DEFAULT_WRIOP_MDIO1_NAME;

	/* Register the EMI 1 */
	fm_memac_mdio_init(bis, &mdio_info);
#endif
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

	/*
	 * XFI does not need a PHY to work, but to avoid U-boot use
	 * default PHY address which is zero to a MAC when it found
	 * a MAC has no PHY address, we give a PHY address to XFI
	 * MAC, and should not use a real XAUI PHY address, since
	 * MDIO can access it successfully, and then MDIO thinks
	 * the XAUI card is used for the XFI MAC, which will cause
	 * error.
	 */
	wriop_set_phy_address(WRIOP1_DPMAC1, 4);
	wriop_set_phy_address(WRIOP1_DPMAC2, 5);
	
	mc_boot_env_var = getenv(MC_BOOT_ENV_VAR);
	if (mc_boot_env_var)
		run_command_list(mc_boot_env_var, -1, 0);

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

	select_i2c_ch_pca9547(I2C_MUX_CH_DEFAULT);

/*	u16 phyReg;
	dev = miiphy_get_dev_by_name(DEFAULT_WRIOP_MDIO2_NAME);
	memac_mdio_write(dev, 0, -1, 22, 4);
	phyReg = memac_mdio_read(dev, 0, -1, 0);
	phyReg |= 0x200;
	memac_mdio_write(dev, 0, -1, 0, phyReg);
	printf("PHY reg0: 0x%04x\n", memac_mdio_read(dev, 0, -1, 0));
	printf("PHY reg1: 0x%04x\n", memac_mdio_read(dev, 0, -1, 1));
	printf("PHY reg1: 0x%04x\n", memac_mdio_read(dev, 0, -1, 1));
	udelay(1000000);
	printf("PHY reg1: 0x%04x\n", memac_mdio_read(dev, 0, -1, 1));
	printf("PHY reg1: 0x%04x\n", memac_mdio_read(dev, 0, -1, 1));
	memac_mdio_write(dev, 0, -1, 22, 0);
*/
	return pci_eth_init(bis);
}
