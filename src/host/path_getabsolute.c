/**
 * \file   path_getabsolute.c
 * \brief  Returns an absolute version of a relative path.
 * \author Copyright (c) 2002-2013 Jess Perkins and the Premake project
 */

#include "premake.h"
#include <string.h>


int do_getabsolute(char* result, size_t result_size, const char* value, const char* relative_to)
{
	size_t base_length = 0;
	size_t buffer_length;
	size_t result_length = 0;
	size_t value_length = strlen(value);
	size_t available;
	int has_drive_root;
	int needs_separator;
	char* ch;
	char* prev;
	const char* base = relative_to;
	char buffer[PREMAKE_PATH_MAX] = { '\0' };

	/* if the path is not already absolute, base it on working dir */
	if (!do_isabsolute(value)) {
		if (relative_to) {
			base_length = strlen(relative_to);
		}
		else {
			if (!do_getcwd(buffer, sizeof(buffer))) {
				return 0;
			}
			base = buffer;
			base_length = strlen(buffer);
		}

		if (base_length >= sizeof(buffer)) {
			return 0;
		}

		needs_separator = base_length == 0 ||
			(base[base_length - 1] != '/' && base[base_length - 1] != '\\');
		available = sizeof(buffer) - base_length - 1;
		if (needs_separator) {
			if (available == 0) {
				return 0;
			}
			--available;
		}
		if (value_length > available) {
			return 0;
		}

		if (base != buffer) {
			memcpy(buffer, base, base_length);
		}
		if (needs_separator) {
			buffer[base_length++] = '/';
		}
		memcpy(buffer + base_length, value, value_length + 1);
		buffer_length = base_length + value_length;
	}
	else {
		if (value_length >= sizeof(buffer)) {
			return 0;
		}
		memcpy(buffer, value, value_length + 1);
		buffer_length = value_length;
	}

	/* Each retained path segment temporarily adds a trailing slash. */
	if (result_size < 2 || buffer_length > result_size - 2) {
		return 0;
	}

	/* normalize the path separators */
	do_translate(buffer, '/');
	has_drive_root = buffer_length >= 3 && buffer[1] == ':' && buffer[2] == '/';

	/* process it part by part */
	result[0] = '\0';
	if (buffer[0] == '/') {
		result[result_length++] = '/';
		if (buffer[1] == '/') {
			result[result_length++] = '/';
		}
		result[result_length] = '\0';
	}

	prev = NULL;
	ch = strtok(buffer, "/");
	while (ch) {
		/* remove ".." where I can */
		if (strcmp(ch, "..") == 0 && (prev == NULL || (prev[0] != '$' && prev[0] != '%' && strcmp(prev, "..") != 0))) {
			int i = (int)result_length - 2;
			while (i >= 0 && result[i] != '/') {
				--i;
			}
			if (i >= 0) {
				result_length = (size_t)i + 1;
				result[result_length] = '\0';
			}
			ch = NULL;
		}

		/* allow everything except "." */
		else if (strcmp(ch, ".") != 0) {
			size_t segment_length = strlen(ch);
			memcpy(result + result_length, ch, segment_length);
			result_length += segment_length;
			result[result_length++] = '/';
			result[result_length] = '\0';
		}

		prev = ch;
		ch = strtok(NULL, "/");
	}

	/* remove trailing slash, except from filesystem roots */
	if (result_length > 0 && result[result_length - 1] == '/' &&
		!((result_length == 1 && result[0] == '/') ||
		  (result_length == 2 && result[0] == '/' && result[1] == '/') ||
		  (result_length == 3 && result[1] == ':' && has_drive_root)))
	{
		result[--result_length] = '\0';
	}

	return 1;
}


int path_getabsolute(lua_State* L)
{
	const char* relative_to;
	char buffer[PREMAKE_PATH_MAX];

	relative_to = NULL;
	if (lua_gettop(L) > 1 && !lua_isnil(L,2)) {
		relative_to = luaL_checkstring(L, 2);
	}

	if (lua_istable(L, 1)) {
		int i = 0;
		lua_newtable(L);
		lua_pushnil(L);
		while (lua_next(L, 1)) {
			const char* value = luaL_checkstring(L, -1);
			if (!do_getabsolute(buffer, sizeof(buffer), value, relative_to)) {
				return luaL_error(L, "path is too long");
			}
			lua_pop(L, 1);

			lua_pushstring(L, buffer);
			lua_rawseti(L, -3, ++i);
		}
		return 1;
	}
	else {
		const char* value = luaL_checkstring(L, 1);
		if (!do_getabsolute(buffer, sizeof(buffer), value, relative_to)) {
			return luaL_error(L, "path is too long");
		}
		lua_pushstring(L, buffer);
		return 1;
	}
}
