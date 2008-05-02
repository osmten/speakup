/*
 * originially written by: Kirk Reiser <kirk@braille.uwo.ca>
 * this version considerably modified by David Borowski, david575@rogers.com
 *
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
 *
 * specificly written as a driver for the speakup screenreview
 * s not a general device driver.
 */
#include <linux/jiffies.h>

#include "spk_priv.h"
#include "serialio.h"

#define MY_SYNTH synth_audptr
#define DRV_VERSION "1.8"
#define SYNTH_CLEAR 0x18 /* flush synth buffer */
#define PROCSPEECH '\r' /* start synth processing speech char */

static int synth_probe(void);
static const char *synth_immediate(const char *buf);
static void do_catch_up(unsigned long data);
static void synth_flush(void);
static int synth_is_alive(void);

static const char init_string[] = "\x05[D1]\x05[Ol]";

static struct st_string_var stringvars[] = {
	{ CAPS_START, "\x05[f99]" },
	{ CAPS_STOP, "\x05[f80]" },
	V_LAST_STRING
};
static struct st_num_var numvars[] = {
	{ RATE, "\x05[r%d]", 10, 0, 20, -100, 10, 0 },
	{ PITCH, "\x05[f%d]", 80, 39, 4500, 0, 0, 0 },
	{ VOL, "\x05[g%d]", 21, 0, 40, 0, 0, 0 },
	{ TONE, "\x05[s%d]", 9, 0, 63, 0, 0, 0 },
	{ PUNCT, "\x05[A%c]", 0, 0, 3, 0, 0, "nmsa" },
	V_LAST_NUM
};

struct spk_synth synth_audptr = {"audptr", DRV_VERSION, "Audapter",
	 init_string, 400, 5, 30, 5000, 0, 0, SYNTH_CHECK,
	stringvars, numvars, synth_probe, spk_serial_release, synth_immediate,
	do_catch_up, NULL, synth_flush, synth_is_alive, NULL, NULL, NULL,
	{NULL, 0, 0, 0} };

static void do_catch_up(unsigned long data)
{
	unsigned long jiff_max = jiffies+speakup_info.jiffy_delta;
	u_char ch;
	synth_stop_timer();
	while (speakup_info.buff_out < speakup_info.buff_in) {
		ch = *speakup_info.buff_out;
		if (ch == '\n')
			ch = PROCSPEECH;
		if (!spk_serial_out(ch)) {
			synth_delay(speakup_info.full_time);
			return;
		}
		speakup_info.buff_out++;
		if (jiffies >= jiff_max && ch == SPACE) {
			spk_serial_out(PROCSPEECH);
			synth_delay(speakup_info.delay_time);
			return;
		}
	}
	spk_serial_out(PROCSPEECH);
	synth_done();
}

static const char *synth_immediate(const char *buf)
{
	u_char ch;
	while ((ch = *buf)) {
		if (ch == '\n')
			ch = PROCSPEECH;
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
	while ((inb(speakup_info.port_tts + UART_LSR) & BOTH_EMPTY) != BOTH_EMPTY);
	outb(SYNTH_CLEAR, speakup_info.port_tts);
	spk_serial_out(PROCSPEECH);
}

static void synth_version(void)
{
	unsigned char test = 0;
	char synth_id[40] = "";
	synth_immediate("\x05[Q]");
	synth_id[test] = spk_serial_in();
	if (synth_id[test] == 'A') {
		do {
			/* read version string from synth */
			synth_id[++test] = spk_serial_in();
		} while (synth_id[test] != '\n' && test < 32);
		synth_id[++test] = 0x00;
	}
	if (synth_id[0] == 'A')
		pr_info("%s version: %s", MY_SYNTH.long_name, synth_id);
}

static int synth_probe(void)
{
	int failed = 0;

	failed = serial_synth_probe();
	if (failed == 0) 
		synth_version();
	return 0;
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
	return 0;
}

module_param_named(start, MY_SYNTH.flags, short, S_IRUGO);

static int __init audptr_init(void)
{
	return synth_add(&MY_SYNTH);
}

static void __exit audptr_exit(void)
{
	synth_remove(&MY_SYNTH);
}

module_init(audptr_init);
module_exit(audptr_exit);
MODULE_AUTHOR("Kirk Reiser <kirk@braille.uwo.ca>");
MODULE_AUTHOR("David Borowski");
MODULE_DESCRIPTION("Speakup support for Audapter synthesizer");
MODULE_LICENSE("GPL");
MODULE_VERSION(DRV_VERSION);

