/*
 * Copyright 2016 Imago Technologies GmbH
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */
//#define DEBUG
#include <common.h>
#include <fsl_ddr.h>

DECLARE_GLOBAL_DATA_PTR;

struct raw_card_parameters {
	u8 raw_card;
	u8 raw_card_rev;
	u32 n_ranks;
//	u32 clk_adjust;
	u16 dq_length[9];
	u16 cmd_length[9];
};

const struct raw_card_parameters udimm_modules[] = {
		{0, 0, 1, {24, 23, 23, 24, 24, 24, 23, 24, 0}, {44, 73, 31, 86, 111, 174, 124, 161, 0}}, // A0
		{0, 1, 1, {17, 15, 17, 14, 15, 17, 15, 17, 0}, {74, 74, 87, 87, 114, 114, 127, 127, 0}}, // A1
		{1, 1, 2, {18, 22, 22, 22, 22, 20, 21, 18, 0}, {54, 79, 29, 104, 133, 208, 158, 182, 0}}, // B1
		{2, 0, 1, {17, 16, 17, 14, 16, 16, 16, 16, 0}, {82, 82, 95, 95, 120, 120, 134, 134, 0}}, // C0
		{3, 0, 1, {26, 24, 23, 23, 24, 23, 24, 23, 24}, {44, 73, 31, 86, 111, 174, 124, 161, 98}}, // D0
		// D1 should be similar to A1, ECC byte estimated in between dq3 and dq4:
		{3, 1, 1, {17, 15, 17, 14, 15, 17, 15, 17, 16}, {74, 74, 87, 87, 114, 114, 127, 127, 100}}, // D1
		{4, 1, 2, {17, 21, 18, 21, 22, 21, 18, 21, 0}, {51, 69, 36, 83, 110, 157, 123, 142, 0}}, // E1
		{6, 0, 2, {14, 24, 19, 25, 24, 19, 24, 13, 27}, {54, 72, 36, 85, 113, 162, 126, 144, 99}}, // G0
		{6, 1, 2, {17, 22, 19, 22, 22, 19, 22, 17, 22}, {52, 69, 36, 83, 111, 159, 125, 144, 95}}, // G1
		{7, 0, 2, {18, 22, 22, 22, 22, 20, 22, 18, 23}, {56, 81, 31, 101, 141, 211, 161, 186, 121}}, // H0
		{}
};

static void calc_wrlevel(memctl_options_t *popts,
		const struct raw_card_parameters *pRawCard,
		unsigned int ctrl_num)
{
	const ulong clock_freq = get_ddr_freq(ctrl_num) / 1000 / 2;	// clock in kHz
	const u16 board_dq_length[2][9] = {
			{88, 82, 86, 64, 72, 72, 75, 75, 59},
			{96, 98, 92, 83, 85, 81, 86, 85, 79}};
	const u16 board_cmd_length[2] = {76, 91};
	const ulong signalVelocity = 140 * 1000 * 1000;	// for Microstrip in m / s
	int length_diff;
	u8 wr_level[9];
	int i;

	popts->clk_adjust = 9;	// Adjust dynamically instead?

	printf("wr_level:"); 
	for (i=0; i<9; i++)
	{
		length_diff = (board_cmd_length[ctrl_num] + pRawCard->cmd_length[i]) -
				(board_dq_length[ctrl_num][i] + pRawCard->dq_length[i]);
		wr_level[i] = (8*clock_freq*length_diff + popts->clk_adjust*signalVelocity/2 + signalVelocity/2) / signalVelocity;
		printf(" %u ", wr_level[i]); 
	}
	printf("\n"); 

	popts->wrlvl_start = wr_level[0];
	popts->wrlvl_ctl_2 = (wr_level[1]<<24) + (wr_level[2]<<16) + (wr_level[3]<<8) + wr_level[4];
	popts->wrlvl_ctl_3 = (wr_level[5]<<24) + (wr_level[6]<<16) + (wr_level[7]<<8) + wr_level[8];
}

void fsl_ddr_board_options(memctl_options_t *popts,
				dimm_params_t *pdimm,
				unsigned int ctrl_num)
{
#ifdef CONFIG_SYS_FSL_HAS_DP_DDR
	u8 dq_mapping_0, dq_mapping_2, dq_mapping_3;
#endif
	ulong ddr_freq;
	const int slot = 0;
	struct ddr4_spd_eeprom_s spd;

	if (ctrl_num > 1) {
		printf("Not supported controller number %d\n", ctrl_num);
		return;
	}

	/*
	 * we use identical timing for all slots. If needed, change the code
	 * to  pbsp = rdimms[ctrl_num] or pbsp = udimms[ctrl_num];
	 */
	if (popts->registered_dimm_en)
	{
		printf("SO-RDIMMs are not supported (controller %d)\n", ctrl_num);
		return;
	}

	memset(&spd, 0, sizeof(spd));
	fsl_ddr_get_spd(&spd, ctrl_num, CONFIG_DIMM_SLOTS_PER_CTLR);

	unsigned int rawCard = spd.mod_section.unbuffered.ref_raw_card & 0x1f;
	unsigned int rawCardRev;
	if ((spd.mod_section.unbuffered.mod_height & 0xe0) == 0)
		rawCardRev = (spd.mod_section.unbuffered.ref_raw_card >> 5) & 0x3;
	else
		rawCardRev = 3 + ((spd.mod_section.unbuffered.mod_height >> 5) & 0x7);
	
	// Fix known incorrect modules:
	// Card Rev. A0
	if (rawCard == 0 && rawCardRev == 0)
	{
		// Transcend with certain date and with Hynix chips incorrectly uses card rev. A0: 
		if (spd.mmid_lsb == 0x01 && spd.mmid_msb == 0x4f &&
			(256 * spd.mdate[0] + spd.mdate[1]) == 0x1624)
		{
			printf("Transcend SPD fix: using Raw Card Rev. A1 for write-leveling instead of A0\n");
			rawCardRev = 1;
		}
		// Micron incorrectly uses card rev. A0:
		else if (spd.mmid_lsb == 0x80 && spd.mmid_msb == 0x2c && pdimm->n_ranks == 1)
		{
			printf("Micron SPD fix: using Raw Card Rev. A1 for write-leveling instead of A0\n");
			rawCardRev = 1;
		}
	}
	// Card Rev. B1
	else if (rawCard == 1 && rawCardRev == 1)
	{
		// Crucial incorrectly uses card rev. B1: 
		if (spd.mmid_lsb == 0x85 && spd.mmid_msb == 0x9b && pdimm->n_ranks == 1)
		{
			printf("Crucial SPD fix: using Raw Card Rev. A1 for write-leveling instead of B1\n");
			rawCard = 0;
		}
	}

	printf("Using Raw Card Revision %c%u for write-leveling\n", 'A'+rawCard, rawCardRev);

	const struct raw_card_parameters *pRawCard = &udimm_modules[0];
	while (pRawCard->n_ranks != 0)
	{
		if (pRawCard->raw_card == rawCard && pRawCard->raw_card_rev == rawCardRev)
		{
			calc_wrlevel(popts, pRawCard, ctrl_num);
			break;
		}
		pRawCard++;
	}
	if (pRawCard->n_ranks == 0)
		panic("SO-DIMM Raw Card %c%u is not supported by this board!\n", 'A'+rawCard, rawCardRev);

	debug("pdimm[%u].rank_density = %u\n", slot, pdimm[slot].rank_density);
	debug("pdimm[%u].n_ranks = %u\n", slot, pdimm[slot].n_ranks);

	/* Get clk_adjust, wrlvl_start, wrlvl_ctl, according to the board ddr
	 * freqency and n_banks specified in board_specific_parameters table.
	 */
	ddr_freq = get_ddr_freq(ctrl_num) / 1000000;

	/* To work at higher than 1333MT/s */
	popts->half_strength_driver_enable = 0;
	/*
	 * Write leveling override
	 */
	popts->wrlvl_override = 1;
	popts->wrlvl_sample = 0x0;	/* 32 clocks */

	/*
	 * Rtt and Rtt_WR override
	 */
	popts->rtt_override = 0;

	/* Enable ZQ calibration */
	popts->zq_en = 1;

	if (ddr_freq < 2350) {
		if (pdimm[0].n_ranks == 2 && pdimm[1].n_ranks == 2) {
			/* four chip-selects */
			popts->ddr_cdr1 = DDR_CDR1_DHC_EN |
					  DDR_CDR1_ODT(DDR_CDR_ODT_80ohm);
			popts->ddr_cdr2 = DDR_CDR2_ODT(DDR_CDR_ODT_80ohm);
			popts->twot_en = 1;	/* enable 2T timing */
		} else {
			popts->ddr_cdr1 = DDR_CDR1_DHC_EN |
					  DDR_CDR1_ODT(DDR_CDR_ODT_60ohm);
			popts->ddr_cdr2 = DDR_CDR2_ODT(DDR_CDR_ODT_60ohm) |
					  DDR_CDR2_VREF_RANGE_2;
		}
	} else {
		popts->ddr_cdr1 = DDR_CDR1_DHC_EN |
				  DDR_CDR1_ODT(DDR_CDR_ODT_100ohm);
		popts->ddr_cdr2 = DDR_CDR2_ODT(DDR_CDR_ODT_100ohm) |
				  DDR_CDR2_VREF_RANGE_2;
	}
	
	popts->ecc_init_using_memctl = ((pdimm[slot].edc_config & EDC_ECC) != 0);
}

phys_size_t initdram(int board_type)
{
	phys_size_t dram_size;

#if defined(CONFIG_SPL) && !defined(CONFIG_SPL_BUILD)
	return fsl_ddr_sdram_size();
#else
	puts("Initializing DDR....using SPD\n");

	dram_size = fsl_ddr_sdram();
#endif

	return dram_size;
}

void dram_init_banksize(void)
{
	/*
	 * gd->secure_ram tracks the location of secure memory.
	 * It was set as if the memory starts from 0.
	 * The address needs to add the offset of its bank.
	 */
	gd->bd->bi_dram[0].start = CONFIG_SYS_SDRAM_BASE;
	if (gd->ram_size > CONFIG_SYS_LS2_DDR_BLOCK1_SIZE) {
		gd->bd->bi_dram[0].size = CONFIG_SYS_LS2_DDR_BLOCK1_SIZE;
		gd->bd->bi_dram[1].start = CONFIG_SYS_DDR_BLOCK2_BASE;
		gd->bd->bi_dram[1].size = gd->ram_size -
					  CONFIG_SYS_LS2_DDR_BLOCK1_SIZE;
#ifdef CONFIG_SYS_MEM_RESERVE_SECURE
		gd->secure_ram = gd->bd->bi_dram[1].start +
				 gd->secure_ram -
				 CONFIG_SYS_LS2_DDR_BLOCK1_SIZE;
		gd->secure_ram |= MEM_RESERVE_SECURE_MAINTAINED;
#endif
	} else {
		gd->bd->bi_dram[0].size = gd->ram_size;
#ifdef CONFIG_SYS_MEM_RESERVE_SECURE
		gd->secure_ram = gd->bd->bi_dram[0].start + gd->secure_ram;
		gd->secure_ram |= MEM_RESERVE_SECURE_MAINTAINED;
#endif
	}
}

