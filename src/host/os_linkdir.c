/**
 * \file os_linkdir.c
 * \brief Creates a symbolic link to a directory.
 * \author Copyright (c) 2024 Jess Perkins and the Premake project
 */

#include <sys/stat.h>
#include "premake.h"

int do_linkdir(lua_State* L, const char* src, const char* dst)
{
#if PLATFORM_WINDOWS
    // Prepend the drive letter if a relative path is given
    char dstPath[MAX_PATH];
	char srcPath[MAX_PATH];
	const wchar_t *wSrcPath, *wDstPath;
	BOOLEAN res;

	do_normalize(L, srcPath, sizeof(srcPath), src);
	do_normalize(L, dstPath, sizeof(dstPath), dst);
	do_translate(dstPath, '\\');
	do_translate(srcPath, '\\');

	// Promote to wide path
	wSrcPath = luaL_convertlstring(L, srcPath, strlen(srcPath), NULL);
	if (!wSrcPath) return FALSE;
	wDstPath = luaL_convertlstring(L, dstPath, strlen(dstPath), NULL);
	if (!wDstPath)
	{
		lua_pop(L, 1);
		return FALSE;
	}

	// If the source path is relative, prepend the current working directory
	if (!do_isabsolute(src))
	{
		// Get the current working directory
		wchar_t cwd[MAX_PATH + 1];
		DWORD cwdLength = GetCurrentDirectoryW(MAX_PATH + 1, cwd);
		if (cwdLength == 0 || cwdLength > MAX_PATH)
		{
			lua_pop(L, 2); /* remove converted strings */
			return FALSE;
		}

		// Convert the source path to a relative path
		wchar_t relSrcPath[2 * MAX_PATH + 1];
		int length = swprintf(relSrcPath, 2 * MAX_PATH + 1, L"%ls\\%ls", cwd, wSrcPath);
		if (length < 0 || length >= 2 * MAX_PATH + 1)
		{
			lua_pop(L, 2); /* remove converted strings */
			return FALSE;
		}

		res = CreateSymbolicLinkW(wDstPath, relSrcPath, SYMBOLIC_LINK_FLAG_DIRECTORY | SYMBOLIC_LINK_FLAG_ALLOW_UNPRIVILEGED_CREATE);
	}
	else
	{
		res = CreateSymbolicLinkW(wDstPath, wSrcPath, SYMBOLIC_LINK_FLAG_DIRECTORY | SYMBOLIC_LINK_FLAG_ALLOW_UNPRIVILEGED_CREATE);
	}
	lua_pop(L, 2);
	return res != 0;
#else
	(void)L;
	if (!do_isabsolute(src))
	{
		char cwd[PATH_MAX];
		if (!do_getcwd(cwd, PATH_MAX))
		{
			return FALSE;
		}

		char relSrcPath[2 * PATH_MAX + 1];
		int length = snprintf(relSrcPath, sizeof(relSrcPath), "%s/%s", cwd, src);
		if (length < 0 || (size_t)length >= sizeof(relSrcPath))
		{
			return FALSE;
		}

		int res = symlink(relSrcPath, dst);
		return res == 0;
	}
	else
	{
		int res = symlink(src, dst);
    	return res == 0;
	}
#endif
}

int os_linkdir(lua_State* L)
{
    const char* src = luaL_checkstring(L, 1);
    const char* dst = luaL_checkstring(L, 2);

    int result = do_linkdir(L, src, dst);
    if (!result)
    {
		lua_pushnil(L);
		lua_pushfstring(L, "Unable to create link from '%s' to '%s'", src, dst);
        return 2;
    }

    lua_pushboolean(L, 1);
    return 1;
}
