/*
 * originially written by: Kirk Reiser <kirk@braille.uwo.ca>
* this version considerably modified by David Borowski, david575@rogers.com
* eventually modified by Samuel Thibault <samuel.thibault@ens-lyon.org>

 * Copyright (C) 1998-99  Kirk Reiser.
 * Copyright (C) 2003 David Borowski.
 * Copyright (C) 2007 Samuel Thibault.
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
 *
 * specificly written as a driver for the speakup screenreview
 * s not a general device driver.
 */
#include "spk_priv.h"
#include "serialio.h"

#define MY_SYNTH synth_dummy
#define DRV_VERSION "1.8"

static const char *synth_immediate(const char *buf);
static void do_catch_up(unsigned long data);
static void synth_flush(void);
static int synth_is_alive(void);

static const char init_string[] = "\x05Z\x05\x43";

static struct st_string_var stringvars[] = {
	{ CAPS_START, "CAPS_START\n" },
	{ CAPS_STOP, "CAPS_STOP" },
	V_LAST_STRING
};
static struct st_num_var numvars[] = {
	{ RATE, "RATE %d\n", 8, 1, 16, 0, 0, 0 },
	{ PITCH, "PITCH %d\n", 8, 0, 16, 0, 0, 0 },
	{ VOL, "VOL %d\n", 8, 0, 16, 0, 0, 0 },
	{ TONE, "TONE %d\n", 8, 0, 16, 0, 0, 0 },
	V_LAST_NUM
};

struct spk_synth synth_dummy = {"dummy", DRV_VERSION, "Dummy",
	init_string, 500, 50, 50, 5000, 0, 0, SYNTH_CHECK,
	stringvars, numvars, serial_synth_probe, spk_serial_release, synth_immediate,
	do_catch_up, NULL, synth_flush, synth_is_alive, NULL, NULL, NULL,
	{NULL, 0, 0, 0} };

static void do_catch_up(unsigned long data)
{
	unsigned long jiff_max = jiffies+speakup_info.jiffy_delta;
	u_char ch;
	synth_stop_timer();
	while (speakup_info.buff_out < speakup_info.buff_in) {
		ch = *speakup_info.buff_out;
		if (!spk_serial_out(ch)) {
			synth_delay(speakup_info.full_time);
			return;
		}
		speakup_info.buff_out++;
		if (jiffies >= jiff_max && ch == ' ') {
			spk_serial_out(' ');
			synth_delay(speakup_info.delay_time);
			return;
		}
	}
	spk_serial_out('\n');
	synth_done();
}

static const char *synth_immediate(const char *buf)
{
	u_char ch;
	while ((ch = *buf)) {
		if (wait_for_xmitr())
			outb(ch, speakup_info.port_tts);
		else
			return buf;
		buf++;
	}
	return 0;
}

static void synth_flush(void)
{
}

static int synth_is_alive(void)
{
	if (speakup_info.alive)
		return 1;
	if (!speakup_info.alive && wait_for_xmitr() > 0) {
		/* restart */
		speakup_info.alive = 1;
		synth_printf("%s",MY_SYNTH.init);
		return 2;
	}
	pr_warn("%s: can't restart synth\n", MY_SYNTH.long_name);
	return 0;
}

module_param_named(start, MY_SYNTH.flags, short, S_IRUGO);

static int __init dummy_init(void)
{
	return synth_add(&MY_SYNTH);
}

static void __exit dummy_exit(void)
{
	synth_remove(&MY_SYNTH);
}

module_init(dummy_init);
module_exit(dummy_exit);
MODULE_AUTHOR("Samuel Thibault <samuel.thibault@ens-lyon.org>");
MODULE_DESCRIPTION("Speakup support for text console");
MODULE_LICENSE("GPL");
MODULE_VERSION(DRV_VERSION);

