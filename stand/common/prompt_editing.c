#include <sys/cdefs.h>
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

void prompt_show_aftergap() {
	/*
	 * Clear to end of line, reprint "LINE[GAP:END]", move cursor back so it is
	 * right before the contents of the gap.
	 */
	
	int gaplen = PROMPT_LINE_LENGTH - GAP;
	char* aftergap = &LINE[GAP];
	
	printf("\x1b[0K");
	
	if (gaplen) {
		printf("%s", aftergap);
		printf("\x1b[%dD", gaplen);
	}
}

void prompt_reprint() {
	interp_emit_prompt();
	
	for (int i = 0; i < CURSOR; i++) {
		printf("%c", LINE[i]);
	}
	
	prompt_show_aftergap();
}

void prompt_reset() {
	/*
	 * Called to simulate "end of line" in the gap buffer
	 */
	
	CURSOR = 0;
	GAP = PROMPT_LINE_LENGTH;
	LINE[GAP] = '\0';
}

void prompt_init() {
	/*
	 * Called once to get the buffer ready to go
	 */
	
	prompt_reset();
	KILLCURSOR = 0;
	KILL[KILLCURSOR] = '\0';
	
	TAILQ_INIT(HISTORY);
}

void prompt_rawinput(char in) {
	/*
	 * Add a character to the buffer without processing it as input
	 */
	
	LINE[CURSOR++] = in;
	
	printf("%c", in);
	
	prompt_show_aftergap();
}

void prompt_forward_char(void* data) {
	if (GAP != PROMPT_LINE_LENGTH) {
		LINE[CURSOR++] = LINE[GAP++];
		
		printf("\x1b[1C");
	}
}
void prompt_backward_char(void* data) {
	if (CURSOR != 0) {
		LINE[--GAP] = LINE[--CURSOR];
		
		printf("\x1b[1D");
	}
}

void prompt_delete_backward_char(void* data)
{
	if (CURSOR != 0) {
		LINE[--CURSOR] = '\0';
		
		putchar('\b');
		
		prompt_show_aftergap();
	}
};

void prompt_delete_forward_char(void* data)
{
	if (GAP != PROMPT_LINE_LENGTH) {
		LINE[GAP++] = '\0';
		
		prompt_show_aftergap();
	}
};

void prompt_move_end_of_line(void* data) {
	int gapsize = PROMPT_LINE_LENGTH - GAP;
	
	if (gapsize != 0) {
		printf("\x1b[%iC", gapsize);
		
		for (int i = 0; i < gapsize; i++) {
			LINE[CURSOR++] = LINE[GAP++];
		}
	}
}
void prompt_move_beginning_of_line(void* data) {
	int cursorsize = CURSOR;
	
	if (cursorsize != 0) {
		printf("\x1b[%iD", cursorsize);
		
		for (int i = 0; i < cursorsize; i++) {
			LINE[--GAP] = LINE[--CURSOR];
		}
	}
}

char* prompt_getline() {
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

static int count_forward_word() {
	/*
	 * Ignore whitespace, then consume a whole word forward
	 */
	
	int gapsize = PROMPT_LINE_LENGTH - GAP;
	int run = 0;
	
	for (; run < gapsize && isspace(LINE[GAP + run]); run++);
	for (; run < gapsize && isgraph(LINE[GAP + run]); run++);
	
	return run;
}
static int count_backward_word() {
	/*
	 * Ignore whitespace, then consume a whole word backward
	 */
	
	int cursorsize = CURSOR;
	int run = 0;
	
	for (; run < cursorsize && isspace(LINE[CURSOR - run - 1]); run++);
	for (; run < cursorsize && isgraph(LINE[CURSOR - run - 1]); run++);
	
	return run;
}

void prompt_forward_word(void* data) {
	int run = count_forward_word();
	
	if (run != 0) {
		for (int i = 0; i < run; i++) {
			LINE[CURSOR++] = LINE[GAP++];
		}
		
		printf("\x1b[%iC", run);
	}
}

void prompt_backward_word(void* data) {
	int run = count_backward_word();
	
	if (run != 0) {
		for (int i = 0; i < run; i++) {
			LINE[--GAP] = LINE[--CURSOR];
		}
		
		printf("\x1b[%iD", run);
	}
}

void prompt_yank(void* data) {
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

void prompt_forward_kill_word(void* data) {
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

void prompt_backward_kill_word(void* data) {
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

void prompt_kill_line(void* data) {
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

void prompt_recall_history(struct prompt_history_entry* entry) {
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

void prompt_next_history_element(void* data) {
	/*
	 * "next-history-element" functions as "delete-line" when at the start of
	 * history
	 */
	
	if (HISTORYCURSOR != NULL) {
		HISTORYCURSOR = TAILQ_NEXT(HISTORYCURSOR, entry);
	}
	
	prompt_recall_history(HISTORYCURSOR);
}
void prompt_previous_history_element(void* data) {
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

void prompt_history_add(const char* line, int len) {
	struct prompt_history_entry* entry = malloc(sizeof(struct prompt_history_entry));
	memcpy(entry->line, line, len);
	
	TAILQ_INSERT_TAIL(HISTORY, entry, entry);
}
void prompt_history_remove(struct prompt_history_entry* entry) {
	TAILQ_REMOVE(HISTORY, entry, entry);
	free(entry);
}

struct prompt_history_entry* prompt_history_first() {
	return TAILQ_FIRST(HISTORY);
}
struct prompt_history_entry* prompt_history_next(struct prompt_history_entry* entry) {
	return TAILQ_NEXT(entry, entry);
}

COMMAND_SET(history, "history", "display history entries", command_history);

static int
command_history(int argc, char *argv[])
{
	struct prompt_history_entry* e;
	
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

#define PROMPT_COLUMNS 80

typedef void*(completer_first)();
typedef void*(completer_next_item)(void*);
typedef void(completer_item_to_string)(void*, char*, int);

void prompt_generic_complete(char* argv, completer_first first, completer_next_item next, completer_item_to_string tostring) {
	int maxlen = 0;
	int arglen = strlen(argv);
	
	void* match = NULL;
	int matches = 0;
	
	void* p = first();
	
	while (p != NULL) {
		char buf[40] = { 0 };
	
		tostring(p, buf, sizeof(buf));
		
		int len = strlen(buf);
		
		if (len > maxlen) {
			maxlen = len;
		}
		
		if (len > arglen && strncmp(argv, buf, arglen) == 0) {
			match = p;
			matches++;
		}
		
		p = next(p);
	}
	
	if (matches == 1) {
		/*
		 * Single match, just complete it.
		 */
		 
		char buf[40] = { 0 };
	
		tostring(match, buf, sizeof(buf));
		
		char* remainder = buf + arglen;
		int rlen = strlen(remainder);
		
		printf("%s", remainder);
		memcpy(&LINE[CURSOR], remainder, rlen);
		CURSOR += rlen;
	}
	else {
		/*
		 * Many matches, print all aligned into columns, and then
		 * re-print the prompt and command line below all options.
		 */
		
		int column = 0;
		int maxcolums = PROMPT_COLUMNS / maxlen;
		
		printf("\n");
		pager_open();
		
		p = first();
		
		while (p != NULL) {
			char buf[40] = { 0 };
		
			tostring(p, buf, sizeof(buf));
			
			int len = strlen(buf);
			
			if (len > arglen && strncmp(argv, buf, arglen) == 0) {
				pager_output(buf);
					
				if (++column == maxcolums) {
					column = 0;
					pager_output("\n");
				}
				else {
					for (int i = len; i <= maxlen; i++) {
						pager_output(" ");
					}
				}
			}
			
			p = next(p);
		}
		
		pager_output("\n");
		pager_close();
		prompt_reprint();
	}
}

static void* command_first() {
	return (void*)1;
}
static void* command_next(void* rawlast) {
	long long int index = (long long int)rawlast;
	
	if (index++ > SET_COUNT(Xcommand_set)) {
		return NULL;
	}
	
	return (void*)(index);
}
static void command_tostring(void* rawindex, char* out, int len) {
	struct bootblk_command* cmd;
	
	cmd = SET_ITEM(Xcommand_set, ((long long int)rawindex) - 1);
	
	snprintf(out, len, "%s", cmd->c_name);
}

#define COMMAND_COMPLETERS command_first, command_next, command_tostring

void prompt_complete_command(void* data) {
	LINE[CURSOR] = 0;
	
	prompt_generic_complete(LINE, COMMAND_COMPLETERS);
}

struct prompt_completer_entry {
	char* command;
	prompt_completer completer;
	
	STAILQ_ENTRY(prompt_completer_entry) next;
};

STAILQ_HEAD(prompt_completers, prompt_completer_entry) prompt_completers_head =
	 STAILQ_HEAD_INITIALIZER(prompt_completers_head);

void prompt_register_completer(char* command, prompt_completer completer) {
	struct prompt_completer_entry* entry = malloc(sizeof(struct prompt_completer_entry));
	
	entry->command = command;
	entry->completer = completer;
	
	STAILQ_INSERT_TAIL(&prompt_completers_head, entry, next);
}

void prompt_complete_smart(void* data) {
	/*
	 * "smart" completion, completes a command if there isn't one typed out
	 * already, or tries to complete command arguments.
	 */
	
	if (GAP != PROMPT_LINE_LENGTH) {
		return;
	}
	
	int cmdlen = 0;
	
	for (; cmdlen < CURSOR && isgraph(LINE[cmdlen]); cmdlen++);
	
	if (!isspace(LINE[cmdlen])) {
		prompt_complete_command(NULL);
		return;
	}
	
	char old = LINE[cmdlen];
	LINE[cmdlen] = '\0';
	
	char* command = LINE;
	
	struct prompt_completer_entry* entry = NULL;
	struct prompt_completer_entry* e;
	
	STAILQ_FOREACH(e, &prompt_completers_head, next) {
		if (strcmp(e->command, command) == 0) {
			entry = e;
			break;
		}
	}
	
	LINE[cmdlen] = old;
	
	if (entry == NULL) {
		return;
	}
	
	char args[PROMPT_LINE_LENGTH] = { 0 };
	memcpy(args, &LINE[cmdlen + 1], CURSOR - cmdlen - 1);
	
	/*
	 * Find the number and text of the last/most recent argument so the completer
	 * has enough context to actually complete the argument.
	 */
	
	int argc = 1;
	char* argv = args;
	char* next = strtok(args, "\t\f\v ");
	
	if (next != NULL) {
		argc = 0;
		
		while (next != NULL) {
			argc++;
			argv = next;
			next = strtok(NULL, "\t\f\v ");
		}
	}
	
	entry->completer(command, argc, argv);
}

static void* keybind_first() {
	return (void*)prompt_first_binding();
}
static void* keybind_next(void* rawlast) {
	return (void*)prompt_next_binding((struct prompt_keybind*)rawlast);
}
static void keybind_tostring(void* raw, char* out, int len) {
	struct prompt_keybind* p = raw;
	
	prompt_stroke_to_string(out, len, p->target);
}

void keyunbind_completer(char* command, int argc, char* argv) {
	prompt_generic_complete(argv, keybind_first, keybind_next, keybind_tostring);
}

static void* environ_first() {
	return environ;
}
static void* environ_next(void* rawlast) {
	struct env_var* ev = rawlast;
	
	return ev->ev_next;
}
static void environ_tostring(void* raw, char* out, int len) {
	struct env_var* ev = raw;
	
	snprintf(out, len, "%s", ev->ev_name);
}

void environ_completer(char* command, int argc, char* argv) {
	prompt_generic_complete(argv, environ_first, environ_next, environ_tostring);
}

static void* predef_first() {
	return prompt_first_action();
}
static void* predef_next(void* rawlast) {
	struct prompt_predefined_action* p = rawlast;
	
	return prompt_next_action(p);
}
static void predef_tostring(void* raw, char* out, int len) {
	struct prompt_predefined_action* p = raw;
	
	snprintf(out, len, "%s", p->name);
}

void predefined_action_completer(char* command, int argc, char* argv) {
	prompt_generic_complete(argv, predef_first, predef_next, predef_tostring);
}