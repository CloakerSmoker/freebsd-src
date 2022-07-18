#include <sys/cdefs.h>
#include <stand.h>
#include <string.h>
#include "bootstrap.h"

#include "prompt_bindings.h"
#include "prompt_editing.h"

#define LINE (prompt_prompt.line)
#define CURSOR (prompt_prompt.cursor)
#define GAP (prompt_prompt.gap)
#define KILL (prompt_prompt.kill)
#define KILLCURSOR (prompt_prompt.killcursor)
#define HISTORY (&prompt_prompt.history_head)
#define HISTORYCURSOR (prompt_prompt.history_cursor)

void prompt_show_aftergap() {
	int gaplen = PROMPT_LINE_LENGTH - GAP;
	char* aftergap = &LINE[GAP];
	
	printf("\x1b[0K");
	
	if (gaplen) {
		printf("%s", aftergap);
		printf("\x1b[%dD", gaplen);
	}
}

void prompt_reset() {
	CURSOR = 0;
	GAP = PROMPT_LINE_LENGTH;
	LINE[GAP] = '\0';
}

void prompt_init() {
	prompt_reset();
	KILLCURSOR = 0;
	KILL[KILLCURSOR] = '\0';
}

void prompt_rawinput(char in) {
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
	prompt_move_end_of_line(NULL);
	LINE[CURSOR] = '\0';
	HISTORYCURSOR = NULL;
	
	if (CURSOR != 0) {
		struct prompt_history_entry* entry = malloc(sizeof(struct prompt_history_entry));
		memcpy(entry->line, LINE, CURSOR);
		
		TAILQ_INSERT_TAIL(HISTORY, entry, entry);
	}
	
	return LINE;
}

static int count_forward_word() {
	int gapsize = PROMPT_LINE_LENGTH - GAP;
	int run = 0;
	
	for (; run < gapsize && isspace(LINE[GAP + run]); run++);
	for (; run < gapsize && isgraph(LINE[GAP + run]); run++);
	
	return run;
}
static int count_backward_word() {
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
		memcpy(KILL, &LINE[GAP], run);
		KILLCURSOR = run;
		
		GAP += run;
		prompt_show_aftergap();
	}
}

void prompt_backward_kill_word(void* data) {
	int run = count_backward_word();
	
	if (run != 0) {
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
		memcpy(KILL, &LINE[GAP], gapsize);
		KILLCURSOR = gapsize;
		
		GAP = PROMPT_LINE_LENGTH;
		
		prompt_show_aftergap();
	}
}

void prompt_recall_history(struct prompt_history_entry* entry) {
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
	if (HISTORYCURSOR != NULL) {
		HISTORYCURSOR = TAILQ_NEXT(HISTORYCURSOR, entry);
	}
	
	prompt_recall_history(HISTORYCURSOR);
}
void prompt_previous_history_element(void* data) {
	if (HISTORYCURSOR == NULL) {
		HISTORYCURSOR = TAILQ_LAST(HISTORY, prompt_history_head);
	}
	else {
		HISTORYCURSOR = TAILQ_PREV(HISTORYCURSOR, prompt_history_head, entry);
	}
	
	prompt_recall_history(HISTORYCURSOR);
}