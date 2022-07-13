#define PROMPT_LINE_LENGTH 256

struct prompt_buffer {
	char line[PROMPT_LINE_LENGTH + 1];
	int cursor;
	int gap;
	
	char kill[PROMPT_LINE_LENGTH + 1];
	int killcursor;
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