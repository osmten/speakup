/*
* written by David Borowski

		Copyright (C) 2003 David Borowski.

		This program is free software; you can redistribute it and/or modify
		it under the terms of the GNU General Public License as published by
		the Free Software Foundation; either version 2 of the License, or
		(at your option) any later version.

		This program is distributed in the hope that it will be useful,
		but WITHOUT ANY WARRANTY; without even the implied warranty of
		MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
		GNU General Public License for more details.

		You should have received a copy of the GNU General Public License
		along with this program; if not, write to the Free Software
		Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

 * this code is specificly written as a driver for the speakup screenreview
 * package and is not a general device driver.
		*/
#include "spk_priv.h"

#define MY_SYNTH synth_keypc
#define SYNTH_IO_EXTENT	0x04
#define SWAIT udelay(70)
#define synth_writable() (inb_p(synth_port) & 0x10)
#define synth_readable() (inb_p(synth_port) & 0x10)
#define synth_full() ((inb_p(synth_port) & 0x80) == 0)
#define PROCSPEECH 0x1f
#define SYNTH_CLEAR 0x03

static int synth_port;
static unsigned int synth_portlist[] = { 0x2a8, 0 };

static int
oops(void)
{
	int s1, s2, s3, s4;
	s1 = inb_p(synth_port);
	s2 = inb_p(synth_port+1);
	s3 = inb_p(synth_port+2);
	s4 = inb_p(synth_port+3);
	pr_warn("synth timeout %d %d %d %d\n", s1, s2, s3, s4);
	return 0;
}

static const char *synth_immediate(const char *buf)
{
	u_char ch;
	int timeout;
	while ((ch = *buf)) {
		if (ch == '\n') ch = PROCSPEECH;
		if (synth_full())
			return buf;
		timeout = 1000;
		while (synth_writable())
			if (--timeout <= 0) return (char *) oops();
		outb_p(ch, synth_port);
		SWAIT;
		buf++;
	}
	return 0;
}

static void do_catch_up(unsigned long data)
{
	unsigned long jiff_max = jiffies+synth_jiffy_delta;
	u_char ch;
	int timeout;
	synth_stop_timer();
	while (synth_buff_out < synth_buff_in) {
 		if (synth_full()) {
			synth_delay(synth_full_time);
			return;
		}
		timeout = 1000;
		while (synth_writable())
			if (--timeout <= 0) break;
		if (timeout <= 0) {
			oops();
			break;
		}
		ch = *synth_buff_out++;
		if (ch == '\n') ch = PROCSPEECH;
		outb_p(ch, synth_port);
		SWAIT;
		if (jiffies >= jiff_max && ch == SPACE) {
			timeout = 1000;
			while (synth_writable())
				if (--timeout <= 0) break;
			if (timeout <= 0) {
				oops();
				break;
			}
			outb_p(PROCSPEECH, synth_port);
			synth_delay(synth_delay_time);
			return;
		}
	}
	timeout = 1000;
	while (synth_writable())
		if (--timeout <= 0) break;
	if (timeout <= 0) oops();
	else outb_p(PROCSPEECH, synth_port);
	synth_done();
}

static void synth_flush(void)
{
	outb_p(SYNTH_CLEAR, synth_port);
}

static int synth_probe(void)
{
	unsigned int port_val = 0;
	int i = 0;
	pr_info("Probing for %s.\n", synth->long_name);
	if (synth_port_forced) {
		synth_port = synth_port_forced;
		pr_info("probe forced to %x by kernel command line\n", synth_port);
		if (synth_request_region(synth_port-1, SYNTH_IO_EXTENT)) {
			pr_warn("sorry, port already reserved\n");
			return -EBUSY;
		}
		port_val = inb(synth_port);
	} else {
		for (i=0; synth_portlist[i]; i++) {
			if (synth_request_region(synth_portlist[i], SYNTH_IO_EXTENT)) {
				pr_warn("request_region: failed with 0x%x, %d\n",
					synth_portlist[i], SYNTH_IO_EXTENT);
				continue;
			}
			port_val = inb(synth_portlist[i]);
			if (port_val == 0x80) {
				synth_port = synth_portlist[i];
				break;
			}
		}
	}
	if (port_val != 0x80) {
		pr_info("%s: not found\n", synth->long_name);
		synth_release_region(synth_portlist[i], SYNTH_IO_EXTENT);
		synth_port = 0;
		return -ENODEV;
	}
	pr_info("%s: %03x-%03x, driver version %s,\n", synth->long_name,
		synth_port,	synth_port+SYNTH_IO_EXTENT-1,
		synth->version);
	return 0;
}

static void keynote_release(void)
{
	if (synth_port)
		synth_release_region(synth_port, SYNTH_IO_EXTENT);
	synth_port = 0;
}

static int synth_is_alive(void)
{
	synth_alive = 1;
	return 1;
}

static const char init_string[] = "[t][n7,1][n8,0]";

static struct st_string_var stringvars[] = {
	{ CAPS_START, "[f130]" },
	{ CAPS_STOP, "[f90]" },
	V_LAST_STRING
};
static struct st_num_var numvars[] = {
	{ RATE, "\04%c ", 8, 0, 10, 81, -8, 0 },
	{ PITCH, "[f%d]", 5, 0, 9, 40, 10, 0 },
	V_LAST_NUM
};

struct spk_synth synth_keypc = {"keypc", "1.1", "Keynote PC",
	 init_string, 500, 50, 50, 1000, 0, 0, SYNTH_CHECK,
	stringvars, numvars, synth_probe, keynote_release, synth_immediate,
	do_catch_up, NULL, synth_flush, synth_is_alive, NULL, NULL, NULL,
	{NULL,0,0,0} };

static int __init keypc_init(void)
{
	return synth_add(&MY_SYNTH);
}

static void __exit keypc_exit(void)
{
	synth_remove(&MY_SYNTH);
}

module_init(keypc_init);
module_exit(keypc_exit);
MODULE_AUTHOR("David Borowski");
MODULE_DESCRIPTION("Speakup support for Keynote Gold PC synthesizers");
MODULE_LICENSE("GPL");

