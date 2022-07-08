#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <stand.h>
#include "bootstrap.h"

#include "interp_bindings.h"

enum {
	esc_normal, 
	esc_esc, 
	esc_bracket,
	esc_bracket_either,
	esc_code_mods,
	esc_code_mods_end
} interact_esc_state;

static char interact_mods_or_code;
static char interact_char_or_mods;

static void unshift(struct interact_input* result, char in)
{
	if ('A' <= in && in <= 'Z')
	{
		result->mods |= INTERP_MOD_SHIFT;
		in ^= 0x20;
	}
	
	result->key = in;
}

static char interact_vt[] = {
	INTERP_KEY_HOME, INTERP_KEY_INSERT, INTERP_KEY_DELETE, INTERP_KEY_END,
	INTERP_KEY_PGUP, INTERP_KEY_PGDN, INTERP_KEY_HOME, INTERP_KEY_END
};

struct interact_input
interact_parse_input(char next)
{
	struct interact_input result = { 0 };
	
	switch (interact_esc_state) {
		case esc_normal:
			if (next == 0x1b)
				interact_esc_state = esc_esc;
			else if (next <= 0x1f && next != 0x8 && next != 0x9 && next != 0xd)
			{
				result.key = next - 1 + 'a';
				result.mods |= INTERP_MOD_CTRL;
			}
			else
				unshift(&result, next);
			break;
		case esc_esc:
			if (next == '[')
				interact_esc_state = esc_bracket;
			else 
			{
				result.mods |= INTERP_MOD_ALT;
				unshift(&result, next);
				interact_esc_state = esc_normal;
			}
			break;
		case esc_bracket:
			if ('A' <= next && next <= 'Z')
			{
				result.key = INTERP_ANSI_TO_KEY + next;
				interact_esc_state = esc_normal;
			}
			else if ('1' <= next && next <= '9')
			{
				interact_esc_state = esc_bracket_either;
				interact_mods_or_code = next;
			}
			else 
			{
				result.mods |= INTERP_MOD_ALT;
				unshift(&result, next);
				interact_esc_state = esc_normal;
			}
			break;
		case esc_bracket_either:
			if (next == ';')
				interact_esc_state = esc_code_mods;
			else if (next == '~')
			{
				interact_mods_or_code -= '1';
				
				if (interact_mods_or_code < 8)
					result.key = interact_vt[(int)interact_mods_or_code];
				
				interact_esc_state = esc_normal;
			}
			else 
			{
				result.mods = interact_mods_or_code - '0' - 1;
				
				unshift(&result, next);
				
				interact_esc_state = esc_normal;
			}
			break;
		case esc_code_mods:
			interact_char_or_mods = next;
			interact_esc_state = esc_code_mods_end;
			break;
		case esc_code_mods_end:
			result.mods = interact_char_or_mods - '0' - 1;
			
			if (next == '~')
			{
				interact_mods_or_code -= '1';
				
				if (interact_mods_or_code < 8)
					result.key = interact_vt[(int)interact_mods_or_code];
			}
			else if (interact_mods_or_code == '1' && ('A' <= next && next <= 'Z'))
			{
				result.key = INTERP_ANSI_TO_KEY + next;
			}
			
			interact_esc_state = esc_normal;
			
			break;
	}
	
	return result;
}

char
interact_input_to_char(struct interact_input input)
{
	if (input.key > 0)
	{
		if (input.mods & INTERP_MOD_SHIFT)
		{
			if ('a' <= input.key && input.key <= 'z')
				return input.key ^ 0x20;
		}
		else if (input.mods)
			return 0;
		
		return input.key;
	}
	
	return 0;
}

STAILQ_HEAD(interact_binds, interact_keybind) interact_binds_head =
	 STAILQ_HEAD_INITIALIZER(interact_binds_head);

struct interact_keybind* interact_find_binding(char mods, char key)
{
	struct interact_keybind* p;
	
	STAILQ_FOREACH(p, &interact_binds_head, next) {
		if (p->target.mods == mods && p->target.key == key)
			return p;
	}
	
	return NULL;
}

struct interact_keybind* interact_add_binding_raw(int extraspace, char mods, char key, interact_action action, void* parameter)
{
	struct interact_keybind* result = interact_find_binding(mods, key);
	int new = result == NULL;
	
	if (new)
		result = malloc(sizeof(struct interact_keybind) + extraspace);
	
	result->target.mods = mods;
	result->target.key = key;
	result->action = action;
	result->parameter = parameter;
	
	if (new)
		STAILQ_INSERT_TAIL(&interact_binds_head, result, next);
	
	return result;
}

struct interact_keybind* interact_add_binding(char mods, char key, interact_action action, void* parameter)
{
	return interact_add_binding_raw(0, mods, key, action, parameter);
}

void interact_remove_binding(struct interact_keybind* bind)
{
	STAILQ_REMOVE(&interact_binds_head, bind, interact_keybind, next);
	
	free(bind);
}

char interact_on_input(char in)
{
	struct interact_input input = interact_parse_input(in);
	
	struct interact_keybind* binding = interact_find_binding(input.mods, input.key);
	
	if (binding != NULL)
	{
		return binding->action(binding);
	}
	
	return interact_input_to_char(input);
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
	{"<left>", INTERP_KEY_LEFT},
	{"<up>", INTERP_KEY_UP},
	{"<right>", INTERP_KEY_RIGHT},
	{"<down>", INTERP_KEY_DOWN},
	{"<end>", INTERP_KEY_END},
	{"<home>", INTERP_KEY_HOME},
	{"<insert>", INTERP_KEY_INSERT},
	{"<delete>", INTERP_KEY_DELETE},
	{"<prior>", INTERP_KEY_PGUP},
	{"<next>", INTERP_KEY_PGDN},
	{0, 0}
};

static char lookup_key_from_name(struct keyname_map* map, const char* name)
{
	int i = 0;
	
	while (map[i].key != 0) {
		if (strcmp(map[i].name, name) == 0)
			return map[i].key;
		
		i += 1;
	}
	
	return 0;
}
static char* lookup_name_from_key(struct keyname_map* map, const char key)
{
	int i = 0;
	
	while (map[i].key != 0) {
		if (map[i].key == key)
			return map[i].name;;
		
		i += 1;
	}
	
	return 0;
}

void interact_print_stroke(struct interact_input stroke)
{
	if (stroke.mods)
	{
		if (stroke.mods & INTERP_MOD_ALT) {
			printf("M-");
		}
		
		if (stroke.mods & INTERP_MOD_CTRL) {
			printf("C-");
		}
		
		if (stroke.mods & INTERP_MOD_SHIFT) {
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

struct interact_input interact_parse_stroke(const char* stroke)
{
	struct interact_input result = { 0 };
	
	int len = strlen(stroke);
	const char* p = stroke;
	
	while (len >= 3 && p[1] == '-')
	{
		switch (*p)
		{
			case 'C':
				result.mods |= INTERP_MOD_CTRL;
				break;
			case 'M':
				result.mods |= INTERP_MOD_ALT;
				break;
			case 'S':
				result.mods |= INTERP_MOD_SHIFT;
				break;
		}
		
		p += 2;
		len -= 2;
	}
	
	if (len != 1)
	{
		if (p[0] == '<')
		{
			result.key = lookup_key_from_name(emacs_longname_to_key, p);
		}
		else
		{
			result.key = lookup_key_from_name(emacs_shortname_to_key, p);
		}
	}
	else
	{
		char ascii = *p;
		
		if ('A' <= ascii && ascii <= 'Z')
		{
			result.mods |= INTERP_MOD_SHIFT;
			ascii ^= 0x20;
		}
		
		result.key = ascii;
	}
	
	return result;
}

struct interact_keybind* interact_add_stroke_binding(char* stroke, interact_action action, void* parameter)
{
	struct interact_input input = interact_parse_stroke(stroke);
	
	return interact_add_binding(input.mods, input.key, action, parameter);
}

STAILQ_HEAD(interact_actions, interact_predefined_action) interact_actions_head =
	 STAILQ_HEAD_INITIALIZER(interact_actions_head);

struct interact_predefined_action* interact_first_action()
{
	return STAILQ_FIRST(&interact_actions_head);
}

struct interact_predefined_action* interact_next_action(struct interact_predefined_action* current)
{
	return STAILQ_NEXT(current, next);
}

struct interact_predefined_action* interact_register_action(char* name, interact_action action, void* parameter)
{
	struct interact_predefined_action* result = malloc(sizeof(struct interact_predefined_action));
	
	result->name = name;
	result->action = action;
	result->parameter = parameter;
	
	STAILQ_INSERT_TAIL(&interact_actions_head, result, next);
	
	return result;
}

static struct interact_predefined_action* find_action(char* name)
{
	struct interact_predefined_action* p;
	
	STAILQ_FOREACH(p, &interact_actions_head, next) {
		if (strcmp(p->name, name) == 0)
			return p;
	}
	
	return NULL;
}

struct interact_keybind* interact_add_stroke_action_binding(char* stroke, char* action_name)
{
	struct interact_predefined_action* predef = find_action(action_name);
	
	if (predef == NULL)
		return NULL;
	
	return interact_add_stroke_binding(stroke, predef->action, predef->parameter);
}

COMMAND_SET(keybind, "keybind", "bind a key to an action", command_keybind);

static int
command_keybind(int argc, char *argv[])
{
	if (argc != 3)
		return 1;
	
	struct interact_keybind* bind = interact_add_stroke_action_binding(argv[1], argv[2]);
	
	if (bind != NULL) {
		printf("Bound ");
		interact_print_stroke(bind->target);
		printf(" to %s\n", argv[2]);
	}
	else {
		printf("Could not bind '%s' to '%s'\n", argv[1], argv[2]);
	}

	return (bind == NULL);
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
lua_pushprompt(lua_State *L, struct interact_line_buffer* buffer) {
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