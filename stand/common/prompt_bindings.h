
#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/queue.h>

struct prompt_input {
	char mods;
	char key;
};

typedef void(*prompt_action)(void*);

struct prompt_keybind {
	struct prompt_input target;
	prompt_action action;
	
	STAILQ_ENTRY(prompt_keybind) next;
};

struct prompt_predefined_action {
	char *name;
	prompt_action action;
	
	STAILQ_ENTRY(prompt_predefined_action) next;
};

#define	PROMPT_MOD_SHIFT 1
#define	PROMPT_MOD_ALT 2
#define	PROMPT_MOD_CTRL 4

#define	PROMPT_ANSI_TO_KEY 0x80
#define	PROMPT_KEY_UP (PROMPT_ANSI_TO_KEY + 'A')
#define	PROMPT_KEY_DOWN (PROMPT_ANSI_TO_KEY + 'B')
#define	PROMPT_KEY_RIGHT (PROMPT_ANSI_TO_KEY + 'C')
#define	PROMPT_KEY_LEFT (PROMPT_ANSI_TO_KEY + 'D')
#define	PROMPT_KEY_END (PROMPT_ANSI_TO_KEY + 'F')
#define	PROMPT_KEY_HOME (PROMPT_ANSI_TO_KEY + 'H')

#define	PROMPT_KEY_INSERT (PROMPT_ANSI_TO_KEY + 'E')
#define	PROMPT_KEY_DELETE (PROMPT_ANSI_TO_KEY + 'I')

#define	PROMPT_KEY_PGUP (PROMPT_ANSI_TO_KEY + 'J')
#define	PROMPT_KEY_PGDN (PROMPT_ANSI_TO_KEY + 'K')

struct prompt_input prompt_parse_input(char);
char prompt_input_to_char(struct prompt_input input);

/*
 * Takes a stream of input escapes, parses them and executes any bindings or
 * converts the input into a character and returns it.
 */
char prompt_on_input(char);

struct prompt_keybind *prompt_find_binding(char, char);
struct prompt_keybind *prompt_add_binding_raw(int, char, char, prompt_action);
struct prompt_keybind *prompt_add_binding(char, char, prompt_action);
void prompt_remove_binding(struct prompt_keybind *);

void prompt_stroke_to_string(char *, size_t, struct prompt_input);
void prompt_print_stroke(struct prompt_input);
struct prompt_input prompt_parse_stroke(const char *);

struct prompt_keybind *prompt_first_binding();
struct prompt_keybind *prompt_next_binding(struct prompt_keybind *);

#define	PREDEF_ACTION_SET(name, func)											\
	static struct prompt_predefined_action _predef_ ## func = { name, func };	\
	DATA_SET(Xpredef_action_set, _predef_ ## func)

SET_DECLARE(Xpredef_action_set, struct prompt_predefined_action);