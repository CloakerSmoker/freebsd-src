/*-
 * Copyright (c) 1998 Michael Smith <msmith@freebsd.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

/*
 * Simple commandline interpreter, toplevel and misc.
 *
 * XXX may be obsoleted by BootFORTH or some other, better, interpreter.
 */

#include <stand.h>
#include <string.h>
#include "bootstrap.h"

#define	MAXARGS	20			/* maximum number of arguments allowed */

struct interact_input {
	char mods;
	char key;
};

enum {
	esc_normal, 
	esc_esc, 
	esc_bracket,
	esc_bracket_either,
	esc_code_mods,
	esc_code_mods_end
} interact_esc_state;

char interact_mods_or_code;
char interact_char_or_mods;

#define INTERP_MOD_SHIFT 1
#define INTERP_MOD_ALT 2
#define INTERP_MOD_CTRL 4

static void interact_unshift(struct interact_input* result, char in)
{
	if ('A' <= in && in <= 'Z')
	{
		result->mods |= INTERP_MOD_SHIFT;
		in ^= 0x20;
	}
	
	result->key = in;
}

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

static char interact_vt[] = {
	INTERP_KEY_HOME, INTERP_KEY_INSERT, INTERP_KEY_DELETE, INTERP_KEY_END,
	INTERP_KEY_PGUP, INTERP_KEY_PGDN, INTERP_KEY_HOME, INTERP_KEY_END
};

static struct interact_input
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

static char
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

/*
 * Interactive mode
 */
void
interact(void)
{
	static char		input[256];		/* big enough? */
<<<<<<< HEAD
=======
	const char * volatile	interp_identifier;
	int input_index;
>>>>>>> 900d6b9cd57e (stand/common: parse more complex input escapes)

	TSENTER();

	/*
	 * Because interp_identifier is volatile, it cannot be optimized out by
	 * the compiler as it's considered an externally observable event.  This
	 * prevents the compiler from optimizing out our carefully placed
	 * $Interpreter:4th string that userboot may use to determine that
	 * we need to switch interpreters.
	 */
	interp_identifier = bootprog_interp;
	interp_init();

	printf("\n");

	/*
	 * Before interacting, we might want to autoboot.
	 */
	autoboot_maybe();

	/*
	 * Not autobooting, go manual
	 */
	printf("\nType '?' for a list of commands, 'help' for more detailed help.\n");
	if (getenv("prompt") == NULL)
		setenv("prompt", "${interpret}", 1);
	if (getenv("interpret") == NULL)
		setenv("interpret", "OK", 1);

	for (;;) {
		input_index = 0;
		interp_emit_prompt();
		
		//printf("\n");
		
		for (;;) {
			char n = getchar();
			
			printf("[%x %c %i] ", n, n, interact_esc_state);
			
			struct interact_input i = interact_parse_input(n);
			char in = interact_input_to_char(i);
			
			if (i.key == 0xd)
			{
				printf("\n");
				break;
			}
			else if (i.key > 0)
			{
				printf("(%x %x) ", i.mods, i.key);
			}
			else if (in != 0)
			{
				input[input_index++] = in;
				
				//printf("%c", in);
			}
		}
		
		input[input_index] = '\0';
		interp_run(input);
	}
}

/*
 * Read commands from a file, then execute them.
 *
 * We store the commands in memory and close the source file so that the media
 * holding it can safely go away while we are executing.
 *
 * Commands may be prefixed with '@' (so they aren't displayed) or '-' (so
 * that the script won't stop if they fail).
 */
COMMAND_SET(include, "include", "read commands from a file", command_include);

static int
command_include(int argc, char *argv[])
{
	int		i;
	int		res;
	char		**argvbuf;

	/*
	 * Since argv is static, we need to save it here.
	 */
	argvbuf = (char**) calloc((u_int)argc, sizeof(char*));
	for (i = 0; i < argc; i++)
		argvbuf[i] = strdup(argv[i]);

	res=CMD_OK;
	for (i = 1; (i < argc) && (res == CMD_OK); i++)
		res = interp_include(argvbuf[i]);

	for (i = 0; i < argc; i++)
		free(argvbuf[i]);
	free(argvbuf);

	return(res);
}

/*
 * Emit the current prompt; use the same syntax as the parser
 * for embedding environment variables. Does not accept input.
 */
void
interp_emit_prompt(void)
{
	char		*pr, *p, *cp, *ev;

	if ((cp = getenv("prompt")) == NULL)
		cp = ">";
	pr = p = strdup(cp);

	while (*p != 0) {
		if ((*p == '$') && (*(p+1) == '{')) {
			for (cp = p + 2; (*cp != 0) && (*cp != '}'); cp++)
				;
			*cp = 0;
			ev = getenv(p + 2);

			if (ev != NULL)
				printf("%s", ev);
			p = cp + 1;
			continue;
		}
		putchar(*p++);
	}
	putchar(' ');
	free(pr);
}

static struct bootblk_command *
interp_lookup_cmd(const char *cmd)
{
	struct bootblk_command	**cmdp;

	/* search the command set for the command */
	SET_FOREACH(cmdp, Xcommand_set) {
		if (((*cmdp)->c_name != NULL) && !strcmp(cmd, (*cmdp)->c_name))
			return (*cmdp);
	}
	return (NULL);
}

/*
 * Perform a builtin command
 */
int
interp_builtin_cmd(int argc, char *argv[])
{
	int			result;
	struct bootblk_command	*cmd;

	if (argc < 1)
		return (CMD_OK);

	/* set return defaults; a successful command will override these */
	command_errmsg = command_errbuf;
	strcpy(command_errbuf, "no error message");
	result = CMD_ERROR;

	cmd = interp_lookup_cmd(argv[0]);
	if (cmd != NULL && cmd->c_fn) {
		result = cmd->c_fn(argc, argv);
	} else {
		command_errmsg = "unknown command";
	}
	return (result);
}

/*
 * Return true if the builtin command exists
 */
bool
interp_has_builtin_cmd(const char *cmd)
{
	return (interp_lookup_cmd(cmd) != NULL);
}
