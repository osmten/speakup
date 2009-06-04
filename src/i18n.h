#ifndef I18N_H
#define I18N_H
/* Internationalization declarations */

enum msg_index_t {
	MSG_BLANK,
	MSG_IAM_ALIVE,
	MSG_YOU_KILLED_SPEAKUP,
	MSG_HEY_THATS_BETTER,
	MSG_YOU_TURNED_ME_OFF,
	MSG_PARKED,
	MSG_UNPARKED,
	MSG_MARK,
	MSG_CUT,
	MSG_MARK_CLEARED,
	MSG_PASTE,
	MSG_BRIGHT,
	MSG_ON_BLINKING,
	MSG_ON,
	MSG_NO_WINDOW,
	MSG_CURSOR_MSGS_START,
	MSG_CURSORING_OFF = MSG_CURSOR_MSGS_START,
	MSG_CURSORING_ON,
	MSG_HIGHLIGHT_TRACKING,
	MSG_READ_WINDOW,
	MSG_READ_ALL,
	MSG_EDIT_DONE,
	MSG_WINDOW_ALREADY_SET,
	MSG_END_BEFORE_START,
	MSG_WINDOW_CLEARED,
	MSG_WINDOW_SILENCED,
	MSG_WINDOW_SILENCE_DISABLED,
	MSG_ERROR,
	MSG_GOTO_CANCELED,
	MSG_GOTO,
	MSG_LEAVING_HELP,
	MSG_IS_UNASSIGNED,
	MSG_HELP_INFO,
/* For completeness.
	MSG_EDGE_MSGS_START,
	MSG_EDGE_TOP  = MSG_EDGE_MSGS_START,
	MSG_EDGE_BOTTOM,
	MSG_EDGE_LEFT,
	MSG_EDGE_RIGHT,
	MSG_EDGE_QUIET,
};

extern char *speakup_msgs[];

#endif
