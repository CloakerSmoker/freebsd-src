/*-
 * Copyright (c) 2022 Connor Bailey
 *
 * SPDX-License-Identifier: BSD-2-clause
 */

#include <stand.h>
#include <string.h>
#include "bootstrap.h"

#include "prompt_bindings.h"
#include "prompt_editing.h"

/*
 * Helper macros, just to make each action a bit more readable
 */

#define LINE (prompt_prompt.line)
#define CURSOR (prompt_prompt.cursor)
#define GAP (prompt_prompt.gap)
#define KILL (prompt_prompt.kill)
#define KILLCURSOR (prompt_prompt.killcursor)
#define HISTORY (&prompt_prompt.history_head)
#define HISTORYCURSOR (prompt_prompt.history_cursor)

static void
prompt_show_aftergap()
{
	/*
	 * Clear to end of line, reprint "LINE[GAP:END]", move cursor back so it is
	 * right before the contents of the gap.
	 */

	int gaplen = PROMPT_LINE_LENGTH - GAP;
	char *aftergap = &LINE[GAP];

	printf("\x1b[0K");

	if (gaplen) {
		printf("%s", aftergap);
		printf("\x1b[%dD", gaplen);
	}
}

static void
prompt_reprint()
{
	interp_emit_prompt();

	LINE[CURSOR] = 0;
	printf("%s", LINE);

	prompt_show_aftergap();
}

void
prompt_reset()
{
	/*
	 * Called to simulate "end of line" in the gap buffer
	 */

	CURSOR = 0;
	GAP = PROMPT_LINE_LENGTH;
	LINE[GAP] = '\0';
}

void
prompt_init()
{
	/*
	 * Called once to get the buffer ready to go
	 */

	prompt_reset();
	KILLCURSOR = 0;
	KILL[KILLCURSOR] = '\0';

	TAILQ_INIT(HISTORY);
}

void
prompt_rawinput(char in)
{
	/*
	 * Add a character to the buffer without processing it as input
	 */

	LINE[CURSOR++] = in;

	printf("%c", in);

	prompt_show_aftergap();
}

char *
prompt_getline()
{
	/*
	 * To get a whole line, we just need to move GAP to the end of the line
	 * which will put the entire line left of the gap, with CURSOR being the
	 * length of the line
	 *
	 * We also use this as a chance to add the line to the history, and reset
	 * the history pointer. This makes the next "history-previous-element" get
	 * the item which we just added to the history.
	 */

	prompt_move_end_of_line(NULL);
	LINE[CURSOR] = '\0';
	HISTORYCURSOR = NULL;

	if (CURSOR != 0) {
		prompt_history_add(LINE, CURSOR);
	}

	return LINE;
}

void
prompt_forward_char(void *data)
{
	if (GAP != PROMPT_LINE_LENGTH) {
		LINE[CURSOR++] = LINE[GAP++];

		printf("\x1b[1C");
	}
}

PREDEF_ACTION_SET("forward-char", prompt_forward_char);

void
prompt_backward_char(void *data)
{
	if (CURSOR != 0) {
		LINE[--GAP] = LINE[--CURSOR];

		printf("\x1b[1D");
	}
}

PREDEF_ACTION_SET("backward-char", prompt_backward_char);

void
prompt_delete_backward_char(void *data)
{
	if (CURSOR != 0) {
		LINE[--CURSOR] = '\0';

		putchar('\b');

		prompt_show_aftergap();
	}
}

PREDEF_ACTION_SET("delete-backward-char", prompt_delete_backward_char);

void
prompt_delete_forward_char(void *data)
{
	if (GAP != PROMPT_LINE_LENGTH) {
		LINE[GAP++] = '\0';

		prompt_show_aftergap();
	}
}

PREDEF_ACTION_SET("delete-forward-char", prompt_delete_forward_char);

void
prompt_move_end_of_line(void *data)
{
	int gapsize = PROMPT_LINE_LENGTH - GAP;

	if (gapsize != 0) {
		printf("\x1b[%iC", gapsize);

		for (int i = 0; i < gapsize; i++) {
			LINE[CURSOR++] = LINE[GAP++];
		}
	}
}

PREDEF_ACTION_SET("move-end-of-line", prompt_move_end_of_line);

void
prompt_move_beginning_of_line(void *data)
{
	int cursorsize = CURSOR;

	if (cursorsize != 0) {
		printf("\x1b[%iD", cursorsize);

		for (int i = 0; i < cursorsize; i++) {
			LINE[--GAP] = LINE[--CURSOR];
		}
	}
}

PREDEF_ACTION_SET("move-beginning-of-line", prompt_move_beginning_of_line);

static int
count_forward_word()
{
	/*
	 * Ignore whitespace, then consume a whole word forward
	 */

	int gapsize = PROMPT_LINE_LENGTH - GAP;
	int run = 0;

	for (; run < gapsize && isspace(LINE[GAP + run]); run++);
	for (; run < gapsize && isgraph(LINE[GAP + run]); run++);

	return run;
}
static int
count_backward_word()
{
	/*
	 * Ignore whitespace, then consume a whole word backward
	 */

	int cursorsize = CURSOR;
	int run = 0;

	for (; run < cursorsize && isspace(LINE[CURSOR - run - 1]); run++);
	for (; run < cursorsize && isgraph(LINE[CURSOR - run - 1]); run++);

	return run;
}

void
prompt_forward_word(void *data)
{
	int run = count_forward_word();

	if (run != 0) {
		for (int i = 0; i < run; i++) {
			LINE[CURSOR++] = LINE[GAP++];
		}

		printf("\x1b[%iC", run);
	}
}

PREDEF_ACTION_SET("forward-word", prompt_forward_word);

void
prompt_backward_word(void *data)
{
	int run = count_backward_word();

	if (run != 0) {
		for (int i = 0; i < run; i++) {
			LINE[--GAP] = LINE[--CURSOR];
		}

		printf("\x1b[%iD", run);
	}
}

PREDEF_ACTION_SET("backward-word", prompt_backward_word);

void
prompt_yank(void *data)
{
	/*
	 * Copy KILL[0:KILLCURSOR] back into LINE, starting at CURSOR
	 * (pushing the gap back)
	 */

	if (KILLCURSOR) {
		for (int i = 0; i < KILLCURSOR; i++) {
			LINE[CURSOR++] = KILL[i];
			printf("%c", KILL[i]);
		}

		prompt_show_aftergap();
	}
}

PREDEF_ACTION_SET("yank", prompt_yank);

void
prompt_forward_kill_word(void *data)
{
	int run = count_forward_word();

	if (run != 0) {
		/*
		 * Find a word, copy it into KILL, then remove it from the gap
		 */

		memcpy(KILL, &LINE[GAP], run);
		KILLCURSOR = run;

		GAP += run;
		prompt_show_aftergap();
	}
}

PREDEF_ACTION_SET("kill-word", prompt_forward_kill_word);

void
prompt_backward_kill_word(void *data)
{
	int run = count_backward_word();

	if (run != 0) {
		/*
		 * Find a word, copy it into KILL, then remove it from the end of CURSOR
		 */

		memcpy(KILL, &LINE[CURSOR - run], run);
		KILLCURSOR = run;

		printf("\x1b[%iD", run);

		CURSOR -= run;
		prompt_show_aftergap();
	}
}

PREDEF_ACTION_SET("backward-kill-word", prompt_backward_kill_word);

void
prompt_kill_line(void *data)
{
	prompt_move_beginning_of_line(NULL);

	int gapsize = PROMPT_LINE_LENGTH - GAP;

	if (gapsize) {
		/*
		 * Kill an entire line, CURSOR is already 0'd by moving to the start of the
		 * line, so we just need to reset GAP to reset the buffer.
		 */

		memcpy(KILL, &LINE[GAP], gapsize);
		KILLCURSOR = gapsize;

		GAP = PROMPT_LINE_LENGTH;

		prompt_show_aftergap();
	}
}

PREDEF_ACTION_SET("kill-line", prompt_kill_line);

void
prompt_recall_history(struct prompt_history_entry *entry)
{
	/*
	 * Clear the command line, then recall a whole line from history (if there is one)
	 */

	prompt_move_beginning_of_line(NULL);
	prompt_reset();
	prompt_show_aftergap();

	if (entry != NULL) {
		CURSOR = strlen(entry->line);
		memcpy(LINE, entry->line, CURSOR);
		printf("%s", entry->line);
	}
}

void
prompt_next_history_element(void *data)
{
	/*
	 * "next-history-element" functions as "delete-line" when at the start of
	 * history
	 */

	if (HISTORYCURSOR != NULL) {
		HISTORYCURSOR = TAILQ_NEXT(HISTORYCURSOR, entry);
	}

	prompt_recall_history(HISTORYCURSOR);
}

PREDEF_ACTION_SET("next-history-element", prompt_next_history_element);

void
prompt_previous_history_element(void *data)
{
	/*
	 * "previous-history-element" at the start of history starts at the most
	 * recently added entry
	 */

	if (HISTORYCURSOR == NULL) {
		HISTORYCURSOR = TAILQ_LAST(HISTORY, prompt_history_head);
	}
	else {
		HISTORYCURSOR = TAILQ_PREV(HISTORYCURSOR, prompt_history_head, entry);
	}

	prompt_recall_history(HISTORYCURSOR);
}

PREDEF_ACTION_SET("previous-history-element", prompt_previous_history_element);

/*
 * History command/Lua interface
 */

void
prompt_history_add(const char *line, int len)
{
	struct prompt_history_entry *entry = malloc(sizeof(struct prompt_history_entry));
	memcpy(entry->line, line, len);

	TAILQ_INSERT_TAIL(HISTORY, entry, entry);
}
void
prompt_history_remove(struct prompt_history_entry *entry)
{
	TAILQ_REMOVE(HISTORY, entry, entry);
	free(entry);
}

/*
 * Used by Lua to populate the "history" global
 */

struct prompt_history_entry *
prompt_history_first()
{
	return TAILQ_FIRST(HISTORY);
}
struct prompt_history_entry *
prompt_history_next(struct prompt_history_entry *entry)
{
	return TAILQ_NEXT(entry, entry);
}

COMMAND_SET(history, "history", "display history entries", command_history);

static int
command_history(int argc, char *argv[])
{
	struct prompt_history_entry *e;

	/*
	 * Pop the "history" entry from the history
	 */
	prompt_history_remove(TAILQ_LAST(HISTORY, prompt_history_head));

	pager_open();

	TAILQ_FOREACH(e, HISTORY, entry) {
		pager_output(e->line);
		pager_output("\n");
	}

	pager_close();

	return 0;
}

/*
 * Completion logic
 */

#define PROMPT_COLUMNS 80

void
prompt_generic_complete(char *argv, generic_completer_first first, generic_completer_next_item next,
	void *last, generic_completer_item_to_string tostring)
{
	/*
	 * Technically, we should be able to call tostring on the same
	 * "handle" at any time and get the same result, but unfortunately
	 * that doesn't work for dirents, so instead of remembering the exact
	 * item that we've matched, in all cases we just remember what the
	 * item stringified to (and recall that instead).
	 */

	char buf[40] = { 0 };

	char maxbuf[40] = { 0 };
	int maxlen = 0;

	char matchbuf[40] = { 0 };
	int matches = 0;

	int arglen = strlen(argv);

	void* p = first();

	while (p != last) {
		tostring(p, buf, sizeof(buf));
		int len = strlen(buf);

		if (len >= arglen && strncmp(argv, buf, arglen) == 0) {
			matches++;

			if (matches == 1) {
				memcpy(matchbuf, buf, sizeof(buf));
			}

			if (len > maxlen) {
				memcpy(maxbuf, buf, sizeof(buf));
				maxlen = len;
			}
		}

		p = next(p);
	}

	if (matches == 0) {
		return;
	}
	else if (matches == 1) {
		/*
		 * Single match, just complete it.
		 */

		char *remainder = matchbuf + arglen;
		int rlen = strlen(remainder);

		printf("%s", remainder);
		memcpy(&LINE[CURSOR], remainder, rlen);
		CURSOR += rlen;
	} else {
		/*
		 * Many matches, print all aligned into columns, and then re-print the
		 * prompt and command line below all options.
		 * If all matches share a common prefix though, complete through to the
		 * end of the prefix so you can "jump" through options by just typing
		 * the few characters of difference.
		 */

		char *prefixbuf = maxbuf;
		int prefixlen = strlen(prefixbuf);

		int column = 0;
		int maxcolums = PROMPT_COLUMNS / maxlen;

		printf("\n");
		pager_open();

		p = first();

		while (p != last) {
			tostring(p, buf, sizeof(buf));
			int len = strlen(buf);

			if (len >= arglen && strncmp(argv, buf, arglen) == 0) {
				pager_output(buf);

				if (++column == maxcolums) {
					column = 0;

					if (pager_output("\n")) {
						break;
					}
				} else {
					for (int i = len; i <= maxlen; i++) {
						pager_output(" ");
					}
				}

				for (int i = 0; i < prefixlen; i++) {
					if (buf[i] != prefixbuf[i]) {
						prefixbuf[i] = '\0';
						prefixlen = i;
						break;
					}
				}
			}

			p = next(p);
		}

		pager_close();
		printf("\n");
		prompt_reprint();

		if (prefixlen != 0 && prefixlen > arglen) {
			char *remainder = prefixbuf + arglen;
			int rlen = prefixlen - arglen;

			printf("%s", remainder);
			memcpy(&LINE[CURSOR], remainder, rlen);
			CURSOR += rlen;
		}
	}
}

/*
 * Command completer
 */

static void *
command_first()
{
	return SET_BEGIN(Xcommand_set);
}
static void *
command_next(void *rawlast)
{
	struct bootblk_command **pcmd = rawlast;

	return (void*)++pcmd;
}
static void
command_tostring(void *rawlast, char *out, int len)
{
	struct bootblk_command **pcmd = rawlast;

	snprintf(out, len, "%s", (*pcmd)->c_name);
}

void prompt_complete_command(void *data) {
	LINE[CURSOR] = 0;

	prompt_generic_complete(LINE, command_first, command_next, SET_LIMIT(Xcommand_set), command_tostring);
}

PREDEF_ACTION_SET("command-complete", prompt_complete_command);

/*
 * "smart"/context sensitive completer
 */

void
prompt_complete_smart(void *data)
{
	/*
	 * "smart" completion, completes a command if there isn't one typed out
	 * already, or tries to complete command arguments.
	 */

	if (GAP != PROMPT_LINE_LENGTH) {
		return;
	}

	int cmdlen = 0;

	for (; cmdlen < CURSOR && isalnum(LINE[cmdlen]); cmdlen++);

	if (!(isspace(LINE[cmdlen]) || ispunct(LINE[cmdlen])) || cmdlen == CURSOR) {
		prompt_complete_command(NULL);
		return;
	}

	char old = LINE[cmdlen];
	LINE[cmdlen] = '\0';

	char *command = LINE;

	char args[PROMPT_LINE_LENGTH] = { 0 };
	memcpy(args, &LINE[cmdlen + 1], CURSOR - cmdlen - 1);

	/*
	 * Find the number and text of the last/most recent argument so the completer
	 * has enough context to actually complete the argument.
	 * Doesn't need to be bulletproof since we only need the last arg, and the count.
	 */

	int argc = 1;
	char *last = args;
	char *next = strpbrk(args, "\t\f\v ");

	while (next != NULL) {
		*next = '\0';
		last = next + 1;

		next = strpbrk(last, "\t\f\v ");
		argc++;
	}

	/*
	 * There's two special cases for completion, either a "_" command which
	 * matches any undefined command, or a "0" arg index, which matches any
	 * argument number.
	 * Matching an undefined command is an escape hatch for completing languages
	 * which are too complicated to properly parse here, and the "0" arg index
	 * is for commands involving flags and (again) more complicated parsing.
	 */

	int defined = false;

	struct bootblk_command **pcmd;
	SET_FOREACH(pcmd, Xcommand_set) {
		if (strcmp((*pcmd)->c_name, command) == 0) {
			defined = true;
			break;
		}
	}

	prompt_completion_entry *entry = NULL;
	prompt_completion_entry *fallthrough = NULL;

	prompt_completion_entry **pce;
	SET_FOREACH(pce, Xcompleter_set) {
		prompt_completion_entry *e = *pce;

		if (strcmp(e->command, command) == 0 && (e->argn == 0 || e->argn == argc)) {
			entry = e;
		}
		else if (strcmp(e->command, "_") == 0 && !defined) {
			fallthrough = e;
		}
	}

	LINE[cmdlen] = old;

	if (entry == NULL) {
		if (fallthrough != NULL) {
			fallthrough->completer(command, last);
		}
	} else {
		entry->completer(command, last);
	}
}

PREDEF_ACTION_SET("smart-complete", prompt_complete_smart);

/*
 * Misc completers, some command-specific, some generic
 */

static void *
keybind_first()
{
	return (void*)prompt_first_binding();
}
static void *
keybind_next(void *rawlast)
{
	return (void*)prompt_next_binding((struct prompt_keybind *)rawlast);
}
static void
keybind_tostring(void *raw, char *out, int len)
{
	struct prompt_keybind *p = raw;

	prompt_stroke_to_string(out, len, p->target);
}

void
keyunbind_completer(char *command, char *argv)
{
	prompt_generic_complete(argv, keybind_first, keybind_next, NULL, keybind_tostring);
}

COMPLETION_SET(keybind, 2, predefined_action_completer);

static void *
environ_first()
{
	return environ;
}
static void *
environ_next(void* rawlast)
{
	struct env_var *ev = rawlast;

	return ev->ev_next;
}
static void
environ_tostring(void *raw, char *out, int len)
{
	struct env_var *ev = raw;

	snprintf(out, len, "%s", ev->ev_name);
}

void
environ_completer(char *command, char *argv)
{
	prompt_generic_complete(argv, environ_first, environ_next, NULL, environ_tostring);
}

COMPLETION_SET(show, 1, environ_completer);
COMPLETION_SET(set, 1, environ_completer);
COMPLETION_SET(unset, 1, environ_completer);

static void *
predef_first()
{
	return SET_BEGIN(Xpredef_action_set);
}
static void *
predef_next(void* rawlast)
{
	struct prompt_predefined_action **ppa = rawlast;

	return (void*)++ppa;
}
static void
predef_tostring(void *rawlast, char *out, int len)
{
	struct prompt_predefined_action **ppa = rawlast;

	snprintf(out, len, "%s", (*ppa)->name);
}

void
predefined_action_completer(char *command, char *argv)
{
	prompt_generic_complete(argv, predef_first, predef_next, SET_LIMIT(Xpredef_action_set), predef_tostring);
}

/*
 * Path completion doesn't fit into the first/next/tostring model super well,
 * since we're working with an fd which contains its own state. Plus, since
 * first() gets called twice, we need to have it reset the internal state of that
 * fd so we can read all dirents again.
 * In practice, this just means we need to remember the fd, and the name of the
 * fd so it can be reopened.
 */

static const char *path_dirname;
static int path_fd;

static void *
path_next(void *raw)
{
	return readdirfd(path_fd);
}

static void *
path_first()
{
	if (path_fd > 0) {
		close(path_fd);
	}

	path_fd = open(path_dirname, O_RDONLY);

	return path_next(NULL);
}

static void
path_tostring(void *rawlast, char *out, int len)
{
	struct dirent *entry = rawlast;

	/*
	 * We need to ensure that path_dirname contains a path with a trailing "/"
	 * otherwise we'll spit out a bunch of garbage that can't be completed.
	 * In practice, the only path *with* a trailing "/" is going to be "/"
	 * itself since our dirname/basename split will always destroy the last "/".
	 */

	int dirnamelen = snprintf(out, len, "%s", path_dirname);
	out += dirnamelen;
	len -= dirnamelen;

	if (*(out - 1) != '/') {
		*out++ = '/';
		len--;
	}

	/*
	 * We also want to show directories (not "." and ".." though) with a trailing
	 * "/" so they can be completed as a whole, and then any of their entries
	 * can also be completed with minimal typing.
	 */

	char *fmt = "%s";

	if ((entry->d_type & DT_DIR) && strcmp(entry->d_name, ".") && strcmp(entry->d_name, "..")) {
		fmt = "%s/";
	}

	snprintf(out, len, fmt, entry->d_name);
}

void
path_completer(char *command, char *argv)
{
	/*
	 * We want to split argv into a dirname/basename pair, so we can check each
	 * entry inside of dirname if it matches the prefix basename.
	 * So, we just need to find the last "/" in argv, and replace it with a null
	 * terminator. Then ("/" + 1) is our basename, and argv is our dirname.
	 * However, since path_tostring gives us an absolute path, we actually need
	 * to throw away the basename and match against the (already absolute)
	 * path in argv, which is why we operate on the copy "path" instead.
	 */

	char path[128] = { 0 };
	snprintf(path, 128, "%s", argv);

	if (strlen(path) == 0) {
		path[0] = '/';
	}

	char *dirname = path;
	char *basename = path + strlen(path);

	while (basename > dirname && *(basename - 1) != '/') {
		basename--;
	}

	*(basename - 1) = '\0';

	if (strlen(dirname) == 0) {
		dirname = "/";
	}

	path_dirname = dirname;
	path_fd = 0;

	prompt_generic_complete(argv, path_first, path_next, NULL, path_tostring);
}

COMPLETION_SET(ls, 0, path_completer);