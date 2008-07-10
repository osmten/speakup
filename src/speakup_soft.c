/* speakup_soft.c - speakup driver to register and make available
 * a user space device for software synthesizers.  written by: Kirk
 * Reiser <kirk@braille.uwo.ca>
 *
 * Copyright (C) 2003  Kirk Reiser.
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
 * this code is specificly written as a driver for the speakup screenreview
 * package and is not a general device driver.  */

#include <linux/unistd.h>
#include <linux/miscdevice.h> /* for misc_register, and SYNTH_MINOR */
#include <linux/poll.h> /* for poll_wait() */
#include "spk_priv.h"
#include "speakup.h"

#define DRV_VERSION "2.4"
#define SOFTSYNTH_MINOR 26 /* might as well give it one more than /dev/synth */
#define PROCSPEECH 0x0d
#define CLEAR_SYNTH 0x18

static int softsynth_probe(struct spk_synth *synth);
static void softsynth_release(void);
static int softsynth_is_alive(struct spk_synth *synth);
static unsigned char get_index(void);

static struct miscdevice synth_device;
static int misc_registered;

static struct var_t vars[] = {
	{ CAPS_START, .u.s = {"\x01+3p" }},
	{ CAPS_STOP, .u.s = {"\x01-3p" }},
	{ RATE, .u.n = {"\x01%ds", 5, 0, 9, 0, 0, NULL }},
	{ PITCH, .u.n = {"\x01%dp", 5, 0, 9, 0, 0, NULL }},
	{ VOL, .u.n = {"\x01%dv", 5, 0, 9, 0, 0, NULL }},
	{ TONE, .u.n = {"\x01%dx", 1, 0, 2, 0, 0, NULL }},
	{ PUNCT, .u.n = {"\x01%db", 7, 0, 15, 0, 0, NULL }},
	{ VOICE, .u.n = {"\x01%do", 0, 0, 7, 0, 0, NULL }},
	{ FREQUENCY, .u.n = {"\x01%df", 5, 0, 9, 0, 0, NULL }},
	V_LAST_VAR
};

static struct spk_synth synth_soft = {
	.name = "soft",
	.version = DRV_VERSION,
	.long_name = "software synth",
	.init = "\01@\x01\x31y\n",
	.procspeech = PROCSPEECH,
	.delay = 0,
	.trigger = 0,
	.jiffies = 0,
	.full = 0,
	.flush_wait = 0,
	.startup = SYNTH_START,
	.checkval = SYNTH_CHECK,
	.vars = vars,
	.probe = softsynth_probe,
	.release = softsynth_release,
	.synth_immediate = NULL,
	.catch_up = NULL,
	.flush = NULL,
	.is_alive = softsynth_is_alive,
	.synth_adjust = NULL,
	.read_buff_add = NULL,
	.get_index = get_index,
	.indexing = {
		.command = "\x01%di",
		.lowindex = 1,
		.highindex = 5,
		.currindex = 1,
	}
};

static int softsynth_open(struct inode *inode, struct file *fp)
{
	unsigned long flags;
	/*if ((fp->f_flags & O_ACCMODE) != O_RDONLY) */
	/*	return -EPERM; */
	spk_lock(flags);
	if (synth_soft.alive) {
		spk_unlock(flags);
		return -EBUSY;
	}
	synth_soft.alive = 1;
	spk_unlock(flags);
	return 0;
}

static int softsynth_close(struct inode *inode, struct file *fp)
{
	unsigned long flags;
	fp->f_op = NULL;
	spk_lock(flags);
	synth_soft.alive = 0;
	spk_unlock(flags);
	/* Make sure we let applications go before leaving */
	speakup_start_ttys();
	return 0;
}

static ssize_t softsynth_read(struct file *fp, char *buf, size_t count,
			      loff_t *pos)
{
	int chars_sent = 0;
	char *cp;
	char ch;
	int empty;
	unsigned long flags;
	DEFINE_WAIT(wait);

	spk_lock(flags);
	while (1) {
		prepare_to_wait(&speakup_event, &wait, TASK_INTERRUPTIBLE);
		if (!synth_buffer_empty() || speakup_info.flushing)
			break;
		spk_unlock(flags);
		if (fp->f_flags & O_NONBLOCK) {
			finish_wait(&speakup_event, &wait);
			return -EAGAIN;
		}
		if (signal_pending(current)) {
			finish_wait(&speakup_event, &wait);
			return -ERESTARTSYS;
		}
		schedule();
		spk_lock(flags);
	}
	finish_wait(&speakup_event, &wait);

	cp = buf;
	while (chars_sent < count) {
		if (speakup_info.flushing) {
			speakup_info.flushing = 0;
			ch = '\x18';
		} else if (synth_buffer_empty()) {
			break;
		} else {
			ch = synth_buffer_getc();
		}
		spk_unlock(flags);
		if (copy_to_user(cp, &ch, 1))
			return -EFAULT;
		spk_lock(flags);
		chars_sent++;
		cp++;
	}
	*pos += chars_sent;
	empty = synth_buffer_empty();
	spk_unlock(flags);
	if (empty) {
		speakup_start_ttys();
		*pos = 0;
	}
	return chars_sent;
}

static int last_index = 0;

static ssize_t softsynth_write(struct file *fp, const char *buf, size_t count,
			       loff_t *pos)
{
	char indbuf[5];
	if (count >= sizeof(indbuf))
		return -EINVAL;

	if (copy_from_user(indbuf, buf, count))
		return -EFAULT;
	indbuf[4] = 0;

	last_index = simple_strtoul(indbuf, NULL, 0);
	return count;
}

static unsigned int softsynth_poll(struct file *fp,
		struct poll_table_struct *wait)
{
	unsigned long flags;
	int ret = 0;
	poll_wait(fp, &speakup_event, wait);

	spk_lock(flags);
	if (! synth_buffer_empty() || speakup_info.flushing)
		ret = POLLIN | POLLRDNORM;
	spk_unlock(flags);
	return ret;
}

static unsigned char get_index(void)
{
	int rv;
	rv = last_index;
	last_index = 0;
	return rv;
}

static struct file_operations softsynth_fops = {
	.poll = softsynth_poll,
	.read = softsynth_read,
	.write = softsynth_write,
	.open = softsynth_open,
	.release = softsynth_close,
};


static int softsynth_probe(struct spk_synth *synth)
{

	if (misc_registered != 0)
		return 0;
	memset(&synth_device, 0, sizeof(synth_device));
	synth_device.minor = SOFTSYNTH_MINOR;
	synth_device.name = "softsynth";
	synth_device.fops = &softsynth_fops;
	if (misc_register(&synth_device)) {
		pr_warn("Couldn't initialize miscdevice /dev/softsynth.\n");
		return -ENODEV;
	}

	misc_registered = 1;
	pr_info("initialized device: /dev/softsynth, node (MAJOR 10, MINOR 26)\n");
	return 0;
}

static void softsynth_release(void)
{
	misc_deregister(&synth_device);
	misc_registered = 0;
	pr_info("unregistered /dev/softsynth\n");
}

static int softsynth_is_alive(struct spk_synth *synth)
{
	if (synth_soft.alive)
		return 1;
	return 0;
}

module_param_named(start, synth_soft.startup, short, S_IRUGO);

MODULE_PARM_DESC(start, "Start the synthesizer once it is loaded.");


static int __init soft_init(void)
{
	return synth_add(&synth_soft);
}

static void __exit soft_exit(void)
{
	synth_remove(&synth_soft);
}

module_init(soft_init);
module_exit(soft_exit);
MODULE_AUTHOR("Kirk Reiser <kirk@braille.uwo.ca>");
MODULE_DESCRIPTION("Speakup userspace software synthesizer support");
MODULE_LICENSE("GPL");
MODULE_VERSION(DRV_VERSION);

