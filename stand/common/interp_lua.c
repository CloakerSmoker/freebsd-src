/*-
 * Copyright (c) 2011 Wojciech A. Koszek <wkoszek@FreeBSD.org>
 * Copyright (c) 2014 Pedro Souza <pedrosouza@freebsd.org>
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

#include <stand.h>
#include "bootstrap.h"

#define lua_c

#include "lstd.h"

#include <lua.h>
#include <ldebug.h>
#include <lauxlib.h>
#include <lualib.h>

#include <lerrno.h>
#include <lfs.h>
#include <lutils.h>

#include "prompt_bindings.h"
#include "prompt_editing.h"

struct interp_lua_softc {
	lua_State	*luap;
};

static struct interp_lua_softc lua_softc;

#ifdef LUA_DEBUG
#define	LDBG(...)	do {			\
	printf("%s(%d): ", __func__, __LINE__);	\
	printf(__VA_ARGS__);			\
	printf("\n");				\
} while (0)
#else
#define	LDBG(...)
#endif

#define	LOADER_LUA	LUA_PATH "/loader.lua"

INTERP_DEFINE("lua");

static void *
interp_lua_realloc(void *ud __unused, void *ptr, size_t osize __unused, size_t nsize)
{

	if (nsize == 0) {
		free(ptr);
		return NULL;
	}
	return realloc(ptr, nsize);
}

/*
 * Appended onto each keybind created from Lua, callback_ref
 * is just the registry index of a callback
 */

struct lua_keybind {
	int callback_ref;
};

static void
lua_keybind_handler(void* data)
{
	struct lua_keybind* bind = data;
	lua_State *L = lua_softc.luap;
	
	lua_rawgeti(L, LUA_REGISTRYINDEX, bind->callback_ref);
	
	lua_pcall(L, 0, 0, 0);
}

static void
delete_bind(lua_State* L, struct prompt_keybind* bind) {
	/*
	 * Delete a binding, unref-ing along the way if Lua owns it
	 */
	
	if (bind->action == lua_keybind_handler) {
		void* data = sizeof(struct prompt_keybind) + (void*)bind;
		struct lua_keybind* luabind = data;
		
		luaL_unref(L, LUA_REGISTRYINDEX, luabind->callback_ref);
	}
	
	prompt_remove_binding(bind);
}

static int
lua_keybind_register(lua_State *L)
{
	int argc = lua_gettop(L);
	
	if (argc != 2) {
		lua_pushnil(L);
		return 1;
	}
	
	if (!lua_isstring(L, -2)) {
		lua_pushnil(L);
		return 1;
	}
	
	const char* stroke = lua_tostring(L, -2);
	struct prompt_input input = prompt_parse_stroke(stroke);
	
	if (input.key == 0) {
		lua_pushnil(L);
		return 1;
	}
	
	struct prompt_keybind* existing = prompt_find_binding(input.mods, input.key);
	
	if (existing != NULL) {
		/*
		 * Delete a existing binding to prevent a leak
		 */
		
		delete_bind(L, existing);
	}
	
	struct prompt_keybind* bind = NULL;
	
	if (lua_islightuserdata(L, -1)) {
		/*
		 * keybind.register(..., lightuserdata)
		 * means we've been called with a predefined action
		 * from the keybind.actions table.
		 * Instead of wrapping a Lua function, just call to the
		 * predefined action directly.
		 */
		
		struct prompt_predefined_action* predef = lua_touserdata(L, -1);
		
		bind = prompt_add_binding(input.mods, input.key, predef->action);
	}
	else {
		struct prompt_keybind* bind = prompt_add_binding_raw(4, input.mods, input.key, lua_keybind_handler);
		struct lua_keybind* luabind = sizeof(struct prompt_keybind) + (void*)bind;
		
		luabind->callback_ref = luaL_ref(L, LUA_REGISTRYINDEX);
	}
	
	/*
	 * Return the binding as lightuserdata, so it can be passed to
	 * keybind.delete to be removed
	 */
	
	lua_pushlightuserdata(L, bind);
	return 1;
}

static int
lua_keybind_delete(lua_State *L)
{
	int argc = lua_gettop(L);
	
	if (argc != 1) {
		lua_pushnil(L);
		return 1;
	}
	
	if (!lua_islightuserdata(L, -1)) {
		lua_pushnil(L);
		return 1;
	}
	
	struct prompt_keybind* bind = (struct prompt_keybind*)lua_touserdata(L, -1);
	
	delete_bind(L, bind);
	
	lua_pushboolean(L, 1);
	return 1;
}

static int
lua_keybind_find(lua_State *L)
{
	/*
	 * Find a binding by name so it can be deleted
	 */
	
	int argc = lua_gettop(L);
	
	if (argc != 1) {
		lua_pushnil(L);
		return 1;
	}
	
	if (!lua_isstring(L, -1)) {
		lua_pushnil(L);
		return 1;
	}
	
	const char* stroke = lua_tostring(L, -1);
	struct prompt_input input = prompt_parse_stroke(stroke);
	
	if (input.key == 0) {
		lua_pushnil(L);
		return 1;
	}
	
	struct prompt_keybind* bind = prompt_find_binding(input.mods, input.key);
	
	if (bind == NULL) {
		lua_pushnil(L);
		return 1;
	}
	
	lua_pushlightuserdata(L, bind);
	return 1;
}

static int
lua_keybind_list(lua_State *L)
{
	/*
	 * Get a list of all bound keys
	 */
	
	int argc = lua_gettop(L);
	
	if (argc != 0) {
		lua_pushnil(L);
		return 1;
	}
	
	lua_newtable(L);
	
	struct prompt_keybind* bind = prompt_first_binding();
	int i = 1;
	
	while (bind != NULL) {
		char buf[20] = { 0 };
		prompt_stroke_to_string(buf, sizeof(buf), bind->target);
		
		lua_pushstring(L, buf);
		lua_rawseti(L, -2, i++);
		
		bind = prompt_next_binding(bind);
	}
	
	return 1;
}

static const struct luaL_Reg keybindlib[] = {
	{"register", lua_keybind_register},
	{"delete", lua_keybind_delete},
	{"find", lua_keybind_find},
	{"list", lua_keybind_list},
	{NULL, NULL}
};

int
luaopen_keybind(lua_State *L)
{
	luaL_newlib(L, keybindlib);
	
	lua_newtable(L);
	
	/*
	 * Build keybind.actions out of the list of predefined actions
	 */
	 
	struct prompt_predefined_action** ppa;
	
	SET_FOREACH(ppa, Xpredef_action_set) {
		struct prompt_predefined_action* a = *ppa;
		
		lua_pushlightuserdata(L, a);
		lua_setfield(L, -2, a->name);
	}
	
	lua_setfield(L, -2, "actions");
	
	return 1;
}

static int
lua_history_add(lua_State *L)
{
	/*
	 * Artificially add a line the the prompt history
	 */
	
	int argc = lua_gettop(L);
	
	if (argc != 1) {
		lua_pushnil(L);
		return 1;
	}
	
	if (!lua_isstring(L, -1)) {
		lua_pushnil(L);
		return 1;
	}
	
	const char* entry = lua_tostring(L, -1);
	
	prompt_history_add(entry, strlen(entry));
	
	lua_pushboolean(L, 1);
	return 1;
}

static int
lua_history_remove(lua_State *L)
{
	/*
	 * Remove a line from the history, identified by index
	 */
	
	int argc = lua_gettop(L);
	
	if (argc != 1) {
		lua_pushnil(L);
		return 1;
	}
	
	int index = (int)lua_tonumber(L, -1);
	
	struct prompt_history_entry* entry = prompt_history_first();
	int i = 0;
	
	while (entry != NULL) {
		if (i++ == index) {
			prompt_history_remove(entry);
			
			lua_pushboolean(L, 1);
			return 1;
		}
		
		entry = prompt_history_next(entry);
	}
	
	lua_pushnil(L);
	return 1;
}

static int
lua_history_list(lua_State *L)
{
	/*
	 * Get a list of all lines in the history
	 */
	
	int argc = lua_gettop(L);
	
	if (argc != 0) {
		lua_pushnil(L);
		return 1;
	}
	
	lua_newtable(L);
	
	struct prompt_history_entry* entry = prompt_history_first();
	int i = 1;
	
	while (entry != NULL) {
		lua_pushstring(L, entry->line);
		lua_rawseti(L, -2, i++);
		
		entry = prompt_history_next(entry);
	}
	
	return 1;
}

static const struct luaL_Reg historylib[] = {
	{"add", lua_history_add},
	{"remove", lua_history_remove},
	{"list", lua_history_list},
	{NULL, NULL}
};

int
luaopen_history(lua_State *L)
{
	luaL_newlib(L, historylib);
	
	return 1;
}

/*
 * The libraries commented out below either lack the proper
 * support from libsa, or they are unlikely to be useful
 * in the bootloader, so have been commented out.
 */
static const luaL_Reg loadedlibs[] = {
  {"_G", luaopen_base},
  {LUA_LOADLIBNAME, luaopen_package},
//  {LUA_COLIBNAME, luaopen_coroutine},
//  {LUA_TABLIBNAME, luaopen_table},
  {LUA_STRLIBNAME, luaopen_string},
//  {LUA_IOLIBNAME, luaopen_io},
//  {LUA_OSLIBNAME, luaopen_os},
//  {LUA_MATHLIBNAME, luaopen_math},
//  {LUA_UTF8LIBNAME, luaopen_utf8},
//  {LUA_DBLIBNAME, luaopen_debug},
  {"errno", luaopen_errno},
  {"io", luaopen_io},
  {"lfs", luaopen_lfs},
  {"loader", luaopen_loader},
  {"pager", luaopen_pager},
  {"keybind", luaopen_keybind},
  {"history", luaopen_history},
  {NULL, NULL}
};

void
interp_init(void)
{
	lua_State *luap;
	struct interp_lua_softc	*softc = &lua_softc;
	const char *filename;
	const luaL_Reg *lib;

	TSENTER();

	setenv("script.lang", "lua", 1);
	LDBG("creating context");

	luap = lua_newstate(interp_lua_realloc, NULL);
	if (luap == NULL) {
		printf("problem initializing the Lua interpreter\n");
		abort();
	}
	softc->luap = luap;

	/* "require" functions from 'loadedlibs' and set results to global table */
	for (lib = loadedlibs; lib->func; lib++) {
		luaL_requiref(luap, lib->name, lib->func, 1);
		lua_pop(luap, 1);  /* remove lib */
	}

	filename = LOADER_LUA;
	if (interp_include(filename) != 0) {
		const char *errstr = lua_tostring(luap, -1);
		errstr = errstr == NULL ? "unknown" : errstr;
		printf("ERROR: %s.\n", errstr);
		lua_pop(luap, 1);
		setenv("autoboot_delay", "NO", 1);
	}

	TSEXIT();
}

int
interp_run(const char *line)
{
	int	argc, nargc;
	char	**argv;
	lua_State *luap;
	struct interp_lua_softc	*softc = &lua_softc;
	int status, ret;

	TSENTER();
	luap = softc->luap;
	LDBG("executing line...");
	if ((status = luaL_dostring(luap, line)) != 0) {
		lua_pop(luap, 1);
		/*
		 * The line wasn't executable as lua; run it through parse to
		 * to get consistent parsing of command line arguments, then
		 * run it through cli_execute. If that fails, then we'll try it
		 * as a builtin.
		 */
		command_errmsg = NULL;
		if (parse(&argc, &argv, line) == 0) {
			lua_getglobal(luap, "cli_execute");
			for (nargc = 0; nargc < argc; ++nargc) {
				lua_pushstring(luap, argv[nargc]);
			}
			status = lua_pcall(luap, argc, 1, 0);
			ret = lua_tointeger(luap, 1);
			lua_pop(luap, 1);
			if (status != 0 || ret != 0) {
				/*
				 * Lua cli_execute will pass the function back
				 * through loader.command, which is a proxy to
				 * interp_builtin_cmd. If we failed to interpret
				 * the command, though, then there's a chance
				 * that didn't happen. Call interp_builtin_cmd
				 * directly if our lua_pcall was not successful.
				 */
				status = interp_builtin_cmd(argc, argv);
			}
			if (status != 0) {
				if (command_errmsg != NULL)
					printf("%s\n", command_errmsg);
				else
					printf("Command failed\n");
				status = CMD_ERROR;
			}
			free(argv);
		} else {
			printf("Failed to parse \'%s\'\n", line);
			status = CMD_ERROR;
		}
	}

	TSEXIT();
	return (status == 0 ? CMD_OK : CMD_ERROR);
}

int
interp_include(const char *filename)
{
	struct interp_lua_softc	*softc = &lua_softc;

	LDBG("loading file %s", filename);

	return (luaL_dofile(softc->luap, filename));
}

static void dumpstack (lua_State *L) {
  int top=lua_gettop(L);
  for (int i=1; i <= top; i++) {
    printf("%d\t%s\t", i, luaL_typename(L,i));
    switch (lua_type(L, i)) {
      case LUA_TNUMBER:
        printf("%jd\n",(intmax_t)lua_tonumber(L,i));
        break;
      case LUA_TSTRING:
        printf("%s\n",lua_tostring(L,i));
        break;
      case LUA_TBOOLEAN:
        printf("%s\n", (lua_toboolean(L, i) ? "true" : "false"));
        break;
      case LUA_TNIL:
        printf("%s\n", "nil");
        break;
      default:
        printf("%p\n",lua_topointer(L,i));
        break;
    }
  }
}

static void* variable_first() {
	lua_State* L = lua_softc.luap;
	
	lua_pushglobaltable(L);
	lua_pushnil(L);
	return (void*)(long long int)lua_next(L, -2);
}
static void* variable_next(void* rawlast) {
	lua_State* L = lua_softc.luap;
	
	return (void*)(long long int)lua_next(L, -2);
}
static void variable_tostring(void* rawlast, char* out, int len) {
	lua_State* L = lua_softc.luap;
	
	int isfunction = lua_isfunction(L, -1);
	lua_pop(L, 1);
	
	if (lua_isstring(L, -1)) {
		lua_pushvalue(L, -1);
		const char* value = lua_tolstring(L, -1, NULL);
		lua_pop(L, 1);
		
		snprintf(out, len, isfunction ? "%s(" : "%s", value);
	}
}

static void lua_completer(char* command, char* argv) {
	lua_State* L = lua_softc.luap;
	
	int top = lua_gettop(L);
	prompt_generic_complete(argv, variable_first, variable_next, NULL, variable_tostring);
	lua_settop(L, top);
}

COMPLETION_SET(_, 0, lua_completer);