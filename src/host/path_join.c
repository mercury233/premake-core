/**
* \file   path_join.c
* \brief  Join two or more pieces of a file system path.
* \author Copyright (c) 2002-2013 Jess Perkins and the Premake project
*/

#include "premake.h"
#include <string.h>
#include "path_isabsolute.h"

#define DEFERRED_JOIN_DELIMITER '\a'

static char* path_join_single(char* buffer, char* ptr, char* end, const char* part, int allowDeferredJoin)
{
	int absoluteType;
	size_t len = strlen(part);
	/* remove leading "./" */
	while (strncmp(part, "./", 2) == 0) {
		part += 2;
		len -= 2;
	}

	/* remove trailing slashes */
	while (len > 1 && part[len - 1] == '/') {
		--len;
	}

	/* ignore empty segments and "." */
	if (len == 0 || (len == 1 && part[0] == '.')) {
		return ptr;
	}

	absoluteType = do_absolutetype(part);
	if (!allowDeferredJoin && absoluteType == JOIN_MAYBE_ABSOLUTE)
		absoluteType = JOIN_RELATIVE;

	/* if I encounter an absolute path, restart my result */
	switch (absoluteType) {
	case JOIN_ABSOLUTE:
		ptr = buffer;
		break;
	case JOIN_RELATIVE:
		/* if source has a .. prefix then take off last dest path part
		note that this doesn't guarantee a normalized result as this
		code doesn't check for .. in the mid path, however .. occurring
		mid path are much more likely to occur during path joins
		and its faster if we handle here as we don't have to remove
		substrings from the middle of the string. */

		while (ptr != buffer && len >= 2 && part[0] == '.' && part[1] == '.')
		{
			/* locate start of previous segment */
			char* start = strrchr(buffer, '/');
			if (!start) {
				start = buffer;
			}
			else {
				++start;
			}

			/* if I hit a segment I can't trim, bail out */
			if (strcmp(start, "..") == 0	/* parent dir */
				|| strcmp(start, ".") == 0	/* current dir */
				|| strstr(start, "**") != NULL	/* recursive wildcard */
				|| strchr(start, '$') != NULL)	/* property expansion */
			{
				break;
			}

			/* otherwise trim segment and the ".." sequence */
			if (start != buffer) {
				--start;
			}
			*start = '\0';
			ptr = start;
			part += 2;
			len -= 2;
			if (len > 0 && part[0] == '/') {
				++part;
				--len;
			}
		}

		/* if the path is already started, split parts */
		if (ptr != buffer && *(ptr - 1) != '/') {
			if (ptr == end - 1) {
				return NULL;
			}
			*(ptr++) = '/';
		}

		break;
	case JOIN_MAYBE_ABSOLUTE:
		if (ptr == end - 1) {
			return NULL;
		}
		*ptr = DEFERRED_JOIN_DELIMITER;
		ptr++;
		break;
	}

	/* append new part */
	if (len >= (size_t)(end - ptr)) {
		return NULL;
	}
	memcpy(ptr, part, len);
	ptr += len;
	*ptr = '\0';
	return ptr;
}

int path_join_internal(lua_State* L, int allowDeferredJoin)
{
	int i;
	const char* part;
	char buffer[PREMAKE_PATH_MAX] = { 0 };
	char* ptr = buffer;

	/* for each argument... */
	int argc = lua_gettop(L);
	for (i = 1; i <= argc; ++i) {
		/* if next argument is nil, skip it */
		if (lua_isnil(L, i)) {
			continue;
		}

		/* grab the next argument */
		part = luaL_checkstring(L, i);
		ptr = path_join_single(buffer, ptr, buffer + sizeof(buffer), part, allowDeferredJoin);
		if (!ptr) {
			return luaL_error(L, "path is too long");
		}
	}

	lua_pushstring(L, buffer);
	return 1;
}


int path_join(lua_State* L)
{
	return path_join_internal(L, 0);
}


int path_deferred_join(lua_State* L)
{
	return path_join_internal(L, 1);
}


static int do_path_has_deferred_join(const char* path)
{
	return (strchr(path, DEFERRED_JOIN_DELIMITER) != NULL);
}


int path_has_deferred_join(lua_State* L)
{
	const char* path = luaL_checkstring(L, -1);
	lua_pushboolean(L, do_path_has_deferred_join(path));
	return 1;
}

int path_resolve_deferred_join(lua_State* L)
{
	const char* path = luaL_checkstring(L, -1);
	char inBuffer[PREMAKE_PATH_MAX];
	char outBuffer[PREMAKE_PATH_MAX] = { 0 };
	char* ptr = outBuffer;
	char* nextPart;
	char* delimiter;
	size_t len = strlen(path);
	if (strchr(path, DEFERRED_JOIN_DELIMITER) == NULL)
	{
		lua_pushvalue(L, -1);
		return 1;
	}
	if (len >= sizeof(inBuffer)) {
		return luaL_error(L, "path is too long");
	}
	memcpy(inBuffer, path, len + 1);

	nextPart = inBuffer;
	do {
		delimiter = strchr(nextPart, DEFERRED_JOIN_DELIMITER);
		if (delimiter) {
			*delimiter = '\0';
		}
		ptr = path_join_single(outBuffer, ptr, outBuffer + sizeof(outBuffer), nextPart, 0);
		if (!ptr) {
			return luaL_error(L, "path is too long");
		}
		nextPart = delimiter ? delimiter + 1 : NULL;
	}
	while (nextPart);

	lua_pushstring(L, outBuffer);
	return 1;
}
