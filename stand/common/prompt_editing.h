/*-
 * Copyright (c) 2022 Connor Bailey
 *
 * SPDX-License-Identifier: BSD-2-clause
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#define PROMPT_LINE_LENGTH 256

/*
 * The prompt is just a gap buffer, with a single kill buffer, and a linked list
 * of history entries.
 */

struct prompt_history_entry {
	char line[PROMPT_LINE_LENGTH];
	TAILQ_ENTRY(prompt_history_entry) entry;
};

struct prompt_buffer {
	char line[PROMPT_LINE_LENGTH + 1];
	int cursor;
	int gap;
	
	char kill[PROMPT_LINE_LENGTH + 1];
	int killcursor;
	
	TAILQ_HEAD(prompt_history_head, prompt_history_entry) history_head;
	struct prompt_history_entry *history_cursor;
};

extern struct prompt_buffer prompt_prompt;

void prompt_init();
void prompt_reset();
void prompt_rawinput(char);
char *prompt_getline();

/*
 * Editing actions
 */

void prompt_forward_char(void *);
void prompt_backward_char(void *);

void prompt_move_end_of_line(void *);
void prompt_move_beginning_of_line(void *);

void prompt_forward_word(void *);
void prompt_backward_word(void *);

void prompt_delete_forward_char(void *);
void prompt_delete_backward_char(void *);

void prompt_yank(void *);

void prompt_forward_kill_word(void *);
void prompt_backward_kill_word(void *);
void prompt_kill_line(void *);

void prompt_next_history_element(void *);
void prompt_previous_history_element(void *);

/*
 * History manipulation
 */

void prompt_history_add(const char *, int);
void prompt_history_remove(struct prompt_history_entry *);
struct prompt_history_entry *prompt_history_first();
struct prompt_history_entry *prompt_history_next(struct prompt_history_entry *);

/*
 * Completion
 */

void prompt_complete_command(void*);
void prompt_complete_smart(void*);

typedef void*(generic_completer_first)();
typedef void*(generic_completer_next_item)(void *);
typedef void(generic_completer_item_to_string)(void *, char *, int);

void prompt_generic_complete(char *, generic_completer_first, generic_completer_next_item, void *, generic_completer_item_to_string);

typedef void(*prompt_completer)(char *, char *);

void keyunbind_completer(char *, char *);
void environ_completer(char *, char *);
void predefined_action_completer(char *, char *);
void path_completer(char *, char *);

typedef struct {
	const char *command;
	int argn;
	prompt_completer completer;
} prompt_completion_entry;

#define COMPLETION_SET(command, argn, func) \
	static prompt_completion_entry _completer_ ## command ## argn = { #command, argn, func }; \
	DATA_SET(Xcompleter_set, _completer_ ## command ## argn)

SET_DECLARE(Xcompleter_set, prompt_completion_entry);