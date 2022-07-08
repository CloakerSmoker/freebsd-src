struct interact_buffer {
	char line[256];
	int cursor;
};

extern struct interact_buffer interact_prompt;

char interact_line_backspace(struct interact_keybind*);