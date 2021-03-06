/* Copyright (c) 2010, Code Aurora Forum. All rights reserved.
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

#include <linux/init.h>
#include <linux/wait.h>
#include <linux/gpio.h>
#include <mach/pmic.h>
#include <mach/msm_qdsp6_audio.h>
#include <asm/string.h>
#include <asm/mach-types.h>
#include <mach/debug_mm.h>
#ifdef CONFIG_AUDIO_TPA2018
#include <mach/tpa2018.h>
#endif
#ifdef CONFIG_ACER_HEADSET
#include <mach/acer_headset.h>
#endif

void analog_init(void)
{
	/* stereo pmic init */
	pmic_spkr_set_gain(LEFT_SPKR, SPKR_GAIN_PLUS12DB);
	pmic_spkr_set_gain(RIGHT_SPKR, SPKR_GAIN_PLUS12DB);
#ifdef CONFIG_MACH_ACER_A1
	pmic_mic_set_volt(MIC_VOLT_2_00V);
#else
	pmic_mic_set_volt(MIC_VOLT_1_80V);
#endif
}

void analog_headset_enable(int en)
{
	/* enable audio amp */
	if(en) {
#ifdef CONFIG_ACER_HEADSET
		hs_amp(true);
#endif
		pr_info("[Audio] Enable HS \n");
	} else {
#ifdef CONFIG_ACER_HEADSET
		hs_amp(false);
#endif
		pr_info("[Audio] Disable HS \n");
	}
}

void analog_speaker_enable(int en)
{
	struct spkr_config_mode scm;
	memset(&scm, 0, sizeof(scm));

	pr_debug("[%s:%s] en = %d\n", __MM_FILE__, __func__, en);
	if (en) {
		scm.is_right_chan_en = 1;
		scm.is_left_chan_en = 1;
		scm.is_stereo_en = 1;
		scm.is_hpf_en = 1;
		pmic_spkr_en_mute(LEFT_SPKR, 0);
		pmic_spkr_en_mute(RIGHT_SPKR, 0);
		pmic_set_spkr_configuration(&scm);
		pmic_spkr_en(LEFT_SPKR, 1);
		pmic_spkr_en(RIGHT_SPKR, 1);
		
#ifdef CONFIG_AUDIO_TPA2018
		set_adie_flag(1);
		tpa2018_software_shutdown(0);
#endif
		pr_info("[Audio] Enable Speaker AMP \n");

		/* unmute */
		pmic_spkr_en_mute(LEFT_SPKR, 1);
		pmic_spkr_en_mute(RIGHT_SPKR, 1);
	} else {
		pmic_spkr_en_mute(LEFT_SPKR, 0);
		pmic_spkr_en_mute(RIGHT_SPKR, 0);
		
#ifdef CONFIG_AUDIO_TPA2018
		set_adie_flag(0);
		tpa2018_software_shutdown(1);
#endif
		pr_info("[Audio] Disable Speaker AMP \n");
		
		pmic_spkr_en(LEFT_SPKR, 0);
		pmic_spkr_en(RIGHT_SPKR, 0);

		pmic_set_spkr_configuration(&scm);
	}
}

void analog_mic_enable(int en)
{
	pr_debug("[%s:%s] en = %d\n", __MM_FILE__, __func__, en);
	pmic_mic_en(en);
}

static struct q6audio_analog_ops ops = {
	.init = analog_init,
	.speaker_enable = analog_speaker_enable,
	.headset_enable = analog_headset_enable,
	.int_mic_enable = analog_mic_enable,
	.ext_mic_enable = analog_mic_enable,
};

static int __init init(void)
{
	q6audio_register_analog_ops(&ops);
	return 0;
}

device_initcall(init);
