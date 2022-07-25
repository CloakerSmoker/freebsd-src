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

void prompt_complete_command(void* data) {
	LINE[CURSOR] = '\0';

	const char* match = NULL;
	int matches = 0;
	int maxlen = 0;
	
	struct bootblk_command	**cmdp;
	
	/*
	 * First pass only determines if there's a single match, second pass will
	 * actually list all possible matches.
	 * If there is a single match, we just complete it and bail out early.
	 * First pass also finds the longest command name, so we can align them
	 * into columns while printing.
	 */
	
	SET_FOREACH(cmdp, Xcommand_set) {
		const char* name = (*cmdp)->c_name;
		
		if (name != NULL) {
			int len = strlen(name);
			
			if (len > maxlen) {
				maxlen = len;
			}
			
			if (len > CURSOR && strncmp(LINE, name, CURSOR) == 0) {
				match = name;
				matches++;
			}
		}
	}
	
	if (matches == 1) {
		/*
		 * Single match, just complete it.
		 */
		
		const char* remainder = &match[CURSOR];
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
		
		SET_FOREACH(cmdp, Xcommand_set) {
			const char* name = (*cmdp)->c_name;
			
			if (name != NULL) {
				int len = strlen(name);
				
				if (len > CURSOR && strncmp(LINE, name, CURSOR) == 0) {
					printf("%s", name);
					
					if (++column == maxcolums) {
						column = 0;
						printf("\n");
					}
					else {
						for (int i = len; i <= maxlen; i++) {
							printf(" ");
						}
					}
				}
			}
		}
		
		printf("\n");
		interp_emit_prompt();
		printf("%s", LINE);
		prompt_show_aftergap();
	}
}