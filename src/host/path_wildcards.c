/**
 * \file   path_wildcards.c
 * \brief  Converts from a simple wildcard syntax to the corresponding Lua pattern.
 * \author Copyright (c) 2015 Tom van Dijck, Jess Perkins and the Premake project
 */

#include "premake.h"
#include <string.h>

/*
--Converts from a simple wildcard syntax, where * is "match any"
-- and ** is "match recursive", to the corresponding Lua pattern.
--
-- @param pattern
--    The wildcard pattern to convert.
-- @returns
--    The corresponding Lua pattern.
*/
int path_wildcards(lua_State* L)
{
	size_t length, i;
	const char* input;
	char buffer[PREMAKE_PATH_MAX];
	char* output;

	input = luaL_checklstring(L, 1, &length);
	output = buffer;

	for (i = 0; i < length; ++i)
	{
		char c = input[i];
		size_t required = 1;
		if (strchr("+.-^$()%", c)) {
			required = 2;
		}
		else if (c == '*') {
			required = ((i + 1) < length && input[i + 1] == '*') ? 2 : 5;
		}

		if (required >= (size_t)(buffer + sizeof(buffer) - output)) {
			return luaL_error(L, "wildcards expansion is too long");
		}

		switch (c)
		{
		case '+':
		case '.':
		case '-':
		case '^':
		case '$':
		case '(':
		case ')':
		case '%':
			*(output++) = '%';
			*(output++) = c;
			break;

		case '*':
			if ((i + 1) < length && input[i + 1] == '*')
			{
				i++; // skip the next character.
				*(output++) = '.';
				*(output++) = '*';
			}
			else
			{
				*(output++) = '[';
				*(output++) = '^';
				*(output++) = '/';
				*(output++) = ']';
				*(output++) = '*';
			}
			break;

		default:
			*(output++) = c;
			break;
		}

	}

	*output = '\0';

	lua_pushstring(L, buffer);
	return 1;
}
