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

static void interact_unshift(struct interact_input* result, char in)
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
			else if (next <= 0x27 && next != 0x8 && next != 0x9 && next != 0xd)
			{
				result.key = next - 1 + 'a';
				result.mods |= INTERP_MOD_CTRL;
			}
			else
				interact_unshift(&result, next);
			break;
		case esc_esc:
			if (next == '[')
				interact_esc_state = esc_bracket;
			else 
			{
				result.mods |= INTERP_MOD_ALT;
				interact_unshift(&result, next);
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
				interact_unshift(&result, next);
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
				
				interact_unshift(&result, next);
				
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

static struct interact_keybind* interact_find_binding(char mods, char key)
{
	struct interact_keybind* p;
	
	STAILQ_FOREACH(p, &interact_binds_head, next) {
		if (p->target.mods == mods && p->target.key == key)
			return p;
	}
	
	return NULL;
}

struct interact_keybind* interact_add_binding(char mods, char key, interact_action action, void* parameter)
{
	struct interact_keybind* result = interact_find_binding(mods, key);
	
	if (result == NULL)
		result = malloc(sizeof(struct interact_keybind));
	
	result->target.mods = mods;
	result->target.key = key;
	result->action = action;
	result->parameter = parameter;
	
	STAILQ_INSERT_TAIL(&interact_binds_head, result, next);
	
	return result;
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