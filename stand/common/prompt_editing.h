#define PROMPT_LINE_LENGTH 256

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
	struct prompt_history_entry* history_cursor;
};

extern struct prompt_buffer prompt_prompt;

void prompt_init();
void prompt_reset();
void prompt_rawinput(char);
char* prompt_getline();

void prompt_forward_char(void*);
void prompt_backward_char(void*);

void prompt_move_end_of_line(void*);
void prompt_move_beginning_of_line(void*);

void prompt_forward_word(void*);
void prompt_backward_word(void*);

void prompt_delete_forward_char(void*);
void prompt_delete_backward_char(void*);

void prompt_yank(void*);

void prompt_forward_kill_word(void*);
void prompt_backward_kill_word(void*);
void prompt_kill_line(void*);

void prompt_next_history_element(void*);
void prompt_previous_history_element(void*);

struct prompt_history_entry* prompt_history_first();
struct prompt_history_entry* prompt_history_next(struct prompt_history_entry*);