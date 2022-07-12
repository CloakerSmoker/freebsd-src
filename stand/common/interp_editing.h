#define PROMPT_LINE_LENGTH 256

struct interact_buffer {
	char line[PROMPT_LINE_LENGTH + 1];
	int cursor;
	int gap;
};

extern struct interact_buffer interact_prompt;

void prompt_init();
void prompt_input(char);
char* prompt_getline();

char prompt_forward_char(struct interact_keybind*);
char prompt_backward_char(struct interact_keybind*);

char prompt_move_end_of_line(struct interact_keybind*);
char prompt_move_beginning_of_line(struct interact_keybind*);

char prompt_forward_word(struct interact_keybind*);
char prompt_backward_word(struct interact_keybind*);

char prompt_delete_backward_char(struct interact_keybind*);
char prompt_delete_forward_char(struct interact_keybind*);
char prompt_delele_char(struct interact_keybind*);
char prompt_kill_line(struct interact_keybind*);
char prompt_kill_word(struct interact_keybind*);
char prompt_backward_kill_word(struct interact_keybind*);