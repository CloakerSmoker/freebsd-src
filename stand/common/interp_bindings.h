

#include <sys/queue.h>

struct interact_input {
	char mods;
	char key;
};

struct interact_keybind;

typedef char(*interact_action)(struct interact_keybind*);

struct interact_keybind {
	struct interact_input target;
	
	interact_action action;
	void* parameter;
	
	STAILQ_ENTRY(interact_keybind) next;
};

struct interact_predefined_action {
	char* name;
	interact_action action;
	void* parameter;
	
	STAILQ_ENTRY(interact_predefined_action) next;
};

#define INTERP_MOD_SHIFT 1
#define INTERP_MOD_ALT 2
#define INTERP_MOD_CTRL 4

#define INTERP_ANSI_TO_KEY 0x80
#define INTERP_KEY_UP (INTERP_ANSI_TO_KEY + 'A')
#define INTERP_KEY_DOWN (INTERP_ANSI_TO_KEY + 'B')
#define INTERP_KEY_RIGHT (INTERP_ANSI_TO_KEY + 'C')
#define INTERP_KEY_LEFT (INTERP_ANSI_TO_KEY + 'D')
#define INTERP_KEY_END (INTERP_ANSI_TO_KEY + 'F')
#define INTERP_KEY_HOME (INTERP_ANSI_TO_KEY + 'H')

#define INTERP_KEY_INSERT (INTERP_ANSI_TO_KEY + 'E')
#define INTERP_KEY_DELETE (INTERP_ANSI_TO_KEY + 'I')

#define INTERP_KEY_PGUP (INTERP_ANSI_TO_KEY + 'J')
#define INTERP_KEY_PGDN (INTERP_ANSI_TO_KEY + 'K')

struct interact_input interact_parse_input(char);
char interact_input_to_char(struct interact_input input);

char interact_on_input(char);

struct interact_keybind* interact_add_binding(char, char, interact_action, void*);
void interact_print_stroke(struct interact_input);
struct interact_input interact_parse_stroke(char*);
struct interact_predefined_action* interact_register_action(char*, interact_action, void*);