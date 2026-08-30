/**
 * \file   path_getrelative.c
 * \brief  Returns a path relative to another.
 * \author Copyright (c) 2002-2013 Jess Perkins and the Premake project
 */

#include "premake.h"
#include <string.h>
#if PLATFORM_WINDOWS
#include <ctype.h>
#endif


int path_getrelative(lua_State* L)
{
	int i, last, count;
	size_t length;
	char src[PREMAKE_PATH_MAX];
	char dst[PREMAKE_PATH_MAX];

	const char* p1 = luaL_checkstring(L, 1);
	const char* p2 = luaL_checkstring(L, 2);

	/* normalize the paths */
	do_normalize(L, src, sizeof(src), p1);
	do_normalize(L, dst, sizeof(dst), p2);

	/* same directory? */
#if PLATFORM_WINDOWS
	if (_stricmp(src, dst) == 0) {
#else
	if (strcmp(src, dst) == 0) {
#endif
		lua_pushstring(L, ".");
		return 1;
	}

	/* dollar macro? Can't tell what the real path might be, so treat
	 * as absolute. This enables paths like $(SDK_ROOT)/include to
	 * work as expected. */
	if (dst[0] == '$') {
		lua_pushstring(L, dst);
		return 1;
	}

	/* find the common leading directories */
	if (strlen(src) > sizeof(src) - 2 || strlen(dst) > sizeof(dst) - 2) {
		return luaL_error(L, "path is too long");
	}
	strcat(src, "/");
	strcat(dst, "/");

	last = -1;
	i = 0;
#if PLATFORM_WINDOWS
	while (src[i] && dst[i] && tolower(src[i]) == tolower(dst[i])) {
#else
	while (src[i] && dst[i] && src[i] == dst[i]) {
#endif
		if (src[i] == '/') {
			last = i;
		}
		++i;
	}

	/* if I end up with just the root of the filesystem, either a single
	 * slash (/) or a drive letter (c:) then return the absolute path. */
	if (last <= 0 || (last == 2 && src[1] == ':')) {
		dst[strlen(dst) - 1] = '\0';
		lua_pushstring(L, dst);
		return 1;
	}

	/* Relative paths within a server can't climb outside the server root.
	* If the paths don't share server name, return the absolute path. */
	if (src[0] == '/' && src[1] == '/' && last == 1) {
		dst[strlen(dst) - 1] = '\0';
		lua_pushstring(L, dst);
		return 1;
	}

	/* count remaining levels in src */
	count = 0;
	for (i = last + 1; src[i] != '\0'; ++i) {
		if (src[i] == '/') {
			++count;
		}
	}

	/* start my result by backing out that many levels */
	length = strlen(dst + last + 1);
	if ((size_t)count > (sizeof(src) - length - 1) / 3) {
		return luaL_error(L, "relative path is too long");
	}
	src[0] = '\0';
	for (i = 0; i < count; ++i) {
		strcat(src, "../");
	}

	/* append what's left */
	strcat(src, dst + last + 1);

	/* remove trailing slash and done */
	length = strlen(src);
	if (length > 0 && src[length - 1] == '/') {
		src[length - 1] = '\0';
	}
	lua_pushstring(L, src);
	return 1;
}
