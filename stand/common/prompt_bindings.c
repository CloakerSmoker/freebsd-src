/*-
 * Copyright (c) 2022 Connor Bailey
 *
 * SPDX-License-Identifier: BSD-2-clause
 */

#include <stand.h>
#include "bootstrap.h"

#include "prompt_bindings.h"

#define BS '\x8'
#define TAB '\x9'
#define CR '\xd'
#define ESC '\x1b'
#define DEL '\x7f'

/*
 * In the input escape parser, we need to handle:
 * "char"
 * "ESC char"
 * "ESC [ code ~"
 * "ESC [ char ; mods"
 * "ESC [ mods ; code ~"
 * each of which is assigned a state, with the proper transitions between each.
 *
 * Special keys are just (PROMPT_ANSI_TO_KEY + ANSI_CODE) with unused ANSI codes
 * picked for VT codes.
 */

enum {
	ESC_NORMAL,
	ESC_ESC,
	ESC_BRACKET,
	ESC_BRACKET_EITHER,
	ESC_CODE_MODS,
	ESC_CODE_MODS_END
} prompt_esc_state;

static char prompt_mods_or_code;
static char prompt_char_or_mods;

static void
unshift(struct prompt_input *result, char in)
{
	if (isupper(in)) {
		result->mods |= PROMPT_MOD_SHIFT;
		in = tolower(in);
	}
	
	result->key = in;
}

/* Map [1-9]~ VT codes back into keycodes */

static char prompt_vt[] = {
	PROMPT_KEY_HOME, PROMPT_KEY_INSERT, PROMPT_KEY_DELETE, PROMPT_KEY_END,
	PROMPT_KEY_PGUP, PROMPT_KEY_PGDN, PROMPT_KEY_HOME, PROMPT_KEY_END
};

struct prompt_input
prompt_parse_input(char next)
{
	struct prompt_input result = { 0 };
	
	switch (prompt_esc_state) {
		case ESC_NORMAL:
			if (next == ESC) {
				prompt_esc_state = ESC_ESC;
			}
			else if (iscntrl(next) && next != BS && next != TAB && next != CR) {
				/*
				 * Control characters turn into ctrl+key
				 */
				
				result.key = next - 1 + 'a';
				result.mods |= PROMPT_MOD_CTRL;
			} else {
				unshift(&result, next);
			}
			break;
		case ESC_ESC:
			if (next == '[') {
				prompt_esc_state = ESC_BRACKET;
			} else {
				/* "ESC char" turns back into alt+char */
				
				result.mods |= PROMPT_MOD_ALT;
				unshift(&result, next);
				prompt_esc_state = ESC_NORMAL;
			}
			break;
		case ESC_BRACKET:
			if ('A' <= next && next <= 'Z') {
				/*
				 * Plain ANSI escapes without modifiers terminate after the first
				 * non-numeric character
				 */
				
				result.key = PROMPT_ANSI_TO_KEY + next;
				prompt_esc_state = ESC_NORMAL;
			}
			else if ('1' <= next && next <= '9') {
				prompt_esc_state = ESC_BRACKET_EITHER;
				prompt_mods_or_code = next;
			} else {
				/*
				 * "ESC [ char" into alt+char, mainly for "ESC [ [" to work
				 */
				
				result.mods |= PROMPT_MOD_ALT;
				unshift(&result, next);
				prompt_esc_state = ESC_NORMAL;
			}
			break;
		case ESC_BRACKET_EITHER:
			if (next == ';') {
				prompt_esc_state = ESC_CODE_MODS;
			}
			else if (next == '~') {
				/*
				 * VT escape terminated by ~, mods_or_code is a code
				 */
				
				prompt_mods_or_code -= '1';
				
				if (prompt_mods_or_code < 8) {
					result.key = prompt_vt[(int)prompt_mods_or_code];
				}
				
				prompt_esc_state = ESC_NORMAL;
			} else {
				/*
				 * ESC [ mods ; char
				 */
				
				result.mods = prompt_mods_or_code - '0' - 1;
				
				unshift(&result, next);
				
				prompt_esc_state = ESC_NORMAL;
			}
			break;
		case ESC_CODE_MODS:
			prompt_char_or_mods = next;
			prompt_esc_state = ESC_CODE_MODS_END;
			break;
		case ESC_CODE_MODS_END:
			result.mods = prompt_char_or_mods - '0' - 1;
			
			if (next == '~') {
				/* VT escape terminated by ~, mods_or_code is a code */
				
				prompt_mods_or_code -= '1';
				
				if (prompt_mods_or_code < 8) {
					result.key = prompt_vt[(int)prompt_mods_or_code];
				}
			}
			else if (prompt_mods_or_code == '1' && ('A' <= next && next <= 'Z')) {
				/* ANSI escape in the "ESC [ mods ; char" format */
				
				result.key = PROMPT_ANSI_TO_KEY + next;
			}
			
			prompt_esc_state = ESC_NORMAL;
			
			break;
	}
	
	return result;
}

char
prompt_input_to_char(struct prompt_input input)
{
	if (input.key > 0) {
		if (input.mods & PROMPT_MOD_SHIFT) {
			if (islower(input.key)) {
				return toupper(input.key);
			}
		}
		else if (input.mods) {
			return 0;
		}
		
		return input.key;
	}
	
	return 0;
}

STAILQ_HEAD(prompt_binds, prompt_keybind) prompt_binds_head =
	 STAILQ_HEAD_INITIALIZER(prompt_binds_head);

struct prompt_keybind *
prompt_find_binding(char mods, char key)
{
	struct prompt_keybind *p;
	
	STAILQ_FOREACH(p, &prompt_binds_head, next) {
		if (p->target.mods == mods && p->target.key == key) {
			return p;
		}
	}
	
	return NULL;
}

struct prompt_keybind *
prompt_add_binding_raw(int extraspace, char mods, char key, prompt_action action)
{
	/*
	 * Allocate extra space on the end of the result for the caller to use how
	 * they like, it will be passed to their callback.
	 */
	
	struct prompt_keybind* result = prompt_find_binding(mods, key);
	int new = result == NULL;
	
	if (new) {
		result = malloc(sizeof(struct prompt_keybind) + extraspace);
	}
	
	result->target.mods = mods;
	result->target.key = key;
	result->action = action;
	
	if (new) {
		STAILQ_INSERT_TAIL(&prompt_binds_head, result, next);
	}
	
	return result;
}

struct prompt_keybind *
prompt_add_binding(char mods, char key, prompt_action action)
{
	return prompt_add_binding_raw(0, mods, key, action);
}

void
prompt_remove_binding(struct prompt_keybind *bind)
{
	STAILQ_REMOVE(&prompt_binds_head, bind, prompt_keybind, next);
	
	free(bind);
}

char
prompt_on_input(char in)
{
	struct prompt_input input = prompt_parse_input(in);
	
	struct prompt_keybind *binding = prompt_find_binding(input.mods, input.key);
	
	if (binding != NULL) {
		/*
		 * Pass any caller data stored past the end of the binding
		 */
		
		binding->action(sizeof(struct prompt_keybind) + (void*)binding);
		return 0;
	}
	
	return prompt_input_to_char(input);
}

struct keyname_map {
	char *name;
	char key;
};

/*
 * Control characters
 */

static struct keyname_map emacs_shortname_to_key[] = {
	{"BS", BS},
	{"TAB", TAB},
	{"RET", CR},
	{"ESC", ESC},
	{"SPC", ' '},
	{"DEL", DEL},
	{0, 0}
};

/*
 * Special characters
 */

static struct keyname_map emacs_longname_to_key[] = {
	{"<left>", PROMPT_KEY_LEFT},
	{"<up>", PROMPT_KEY_UP},
	{"<right>", PROMPT_KEY_RIGHT},
	{"<down>", PROMPT_KEY_DOWN},
	{"<end>", PROMPT_KEY_END},
	{"<home>", PROMPT_KEY_HOME},
	{"<insert>", PROMPT_KEY_INSERT},
	{"<delete>", PROMPT_KEY_DELETE},
	{"<prior>", PROMPT_KEY_PGUP},
	{"<next>", PROMPT_KEY_PGDN},
	{0, 0}
};

static char
lookup_key_from_name(struct keyname_map *map, const char *name)
{
	int i = 0;
	
	while (map[i].key != 0) {
		if (strcmp(map[i].name, name) == 0) {
			return map[i].key;
		}
		
		i++;
	}
	
	return 0;
}
static char *
lookup_name_from_key(struct keyname_map *map, const char key)
{
	int i = 0;
	
	while (map[i].key != 0) {
		if (map[i].key == key) {
			return map[i].name;
		}
		
		i++;
	}
	
	return 0;
}

void
prompt_stroke_to_string(char *buf, size_t len, struct prompt_input stroke)
{
	int off = 0;
	
	if (stroke.mods) {
		if (stroke.mods & PROMPT_MOD_ALT) {
			off += snprintf(&buf[off], len, "M-");
		}
		
		if (stroke.mods & PROMPT_MOD_CTRL) {
			off += snprintf(&buf[off], len, "C-");
		}
		
		if (stroke.mods & PROMPT_MOD_SHIFT) {
			off += snprintf(&buf[off], len, "S-");
		}
	}
	
	if (iscntrl(stroke.key)) {
		/*
		 * Control characters
		 */
		
		char *name = lookup_name_from_key(emacs_shortname_to_key, stroke.key);
		
		if (name) {
			off += snprintf(&buf[off], len, "%s", name);
		} else {
			off += snprintf(&buf[off], len, "\\x%x", stroke.key);
		}
	}
	else if (!iscntrl(stroke.key) && stroke.key != ' ') {
		/*
		 * Printable characters (no isprint, easy enough to fake)
		 */
		
		off += snprintf(&buf[off], len, "%c", stroke.key);
	} else {
		/*
		 * Special characters
		 */
		
		char *name = lookup_name_from_key(emacs_longname_to_key, stroke.key);
		
		if (name) {
			off += snprintf(&buf[off], len, "%s", name);
		} else {
			off += snprintf(&buf[off], len, "\\x%x", stroke.key);
		}
	}
}

void
prompt_print_stroke(struct prompt_input stroke)
{
	char buf[20] = { 0 };
	
	prompt_stroke_to_string(buf, sizeof(buf), stroke);
	
	printf("%s", buf);
}

struct prompt_input
prompt_parse_stroke(const char *stroke)
{
	struct prompt_input result = { 0 };
	
	int len = strlen(stroke);
	const char *p = stroke;
	
	/*
	 * Any number of "X-" modifiers, back to back
	 */
	
	while (len >= 3 && p[1] == '-') {
		switch (*p) {
			case 'C':
				result.mods |= PROMPT_MOD_CTRL;
				break;
			case 'M':
				result.mods |= PROMPT_MOD_ALT;
				break;
			case 'S':
				result.mods |= PROMPT_MOD_SHIFT;
				break;
		}
		
		p += 2;
		len -= 2;
	}
	
	if (len != 1) {
		/*
		 * More left to parse than a single character like "M-x"
		 */
		
		if (p[0] == '<') {
			/*
			 * "M-<special>"
			 */
			
			result.key = lookup_key_from_name(emacs_longname_to_key, p);
		} else {
			/*
			 * "M-CTRL"
			 */
			
			result.key = lookup_key_from_name(emacs_shortname_to_key, p);
		}
	} else {
		/*
		 * A single character, might include shift as a modifier if it is uppercase
		 */
		
		char ascii = *p;
		
		if (isupper(ascii)) {
			result.mods |= PROMPT_MOD_SHIFT;
			ascii = tolower(ascii);
		}
		
		result.key = ascii;
	}
	
	return result;
}

struct prompt_keybind *
prompt_first_binding()
{
	return STAILQ_FIRST(&prompt_binds_head);
}
struct prompt_keybind *
prompt_next_binding(struct prompt_keybind* bind)
{
	return STAILQ_NEXT(bind, next);
}

struct prompt_keybind *
prompt_add_stroke_binding(char *stroke, prompt_action action)
{
	struct prompt_input input = prompt_parse_stroke(stroke);
	
	if (input.mods == 0 && input.key == 0) {
		return NULL;
	}
	
	return prompt_add_binding(input.mods, input.key, action);
}

/*
 * Predefined actions just map names to functions, mainly for simp since it isn't
 * possible to define a callback.
 * Still useful for Lua though, since otherwise Lua would need to manually
 * implement every editing option instead of just using the predefined ones.
 *
 * Lua translates Xpredef_action_set into the keybind.actions table at runtime
 * and then each entry in keybind.actions can be passed to keybind.register to
 * accomplish the same as the simp "keybind" command.
 */

static struct prompt_predefined_action *
find_predef_by_name(char *name)
{
	struct prompt_predefined_action **ppa;
	
	SET_FOREACH(ppa, Xpredef_action_set) {
		struct prompt_predefined_action *a = *ppa;
		
		if (strcmp(a->name, name) == 0) {
			return a;
		}
	}
	
	return NULL;
}

static struct prompt_predefined_action *
find_predef_by_action(prompt_action action)
{
	struct prompt_predefined_action **ppa;
	
	SET_FOREACH(ppa, Xpredef_action_set) {
		struct prompt_predefined_action *a = *ppa;
		
		if (a->action == action) {
			return a;
		}
	}
	
	return NULL;
}

struct prompt_keybind *
prompt_add_stroke_action_binding(char *stroke, char *action_name)
{
	struct prompt_predefined_action *predef = find_predef_by_name(action_name);
	
	if (predef == NULL) {
		return NULL;
	}
	
	return prompt_add_stroke_binding(stroke, predef->action);
}

COMMAND_SET(keybind, "keybind", "bind a key to an action", command_keybind);

static int
command_keybind(int argc, char *argv[])
{
	if (argc != 3) {
		command_errmsg = "wrong number of arguments";
		return CMD_ERROR;
	}
	
	struct prompt_keybind *bind = prompt_add_stroke_action_binding(argv[1], argv[2]);
	
	if (bind != NULL) {
		return CMD_OK;
	} else {
		snprintf(command_errbuf, sizeof(command_errbuf), "could not bind '%s' to '%s'", argv[1], argv[2]);
		
		return CMD_ERROR;
	}
}

COMMAND_SET(keybinds, "keybinds", "list bound keys", command_keybinds);

static int
command_keybinds(int argc, char *argv[])
{
	struct prompt_keybind *p;
	
	STAILQ_FOREACH(p, &prompt_binds_head, next) {
		prompt_print_stroke(p->target);
		
		struct prompt_predefined_action *predef = find_predef_by_action(p->action);
		
		if (predef != NULL) {
			printf(" %s\n", predef->name);
		} else {
			/*
			 * Wouldn't be very kind of us to dig into Lua's data stored after the
			 * binding, so we have to default to a generic name for Lua actions.
			 */
			
			printf(" <interpreter callback %p>\n", p->action);
		}
	}
	
	return CMD_OK;
}

COMMAND_SET(keyunbind, "keyunbind", "unbind a previously bound key", command_keyunbind);

static int
command_keyunbind(int argc, char *argv[]) {
	if (argc != 2) {
		command_errmsg = "wrong number of arguments";
		return CMD_ERROR;
	}
	
	struct prompt_input stroke = prompt_parse_stroke(argv[1]);
	
	if (stroke.mods == 0 && stroke.key == 0) {
		command_errmsg = "could not parse key stroke";
		return CMD_ERROR;
	}
	
	struct prompt_keybind *bind = prompt_find_binding(stroke.mods, stroke.key);
	
	if (bind == NULL) {
		command_errmsg = "could not find any binding for key stroke";
		return CMD_ERROR;
	}
	
	prompt_remove_binding(bind);
	
	return CMD_OK;
}

COMMAND_SET(showkey, "showkey", "shows how a single keystroke is parsed", command_showkey);

static int
command_showkey(int argc, char *argv[]) {
	if (argc != 1) {
		command_errmsg = "wrong number of arguments";
		return CMD_ERROR;
	}
	
	struct prompt_input in = { 0 };
	
	while (in.mods == 0 && in.key == 0)
		in = prompt_parse_input(getchar());
	
	prompt_print_stroke(in);
	printf("\n");
	
	return CMD_OK;
}
