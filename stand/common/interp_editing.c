#include <sys/cdefs.h>
#include <stand.h>
#include <string.h>
#include "bootstrap.h"

#include "interp_bindings.h"
#include "interp_editing.h"

#define LINE (interact_prompt.line)
#define CURSOR (interact_prompt.cursor)
#define GAP (interact_prompt.gap)

void prompt_show_aftergap() {
	char* aftergap = &LINE[GAP];
	
	printf("\x1b[0K");
	printf("%s", aftergap);
	printf("\x1b[%zuD", strlen(aftergap));
}

char prompt_forward_char(struct interact_keybind* bind) {
	if (GAP != PROMPT_LINE_LENGTH) {
		LINE[CURSOR++] = LINE[GAP++];
		
		printf("\x1b[1C");
	}
	
	return 0;
}
char prompt_backward_char(struct interact_keybind* bind) {
	if (CURSOR != 0) {
		LINE[--GAP] = LINE[--CURSOR];
		
		printf("\x1b[1D");
	}
	
	return 0;
}

char prompt_delete_backward_char(struct interact_keybind* bind)
{
	if (CURSOR != 0) {
		LINE[--CURSOR] = '\0';
		
		putchar('\b');
		
		prompt_show_aftergap();
	}
	
	return 0;
};

char prompt_delete_forward_char(struct interact_keybind* bind)
{
	if (GAP != PROMPT_LINE_LENGTH) {
		LINE[GAP++] = '\0';
		
		prompt_show_aftergap();
	}
	
	return 0;
};

char prompt_move_end_of_line(struct interact_keybind* bind) {
	int gapsize = PROMPT_LINE_LENGTH - GAP;
	
	printf("\x1b[%iC", gapsize);
	
	for (int i = 0; i < gapsize; i++) {
		LINE[CURSOR++] = LINE[GAP++];
	}
	
	return 0;
}
char prompt_move_beginning_of_line(struct interact_keybind* bind) {
	int cursorsize = CURSOR;
	
	printf("\x1b[%iD", cursorsize);
	
	for (int i = 0; i < cursorsize; i++) {
		LINE[--GAP] = LINE[--CURSOR];
	}
	
	return 0;
}