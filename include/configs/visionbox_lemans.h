/*
 * Copyright 2016 IMAGO Technologies GmbH
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#ifndef __LS2_RDB_H
#define __LS2_RDB_H

#include "ls2080a_common.h"

/*#define CONFIG_FSL_LS_PPA*/
#if defined(CONFIG_FSL_LS_PPA)
/* PSCI (Power_State_Coordination_Interface):
 * Achtung: Mit PSCI im Device-Tree kann der GPIO power-off nicht verwendet werden.
 * In der PPA Firmware muss dann smc32_psci_system_off() in psci.s implementiert werden,
 * um den GPIO Pin zu ziehen. */
#define CONFIG_ARMV8_PSCI
#define CONFIG_SYS_LS_PPA_DRAM_BLOCK_MIN_SIZE	(1UL * 1024 * 1024)

#define CONFIG_SYS_LS_PPA_FW_IN_XIP
#ifdef CONFIG_SYS_LS_PPA_FW_IN_XIP
#define CONFIG_SYS_LS_PPA_FW_ADDR		0xA3000000
#endif
#endif

/*#define CONFIG_DISPLAY_BOARDINFO	*//* Show 'Model: ...' */

/*#define CONFIG_BOARD_LATE_INIT*/	/* call board_late_init() in soc.c */

#undef CONFIG_BOOTCOMMAND
#define CONFIG_BOOTCOMMAND "run mmcboot"

/* Initial environment variables */
#undef CONFIG_EXTRA_ENV_SETTINGS
#define CONFIG_EXTRA_ENV_SETTINGS		\
	"hwconfig=fsl_ddr:bank_intlv=auto\0"	\
	"loadaddr=0x80100000\0"			\
	"kernel_addr=0x100000\0"		\
	"ramdisk_addr=0x800000\0"		\
	"ramdisk_size=0x2000000\0"		\
	"fdt_high=0xa0000000\0"			\
	"initrd_high=0xffffffffffffffff\0"	\
	"kernel_load=0xa0000000\0" \
	"mcmemsize=0x70000000\0" \
	"mcinitcmd=mmc dev 0;ext2load mmc 0:1 0xa2300000 /boot/mc.itb;" \
	"ext2load mmc 0:1 0xa2800000 /boot/dpc-0x3741.dtb;"	\
	"fsl_mc start mc 0xa2300000 0xa2800000\0" \
	"mmcboot=ext2load mmc 0:1 0xa2700000 /boot/dpl-eth.0x37_0x41.dtb;" \
	"fixup_dpl 0xa2700000;" \
	"fsl_mc apply DPL 0xa2700000;ext2load mmc 0:1 0xa0000000 /boot/Image.gz;" \
	"unzip 0xa0000000 0xb0000000;ext2load mmc 0:1 0xa1000000 /boot/fsl-ls2088a-lemans.dtb;" \
	"booti 0xb0000000 - 0xa1000000"
	
#undef CONFIG_BOOTARGS
#define CONFIG_BOOTARGS		"console=ttyS0,115200 root=/dev/mmcblk0p1 rw rootwait " \
				"earlycon=uart8250,mmio,0x21c0500 " \
				"ramdisk_size=0x2000000 default_hugepagesz=2m " \
				"hugepagesz=2m hugepages=256 " \
				"libata.force=3.0G"

/*#define CONFIG_CMD_BOOTI*/	/* enable support for ARM64 kernel images */
#define CONFIG_CMD_UNZIP	/* enable unzip command support for compressed images */

	
/*
 * I2C
 */
#define CONFIG_SYS_I2C_EARLY_INIT
#undef CONFIG_SYS_I2C_MXC_I2C2		/* disable I2C bus 2 */
#undef CONFIG_SYS_I2C_MXC_I2C3		/* disable I2C bus 3 */
#undef CONFIG_SYS_I2C_MXC_I2C4		/* disable I2C bus 4 */

#define I2C_MUX_PCA_ADDR		0x75
/*#define I2C_MUX_CH_DEFAULT      0x8*/ /* Channel 0 + Enable Bit (1<<3) */
#define I2C_MUX_CH_DDR4			0x8 /* Channel 0 + Enable Bit (1<<3) */
#define I2C_MUX_CH_VOL_MONITOR	0xa /* Channel 2 + Enable Bit (1<<3) */
#define I2C_MUX_CH_RTC			0xb /* Channel 5 + Enable Bit (1<<3) */
#define I2C_MUX_CH_XFI1			0xe /* Channel 6 + Enable Bit (1<<3) */
#define I2C_MUX_CH_XFI2			0xf /* Channel 7 + Enable Bit (1<<3) */

#define I2C_XFI_RETIMER_ADDR	0x18
#define I2C_XFI_GPIOMUX_ADDR	0x70


/*
 * VID
 */
#define CONFIG_VID_FLS_ENV		"vdd_override_mv"
#define CONFIG_VID

#define I2C_VOL_MONITOR_ADDR		0x38
#define CONFIG_VOL_MONITOR_IR36021_READ
#define CONFIG_VOL_MONITOR_IR36021_SET

/* step the IR regulator in 5mV increments */
#define IR_VDD_STEP_DOWN		5
#define IR_VDD_STEP_UP			5
/* The lowest and highest voltage allowed */
#define VDD_MV_MIN			819
#define VDD_MV_MAX			1212


/*
 * PCIe
 */

/*#define CONFIG_PCI*/		/* Enable PCIE */
/*#define CONFIG_PCIE_LAYERSCAPE	*//* Use common FSL Layerscape PCIe code */

/* PCIe enumeration doesn't work properly when using multiple PCIe Switches. Let the kernel
 * handle enumeration instead.
#define CONFIG_PCI_PNP
#define CONFIG_PCI_SCAN_SHOW
#define CONFIG_CMD_PCI
#define CONFIG_CMD_PCI_ENUM*/
/*
#undef CONFIG_PCIE1
#undef FSL_PEX1_STREAM_ID_START
#undef FSL_PEX1_STREAM_ID_END
#undef FSL_PEX2_STREAM_ID_START
#undef FSL_PEX2_STREAM_ID_END
#undef FSL_PEX3_STREAM_ID_START
#undef FSL_PEX3_STREAM_ID_END
#undef FSL_PEX4_STREAM_ID_START
#undef FSL_PEX4_STREAM_ID_END
#define FSL_PEX2_STREAM_ID_START                64
#define FSL_PEX2_STREAM_ID_END                  95
#define FSL_PEX3_STREAM_ID_START                96
#define FSL_PEX3_STREAM_ID_END                  127
#define FSL_PEX4_STREAM_ID_START                7
#define FSL_PEX4_STREAM_ID_END                  10
*/

#undef FSL_PEX_STREAM_ID_START
#undef FSL_PEX_STREAM_ID_END
#define FSL_PEX_STREAM_ID_START	64
#define FSL_PEX_STREAM_ID_END	127

#undef CONFIG_FSL_IFC	/* no IFC */

#define CONFIG_SYS_TEXT_BASE		0x20100000

/*#define CONFIG_ENV_IS_IN_SPI_FLASH*/
#undef CONFIG_ENV_SIZE
#define CONFIG_ENV_SIZE			0x10000        /* 64kB */
#undef CONFIG_ENV_OFFSET
#define CONFIG_ENV_OFFSET		0x10000        /* 64kB */
#undef CONFIG_ENV_SECT_SIZE
#define CONFIG_ENV_SECT_SIZE	0x10000        /* 64kB */

#define FSL_QSPI_FLASH_SIZE		(1 << 24) /* 16MB */
#define FSL_QSPI_FLASH_NUM		1

/*#define CONFIG_SYS_NO_FLASH*/
#undef CONFIG_CMD_IMLS

#define CONFIG_SYS_CLK_FREQ		100000000
#define CONFIG_DDR_CLK_FREQ		133333333

#define CONFIG_DDR_SPD
#define CONFIG_DDR_ECC
#define CONFIG_ECC_INIT_VIA_DDRCONTROLLER
#define CONFIG_MEM_INIT_VALUE		0xdeadbeef
#undef CONFIG_SYS_DP_DDR_BASE_PHY

#define CONFIG_SYS_I2C_EEPROM_ADDR_LEN 1
#define SPD_EEPROM_ADDRESS1	0x51
#define SPD_EEPROM_ADDRESS2	0x53
#define SPD_EEPROM_ADDRESS	SPD_EEPROM_ADDRESS1
#define CONFIG_SYS_SPD_BUS_NUM	0	/* SPD on I2C bus 0 */
#define CONFIG_DIMM_SLOTS_PER_CTLR		1
#define CONFIG_CHIP_SELECTS_PER_CTRL	4
#undef	CONFIG_NR_DRAM_BANKS
#define CONFIG_NR_DRAM_BANKS			2
#define CONFIG_FSL_DDR_BIST	/* enable built-in memory test */

#define CONFIG_CMD_MEMINFO
#define CONFIG_CMD_MEMTEST
#define CONFIG_SYS_ALT_MEMTEST
#define CONFIG_SYS_MEMTEST_START	0x80000000
#define CONFIG_SYS_MEMTEST_END		0x9fffffff

/* SATA */
#define CONFIG_CMD_SCSI
#define CONFIG_LIBATA
#define CONFIG_SCSI_AHCI
#define CONFIG_SCSI_AHCI_PLAT
#define CONFIG_LBA48
#define CONFIG_SYS_64BIT_LBA

#define CONFIG_SYS_SATA1			AHCI_BASE_ADDR1
#define CONFIG_SYS_SATA2			AHCI_BASE_ADDR2

#define CONFIG_SYS_SCSI_MAX_SCSI_ID		1
#define CONFIG_SYS_SCSI_MAX_LUN			1
#define CONFIG_SYS_SCSI_MAX_DEVICE		(CONFIG_SYS_SCSI_MAX_SCSI_ID * \
						CONFIG_SYS_SCSI_MAX_LUN)

/*#define CONFIG_CMD_SF*/
#define CONFIG_SPI_FLASH
#define CONFIG_SPI_FLASH_STMICRO
#define CONFIG_SPI_FLASH_BAR

/* Debug Server firmware */
#undef CONFIG_FSL_DEBUG_SERVER

#define CONFIG_SYS_LS_MC_BOOT_TIMEOUT_MS 5000

/*
 * RTC configuration
 */
#define RTC
#define CONFIG_RTC_DS3231               1
#define CONFIG_SYS_I2C_RTC_ADDR         0x68
/*//#define CONFIG_CMD_DATE*/

#define CONFIG_FSL_MEMAC

/*  MMC  */
/*#define CONFIG_MMC
#define CONFIG_CMD_MMC*/
#define CONFIG_FSL_ESDHC
#define CONFIG_SYS_FSL_MMC_HAS_CAPBLT_VS33
/*#define CONFIG_GENERIC_MMC*/
/*#define CONFIG_CMD_FAT
#define CONFIG_DOS_PARTITION*/

#define CONFIG_MISC_INIT_R	/* call misc_init_r() */

/*
 * USB
 */
/*#define CONFIG_HAS_FSL_XHCI_USB*/
/*#define CONFIG_USB_XHCI*/
#define CONFIG_USB_XHCI_FSL
/*#define CONFIG_USB_XHCI_DWC3*/
/*#define CONFIG_USB_MAX_CONTROLLER_COUNT         2*/
/*#define CONFIG_SYS_USB_XHCI_MAX_ROOT_PORTS      2/
 */
/*#define CONFIG_CMD_USB
#define CONFIG_USB_STORAGE
#define CONFIG_CMD_EXT2*/

/* MAC/PHY configuration */
#ifdef CONFIG_FSL_MC_ENET
#define CONFIG_PHYLIB_10G
#define CONFIG_PHYLIB

#define CONFIG_MII
#define CONFIG_ETHPRIME		"DPMAC13@qsgmii"
#define CONFIG_PHY_GIGE
#endif

#include <asm/fsl_secure_boot.h>

#endif /* __LS2_RDB_H */
