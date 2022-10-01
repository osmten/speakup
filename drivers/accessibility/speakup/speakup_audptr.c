// SPDX-License-Identifier: GPL-2.0+
/*
 * originally written by: Kirk Reiser <kirk@braille.uwo.ca>
 * this version considerably modified by David Borowski, david575@rogers.com
 *
 * Copyright (C) 1998-99  Kirk Reiser.
 * Copyright (C) 2003 David Borowski.
 *
 * specifically written as a driver for the speakup screenreview
 * s not a general device driver.
 */
#include "spk_priv.h"
#include "speakup.h"

#define DRV_VERSION "2.11"
#define SYNTH_CLEAR 0x18 /* flush synth buffer */
#define PROCSPEECH '\r' /* start synth processing speech char */

static int synth_probe(struct spk_synth *synth);
static void synth_flush(struct spk_synth *synth);


enum default_vars_id {
	CAPS_START_id=0,CAPS_STOP_id,
	RATE_id,PITCH_id,
	VOL_id,TONE_id,punct_id,
	DIRECT_id,
}



static struct var_t vars[] = {
	{ CAPS_START, .u.s = {"\x05[f99]" } },
	{ CAPS_STOP, .u.s = {"\x05[f80]" } },
	{ RATE, .u.n = {"\x05[r%d]", 10, 0, 20, 100, -10, NULL } },
	{ PITCH, .u.n = {"\x05[f%d]", 80, 39, 4500, 0, 0, NULL } },
	{ VOL, .u.n = {"\x05[g%d]", 21, 0, 40, 0, 0, NULL } },
	{ TONE, .u.n = {"\x05[s%d]", 9, 0, 63, 0, 0, NULL } },
	{ PUNCT, .u.n = {"\x05[A%c]", 0, 0, 3, 0, 0, "nmsa" } },
	{ DIRECT, .u.n = {NULL, 0, 0, 1, 0, 0, NULL } },
	V_LAST_VAR
};

/*
 * These attributes will appear in /sys/accessibility/speakup/audptr.
 */
static struct kobj_attribute caps_start_attribute =
	__ATTR(caps_start, 0644, spk_var_show, spk_var_store);
static struct kobj_attribute caps_stop_attribute =
	__ATTR(caps_stop, 0644, spk_var_show, spk_var_store);
static struct kobj_attribute pitch_attribute =
	__ATTR(pitch, 0644, spk_var_show, spk_var_store);
static struct kobj_attribute punct_attribute =
	__ATTR(punct, 0644, spk_var_show, spk_var_store);
static struct kobj_attribute rate_attribute =
	__ATTR(rate, 0644, spk_var_show, spk_var_store);
static struct kobj_attribute tone_attribute =
	__ATTR(tone, 0644, spk_var_show, spk_var_store);
static struct kobj_attribute vol_attribute =
	__ATTR(vol, 0644, spk_var_show, spk_var_store);

static struct kobj_attribute delay_time_attribute =
	__ATTR(delay_time, 0644, spk_var_show, spk_var_store);
static struct kobj_attribute direct_attribute =
	__ATTR(direct, 0644, spk_var_show, spk_var_store);
static struct kobj_attribute full_time_attribute =
	__ATTR(full_time, 0644, spk_var_show, spk_var_store);
static struct kobj_attribute jiffy_delta_attribute =
	__ATTR(jiffy_delta, 0644, spk_var_show, spk_var_store);
static struct kobj_attribute trigger_time_attribute =
	__ATTR(trigger_time, 0644, spk_var_show, spk_var_store);

/*
 * Create a group of attributes so that we can create and destroy them all
 * at once.
 */
static struct attribute *synth_attrs[] = {
	&caps_start_attribute.attr,
	&caps_stop_attribute.attr,
	&pitch_attribute.attr,
	&punct_attribute.attr,
	&rate_attribute.attr,
	&tone_attribute.attr,
	&vol_attribute.attr,
	&delay_time_attribute.attr,
	&direct_attribute.attr,
	&full_time_attribute.attr,
	&jiffy_delta_attribute.attr,
	&trigger_time_attribute.attr,
	NULL,	/* need to NULL terminate the list of attributes */
};

static struct spk_synth synth_audptr = {
	.name = "audptr",
	.version = DRV_VERSION,
	.long_name = "Audapter",
	.init = "\x05[D1]\x05[Ol]",
	.procspeech = PROCSPEECH,
	.clear = SYNTH_CLEAR,
	.delay = 400,
	.trigger = 50,
	.jiffies = 30,
	.full = 18000,
	.dev_name = SYNTH_DEFAULT_DEV,
	.startup = SYNTH_START,
	.checkval = SYNTH_CHECK,
	.vars = vars,
	.io_ops = &spk_ttyio_ops,
	.probe = synth_probe,
	.release = spk_ttyio_release,
	.synth_immediate = spk_ttyio_synth_immediate,
	.catch_up = spk_do_catch_up,
	.flush = synth_flush,
	.is_alive = spk_synth_is_alive_restart,
	.synth_adjust = NULL,
	.read_buff_add = NULL,
	.get_index = NULL,
	.indexing = {
		.command = NULL,
		.lowindex = 0,
		.highindex = 0,
		.currindex = 0,
	},
	.attributes = {
		.attrs = synth_attrs,
		.name = "audptr",
	},
};

static void synth_flush(struct spk_synth *synth)
{
	synth->io_ops->flush_buffer(synth);
	synth->io_ops->send_xchar(synth, SYNTH_CLEAR);
	synth->io_ops->synth_out(synth, PROCSPEECH);
}

static void synth_version(struct spk_synth *synth)
{
	unsigned i;
	char synth_id[33];

	synth->synth_immediate(synth, "\x05[Q]");
	synth_id[0] = synth->io_ops->synth_in(synth);
	if (synth_id[0] != 'A')
		return;

	for (i = 1; i < sizeof(synth_id) - 1; i++) {
		/* read version string from synth */
		synth_id[i] = synth->io_ops->synth_in(synth);
		if (synth_id[i] == '\n')
			break;
	}
	synth_id[i] = '\0';
	pr_info("%s version: %s", synth->long_name, synth_id);
}

static int synth_probe(struct spk_synth *synth)
{
	int failed;

	failed = spk_ttyio_synth_probe(synth);
	if (failed == 0)
		synth_version(synth);
	synth->alive = !failed;
	return 0;
}

module_param_named(ser, synth_audptr.ser, int, 0444);
module_param_named(dev, synth_audptr.dev_name, charp, 0444);
module_param_named(start, synth_audptr.startup, short, 0444);
module_param_named(caps_start, vars[CAPS_START_id].u.s.default_val, int, 0444);
module_param_named(caps_stop, vars[CAPS_STOP_id].u.s.default_val, int, 0444);
module_param_named(rate, vars[RATE_id].u.n.default_val, int, 0444);
module_param_named(pitch, vars[PITCH_id].u.n.default_val, int, 0444);
module_param_named(vol, vars[VOL_id].u.n.default_val, int, 0444);
module_param_named(tone, vars[TONE_id].u.n.default_val, int, 0444);
module_param_named(punct, vars[punct_id].u.n.default_val, int, 0444);
module_param_named(direct, vars[DIRECT_id].u.n.default_val, int, 0444);




MODULE_PARM_DESC(ser, "Set the serial port for the synthesizer (0-based).");
MODULE_PARM_DESC(dev, "Set the device e.g. ttyUSB0, for the synthesizer.");
MODULE_PARM_DESC(start, "Start the synthesizer once it is loaded.");
MODULE_PARM_DESC(caps_start, "Set the caps_start variable on load.");
MODULE_PARM_DESC(caps_stop, "Set the caps_stop variable on load.");
MODULE_PARM_DESC(rate, "Set the rate variable on load.");
MODULE_PARM_DESC(pitch, "Set the pitch variable on load.");
MODULE_PARM_DESC(vol, "Set the vol variable on load.");
MODULE_PARM_DESC(tone, "Set the tone variable on load.");
MODULE_PARM_DESC(punct, "Set the punct variable on load.");
MODULE_PARM_DESC(direct, "Set the direct variable on load.");

module_spk_synth(synth_audptr);

MODULE_AUTHOR("Kirk Reiser <kirk@braille.uwo.ca>");
MODULE_AUTHOR("David Borowski");
MODULE_DESCRIPTION("Speakup support for Audapter synthesizer");
MODULE_LICENSE("GPL");
MODULE_VERSION(DRV_VERSION);

