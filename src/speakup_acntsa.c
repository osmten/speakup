/*
 * originially written by: Kirk Reiser <kirk@braille.uwo.ca>
* this version considerably modified by David Borowski, david575@rogers.com

 * Copyright (C) 1998-99  Kirk Reiser.
 * Copyright (C) 2003 David Borowski.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

 * this code is specificly written as a driver for the speakup screenreview
 * package and is not a general device driver.
 */

#include <linux/jiffies.h>

#include "spk_priv.h"
#include "serialio.h"
#include "speakup_acnt.h" /* local header file for Accent values */

#define MY_SYNTH synth_acntsa
#define DRV_VERSION "1.8"
#define synth_full() (spk_serial_in() == 'F')
#define PROCSPEECH '\r'

static int synth_probe(void);

static const char init_string[] = "\033T2\033=M\033Oi\033N1\n";

static struct st_string_var stringvars[] = {
	{ CAPS_START, "\033P8" },
	{ CAPS_STOP, "\033P5" },
	V_LAST_STRING
};
static struct st_num_var numvars[] = {
	{ RATE, "\033R%c", 9, 0, 17, 0, 0, "0123456789abcdefgh" },
	{ PITCH, "\033P%d", 5, 0, 9, 0, 0, 0 },
	{ VOL, "\033A%d", 9, 0, 9, 0, 0, 0 },
	{ TONE, "\033V%d", 5, 0, 9, 0, 0, 0 },
	V_LAST_NUM
};

static struct spk_synth synth_acntsa = {
	.name = "acntsa",
	.version = DRV_VERSION,
	.long_name = "Accent-SA",
	.init = init_string,
	.procspeech = PROCSPEECH,
	.clear = SYNTH_CLEAR,
	.delay = 400,
	.trigger = 5,
	.jiffies = 30,
	.full = 1000,
	.flush_wait = 0,
	.flags = SYNTH_START,
	.checkval = SYNTH_CHECK,
	.string_vars = stringvars,
	.num_vars = numvars,
	.probe = synth_probe,
	.release = spk_serial_release,
	.synth_immediate = spk_synth_immediate,
	.catch_up = spk_do_catch_up,
	.start = NULL,
	.flush = spk_synth_flush,
	.is_alive = spk_synth_is_alive_restart,
	.synth_adjust = NULL,
	.read_buff_add = NULL,
	.get_index = NULL,
	.indexing = {
		.command = NULL,
		.lowindex = 0,
		.highindex = 0,
		.currindex = 0,
	}
};

static int synth_probe(void)
{
	int failed = 0;

	failed = serial_synth_probe();
	if (failed == 0) {
		spk_synth_immediate(&MY_SYNTH, "\033=R\r");
		mdelay(100);
	}
	return failed;
}

module_param_named(start, MY_SYNTH.flags, short, S_IRUGO);

static int __init acntsa_init(void)
{
	return synth_add(&MY_SYNTH);
}

static void __exit acntsa_exit(void)
{
	synth_remove(&MY_SYNTH);
}

module_init(acntsa_init);
module_exit(acntsa_exit);
MODULE_AUTHOR("Kirk Reiser <kirk@braille.uwo.ca>");
MODULE_AUTHOR("David Borowski");
MODULE_DESCRIPTION("Speakup support for Accent SA synthesizer");
MODULE_LICENSE("GPL");
MODULE_VERSION(DRV_VERSION);

