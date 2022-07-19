#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <stand.h>
#include "bootstrap.h"

#include "prompt_bindings.h"

enum {
	esc_normal, 
	esc_esc, 
	esc_bracket,
	esc_bracket_either,
	esc_code_mods,
	esc_code_mods_end
} prompt_esc_state;

static char prompt_mods_or_code;
static char prompt_char_or_mods;

static void
unshift(struct prompt_input* result, char in) {
	if ('A' <= in && in <= 'Z') {
		result->mods |= PROMPT_MOD_SHIFT;
		in ^= 0x20;
	}
	
	result->key = in;
}

static char prompt_vt[] = {
	PROMPT_KEY_HOME, PROMPT_KEY_INSERT, PROMPT_KEY_DELETE, PROMPT_KEY_END,
	PROMPT_KEY_PGUP, PROMPT_KEY_PGDN, PROMPT_KEY_HOME, PROMPT_KEY_END
};

struct prompt_input
prompt_parse_input(char next) {
	struct prompt_input result = { 0 };
	
	switch (prompt_esc_state) {
		case esc_normal:
			if (next == 0x1b)
				prompt_esc_state = esc_esc;
			else if (next <= 0x1f && next != 0x8 && next != 0x9 && next != 0xd)
			{
				result.key = next - 1 + 'a';
				result.mods |= PROMPT_MOD_CTRL;
			}
			else
				unshift(&result, next);
			break;
		case esc_esc:
			if (next == '[')
				prompt_esc_state = esc_bracket;
			else 
			{
				result.mods |= PROMPT_MOD_ALT;
				unshift(&result, next);
				prompt_esc_state = esc_normal;
			}
			break;
		case esc_bracket:
			if ('A' <= next && next <= 'Z')
			{
				result.key = PROMPT_ANSI_TO_KEY + next;
				prompt_esc_state = esc_normal;
			}
			else if ('1' <= next && next <= '9')
			{
				prompt_esc_state = esc_bracket_either;
				prompt_mods_or_code = next;
			}
			else 
			{
				result.mods |= PROMPT_MOD_ALT;
				unshift(&result, next);
				prompt_esc_state = esc_normal;
			}
			break;
		case esc_bracket_either:
			if (next == ';')
				prompt_esc_state = esc_code_mods;
			else if (next == '~')
			{
				prompt_mods_or_code -= '1';
				
				if (prompt_mods_or_code < 8)
					result.key = prompt_vt[(int)prompt_mods_or_code];
				
				prompt_esc_state = esc_normal;
			}
			else 
			{
				result.mods = prompt_mods_or_code - '0' - 1;
				
				unshift(&result, next);
				
				prompt_esc_state = esc_normal;
			}
			break;
		case esc_code_mods:
			prompt_char_or_mods = next;
			prompt_esc_state = esc_code_mods_end;
			break;
		case esc_code_mods_end:
			result.mods = prompt_char_or_mods - '0' - 1;
			
			if (next == '~')
			{
				prompt_mods_or_code -= '1';
				
				if (prompt_mods_or_code < 8)
					result.key = prompt_vt[(int)prompt_mods_or_code];
			}
			else if (prompt_mods_or_code == '1' && ('A' <= next && next <= 'Z'))
			{
				result.key = PROMPT_ANSI_TO_KEY + next;
			}
			
			prompt_esc_state = esc_normal;
			
			break;
	}
	
	return result;
}

char
prompt_input_to_char(struct prompt_input input) {
	if (input.key > 0) {
		if (input.mods & PROMPT_MOD_SHIFT) {
			if ('a' <= input.key && input.key <= 'z')
				return input.key ^ 0x20;
		}
		else if (input.mods)
			return 0;
		
		return input.key;
	}
	
	return 0;
}

STAILQ_HEAD(prompt_binds, prompt_keybind) prompt_binds_head =
	 STAILQ_HEAD_INITIALIZER(prompt_binds_head);

struct prompt_keybind*
prompt_find_binding(char mods, char key) {
	struct prompt_keybind* p;
	
	STAILQ_FOREACH(p, &prompt_binds_head, next) {
		if (p->target.mods == mods && p->target.key == key)
			return p;
	}
	
	return NULL;
}

struct prompt_keybind*
prompt_add_binding_raw(int extraspace, char mods, char key, prompt_action action) {
	struct prompt_keybind* result = prompt_find_binding(mods, key);
	int new = result == NULL;
	
	if (new)
		result = malloc(sizeof(struct prompt_keybind) + extraspace);
	
	result->target.mods = mods;
	result->target.key = key;
	result->action = action;
	
	if (new)
		STAILQ_INSERT_TAIL(&prompt_binds_head, result, next);
	
	return result;
}

struct prompt_keybind*
prompt_add_binding(char mods, char key, prompt_action action) {
	return prompt_add_binding_raw(0, mods, key, action);
}

void
prompt_remove_binding(struct prompt_keybind* bind) {
	STAILQ_REMOVE(&prompt_binds_head, bind, prompt_keybind, next);
	
	free(bind);
}

char
prompt_on_input(char in) {
	struct prompt_input input = prompt_parse_input(in);
	
	struct prompt_keybind* binding = prompt_find_binding(input.mods, input.key);
	
	if (binding != NULL) {
		binding->action(sizeof(struct prompt_keybind) + (void*)binding);
		return 0;
	}
	
	return prompt_input_to_char(input);
}

struct keyname_map {
	char* name;
	char key;
};

static struct keyname_map emacs_shortname_to_key[] = {
	{"BS", 0x8},
	{"TAB", 0x9},
	{"RET", 0x0a},
	{"ESC", 0x1b},
	{"SPC", 0x20},
	{"DEL", 0x7f},
	{0, 0}
};

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
lookup_key_from_name(struct keyname_map* map, const char* name) {
	int i = 0;
	
	while (map[i].key != 0) {
		if (strcmp(map[i].name, name) == 0)
			return map[i].key;
		
		i += 1;
	}
	
	return 0;
}
static char*
lookup_name_from_key(struct keyname_map* map, const char key) {
	int i = 0;
	
	while (map[i].key != 0) {
		if (map[i].key == key)
			return map[i].name;;
		
		i += 1;
	}
	
	return 0;
}

void
prompt_print_stroke(struct prompt_input stroke) {
	if (stroke.mods) {
		if (stroke.mods & PROMPT_MOD_ALT) {
			printf("M-");
		}
		
		if (stroke.mods & PROMPT_MOD_CTRL) {
			printf("C-");
		}
		
		if (stroke.mods & PROMPT_MOD_SHIFT) {
			printf("S-");
		}
	}
	
	if (0 <= stroke.key && stroke.key <= 0x20) {
		char* name = lookup_name_from_key(emacs_shortname_to_key, stroke.key);
		
		if (name) {
			printf("%s", name);
		}
		else {
			printf("\\x%x", stroke.key);
		}
	}
	else if (0x20 <= stroke.key && stroke.key <= 0x126) {
		printf("%c", stroke.key);
	}
	else {
		char* name = lookup_name_from_key(emacs_longname_to_key, stroke.key);
		
		if (name) {
			printf("%s", name);
		}
		else {
			printf("\\x%x", stroke.key);
		}
	}
}

struct prompt_input
prompt_parse_stroke(const char* stroke) {
	struct prompt_input result = { 0 };
	
	int len = strlen(stroke);
	const char* p = stroke;
	
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
		if (p[0] == '<') {
			result.key = lookup_key_from_name(emacs_longname_to_key, p);
		}
		else {
			result.key = lookup_key_from_name(emacs_shortname_to_key, p);
		}
	}
	else {
		char ascii = *p;
		
		if ('A' <= ascii && ascii <= 'Z') {
			result.mods |= PROMPT_MOD_SHIFT;
			ascii ^= 0x20;
		}
		
		result.key = ascii;
	}
	
	return result;
}

struct prompt_keybind*
prompt_add_stroke_binding(char* stroke, prompt_action action) {
	struct prompt_input input = prompt_parse_stroke(stroke);
	
	if (input.mods == 0 && input.key == 0)
		return NULL;
	
	return prompt_add_binding(input.mods, input.key, action);
}

STAILQ_HEAD(prompt_actions, prompt_predefined_action) prompt_actions_head =
	 STAILQ_HEAD_INITIALIZER(prompt_actions_head);

struct prompt_predefined_action*
prompt_first_action() {
	return STAILQ_FIRST(&prompt_actions_head);
}

struct prompt_predefined_action* 
prompt_next_action(struct prompt_predefined_action* current) {
	return STAILQ_NEXT(current, next);
}

struct prompt_predefined_action* 
prompt_register_action(char* name, prompt_action action) {
	struct prompt_predefined_action* result = malloc(sizeof(struct prompt_predefined_action));
	
	result->name = name;
	result->action = action;
	
	STAILQ_INSERT_TAIL(&prompt_actions_head, result, next);
	
	return result;
}

static struct prompt_predefined_action*
find_predef_by_name(char* name) {
	struct prompt_predefined_action* p;
	
	STAILQ_FOREACH(p, &prompt_actions_head, next) {
		if (strcmp(p->name, name) == 0)
			return p;
	}
	
	return NULL;
}

static struct prompt_predefined_action*
find_predef_by_action(prompt_action action) {
	struct prompt_predefined_action* p;
	
	STAILQ_FOREACH(p, &prompt_actions_head, next) {
		if (p->action == action)
			return p;
	}
	
	return NULL;
}

struct prompt_keybind* 
prompt_add_stroke_action_binding(char* stroke, char* action_name) {
	struct prompt_predefined_action* predef = find_predef_by_name(action_name);
	
	if (predef == NULL)
		return NULL;
	
	return prompt_add_stroke_binding(stroke, predef->action);
}

COMMAND_SET(keybind, "keybind", "bind a key to an action", command_keybind);

static int
command_keybind(int argc, char *argv[]) {
	if (argc != 3) {
		command_errmsg = "wrong number of arguments";
		return CMD_ERROR;
	}
	
	struct prompt_keybind* bind = prompt_add_stroke_action_binding(argv[1], argv[2]);
	
	if (bind != NULL) {
		return CMD_OK;
	}
	else {
		snprintf(command_errbuf, sizeof(command_errbuf), "could not bind '%s' to '%s'", argv[1], argv[2]);
		
		return CMD_ERROR;
	}
}

COMMAND_SET(keybinds, "keybinds", "list bound keys", command_keybinds);

static int
command_keybinds(int argc, char *argv[]) {
	struct prompt_keybind* p;
	
	STAILQ_FOREACH(p, &prompt_binds_head, next) {
		prompt_print_stroke(p->target);
		
		struct prompt_predefined_action* predef = find_predef_by_action(p->action);
		
		if (predef != NULL) {
			printf(" %s\n", predef->name);
		}
		else {
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
	
	struct prompt_keybind* bind = prompt_find_binding(stroke.mods, stroke.key);
	
	if (bind == NULL) {
		command_errmsg = "could not find any binding for key stroke";
		return CMD_ERROR;
	}
	
	prompt_remove_binding(bind);
	
	return CMD_OK;
}

/*
static int
prompt_instance_index(lua_State *L)
{
	
	
}

static const struct luaL_Reg bufferlib[] = {
	{"__index", buffer_instance_index},
	{"__newindex", buffer_instance_newindex}
}

static const struct luaL_Reg prompt_instance_meta[] = {
	{"__index", prompt_instance_index},
	{"cursor", prompt_instance_cursor}
	{ NULL, NULL },
};

static int
lua_pushprompt(lua_State *L, struct prompt_line_buffer* buffer) {
	lua_newtable(L);
	
	lua_pushlightuserdata(L, buffer);
	lua_setfield(L, -2, "pbuffer");
	
	lua_getglobal(L, "loader_prompt");
	lua_setmetatable(L, -2);
	
	return 1;
}

int
luaopen_prompt(lua_State *L)
{
	luaL_newlib(L, keybindlib);
	
	lua_pushstring(L, "");
	lua_setfield(L, -2, "machine");
	lua_pushstring(L, MACHINE_ARCH);
	lua_setfield(L, -2, "machine_arch");
	lua_pushstring(L, LUA_PATH);
	lua_setfield(L, -2, "lua_path");
	
	luaL_newmetatable(L, "loader_prompt");
	luaL_register(L, NULL, prompt_instance_meta);
	lua_pop(L, 1);
	
	return 1;
}
*/