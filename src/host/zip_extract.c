/**
* \file   os_unzip.c
* \brief  Unzip file using libzip library.
* \author battle.net -- abrunasso.int@blizzard.com
*/

#include "premake.h"

#ifdef PREMAKE_COMPRESSION

#include "zip.h"

#ifdef WIN32
#include <direct.h>
#include <io.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <string.h>

// File Attribute for Unix
#define FA_IFMT   0170000  /* file type mask */
#define FA_IFIFO  0010000  /* named pipe (fifo) */
#define FA_IFCHR  0020000  /* character special */
#define FA_IFDIR  0040000  /* directory */
#define FA_IFBLK  0060000  /* block special */
#define FA_IFREG  0100000  /* regular */
#define FA_IFLNK  0120000  /* symbolic link */
#define FA_IFSOCK 0140000  /* socket */

#define FA_ISUID 0004000 /* set user id on execution */
#define FA_ISGID 0002000 /* set group id on execution */
#define FA_ISTXT 0001000 /* sticky bit */
#define FA_IRWXU 0000700 /* RWX mask for owner */
#define FA_IRUSR 0000400 /* R for owner */
#define FA_IWUSR 0000200 /* W for owner */
#define FA_IXUSR 0000100 /* X for owner */
#define FA_IRWXG 0000070 /* RWX mask for group */
#define FA_IRGRP 0000040 /* R for group */
#define FA_IWGRP 0000020 /* W for group */
#define FA_IXGRP 0000010 /* X for group */
#define FA_IRWXO 0000007 /* RWX mask for other */
#define FA_IROTH 0000004 /* R for other */
#define FA_IWOTH 0000002 /* W for other */
#define FA_IXOTH 0000001 /* X for other */
#define FA_ISVTX 0001000 /* save swapped text even after use */

/* Keep archive-controlled resource use bounded even when the ZIP metadata is
 * malicious. These can be overridden by embedders with stricter requirements. */
#ifndef PREMAKE_ZIP_EXTRACT_MAX_ENTRIES
#define PREMAKE_ZIP_EXTRACT_MAX_ENTRIES 100000
#endif

#ifndef PREMAKE_ZIP_EXTRACT_MAX_ENTRY_SIZE
#define PREMAKE_ZIP_EXTRACT_MAX_ENTRY_SIZE ((zip_uint64_t)1024 * 1024 * 1024)
#endif

#ifndef PREMAKE_ZIP_EXTRACT_MAX_TOTAL_SIZE
#define PREMAKE_ZIP_EXTRACT_MAX_TOTAL_SIZE ((zip_uint64_t)4 * 1024 * 1024 * 1024)
#endif

// ----------------------------------------------------------------------------

static int has_dos_file_attributes(zip_uint8_t opsys)
{
	return opsys == ZIP_OPSYS_DOS ||
		opsys == ZIP_OPSYS_OS_2 ||
		opsys == ZIP_OPSYS_WINDOWS_NTFS ||
		opsys == ZIP_OPSYS_VFAT;
}


static int has_unix_file_attributes(zip_uint8_t opsys)
{
	return opsys == ZIP_OPSYS_UNIX || opsys == ZIP_OPSYS_OS_X;
}


static int is_symlink(zip_uint8_t opsys, zip_uint32_t attrib)
{
	if (has_dos_file_attributes(opsys))
		return (attrib & 0x400) == 0x400;  // FILE_ATTRIBUTE_REPARSE_POINT

	if (has_unix_file_attributes(opsys))
		return ((attrib >> 16) & FA_IFMT) == FA_IFLNK;

	return 0;
}


static int is_directory(zip_uint8_t opsys, zip_uint32_t attrib)
{
	if (has_dos_file_attributes(opsys))
		return (attrib & 0x10) == 0x10;  // FILE_ATTRIBUTE_DIRECTORY

	if (has_unix_file_attributes(opsys))
		return ((attrib >> 16) & FA_IFMT) == FA_IFDIR;

	return 0;
}


extern int do_mkdir(lua_State *L, const char* path);

static int extract(lua_State *L, const char* src, const char* destination)
{
	int err = 0;
	int status = -1;
	int mkdir_result;
	int zip_file_close_result;
	FILE *fp = NULL;
	struct zip *z_archive = NULL;
	struct zip_file *zf = NULL;
	struct zip_stat st;
	zip_uint64_t i;
	zip_uint64_t entry_size;
	zip_uint64_t total_size = 0;
	zip_int64_t entries;
	char buffer[4096];
	char *normalized_name = NULL;
	char *output_path = NULL;
	char *cursor;
	char *component_start;
	char *last_separator;
	char *directory_end;
	const char *full_name;
	size_t name_length;
	size_t destination_length;
	size_t component_length;
	size_t output_offset;
	size_t written;
	zip_int64_t bytes_read;
	int entry_is_directory;

	if (destination[0] == '\0')
	{
		printf("ZIP extraction destination is empty\n");
		goto cleanup;
	}

	z_archive = zip_open(src, 0, &err);

	if (!z_archive)
	{
		printf("Unable to open ZIP archive %s (error %d)\n", src, err);
		goto cleanup;
	}

	entries = zip_get_num_entries(z_archive, 0);
	if (entries < 0)
	{
		printf("Unable to read ZIP entry count: %s\n", zip_strerror(z_archive));
		goto cleanup;
	}
	if (entries > PREMAKE_ZIP_EXTRACT_MAX_ENTRIES)
	{
		printf("ZIP archive contains too many entries\n");
		goto cleanup;
	}

	for (i = 0; i < (zip_uint64_t)entries; ++i)
	{
		zip_uint8_t opsys = 0;
		zip_uint32_t attrib = 0;

		full_name = zip_get_name(z_archive, i, 0);
		if (full_name == NULL)
		{
			printf("Unable to read ZIP entry name: %s\n", zip_strerror(z_archive));
			goto cleanup;
		}

		name_length = strlen(full_name);
		if (name_length == 0 || name_length > (size_t)-1 - 2)
		{
			printf("ZIP archive contains an empty or invalid entry name\n");
			goto cleanup;
		}

		normalized_name = (char *)malloc(name_length + 1);
		if (normalized_name == NULL)
		{
			printf("Out of memory while extracting ZIP archive\n");
			goto cleanup;
		}
		memcpy(normalized_name, full_name, name_length + 1);
		do_translate(normalized_name, '/');

		if (normalized_name[0] == '/' ||
			(name_length >= 2 &&
			 ((normalized_name[0] >= 'A' && normalized_name[0] <= 'Z') ||
			  (normalized_name[0] >= 'a' && normalized_name[0] <= 'z')) &&
			 normalized_name[1] == ':'))
		{
			printf("Refusing to extract unsafe ZIP entry: %s\n", full_name);
			goto cleanup;
		}

		component_start = normalized_name;
		for (cursor = normalized_name; ; ++cursor)
		{
			if (*cursor == '/' || *cursor == '\0')
			{
				component_length = (size_t)(cursor - component_start);
				if (component_length == 2 && component_start[0] == '.' && component_start[1] == '.')
				{
					printf("Refusing to extract unsafe ZIP entry: %s\n", full_name);
					goto cleanup;
				}
#if PLATFORM_WINDOWS
				if (!(component_length == 1 && component_start[0] == '.'))
				{
					const char *extension = (const char *)memchr(component_start, '.', component_length);
					size_t device_name_length = extension != NULL ? (size_t)(extension - component_start) : component_length;
					int reserved_device_name = 0;
					char device_name[7] = { 0, 0, 0, 0, 0, 0, 0 };
					size_t device_name_index;

					/* Win32 ignores ASCII spaces between a DOS device name and its extension. */
					while (device_name_length > 0 && component_start[device_name_length - 1] == ' ')
						--device_name_length;

					for (device_name_index = 0; device_name_index < sizeof(device_name) && device_name_index < device_name_length; ++device_name_index)
					{
						char ch = component_start[device_name_index];
						device_name[device_name_index] = (ch >= 'A' && ch <= 'Z') ? (char)(ch - 'A' + 'a') : ch;
					}

					if (device_name_length == 3)
					{
						reserved_device_name =
							(device_name[0] == 'c' && device_name[1] == 'o' && device_name[2] == 'n') ||
							(device_name[0] == 'p' && device_name[1] == 'r' && device_name[2] == 'n') ||
							(device_name[0] == 'a' && device_name[1] == 'u' && device_name[2] == 'x') ||
							(device_name[0] == 'n' && device_name[1] == 'u' && device_name[2] == 'l');
					}
					else if (device_name_length == 4)
					{
						reserved_device_name =
							(((device_name[0] == 'c' && device_name[1] == 'o' && device_name[2] == 'm') ||
							  (device_name[0] == 'l' && device_name[1] == 'p' && device_name[2] == 't')) &&
							 component_start[3] >= '1' && component_start[3] <= '9');
					}
					else if (device_name_length == 5)
					{
						reserved_device_name =
							(((device_name[0] == 'c' && device_name[1] == 'o' && device_name[2] == 'm') ||
							  (device_name[0] == 'l' && device_name[1] == 'p' && device_name[2] == 't')) &&
							 (unsigned char)component_start[3] == 0xc2 &&
							 ((unsigned char)component_start[4] == 0xb9 ||
							  (unsigned char)component_start[4] == 0xb2 ||
							  (unsigned char)component_start[4] == 0xb3));
					}
					else if (device_name_length == 6)
					{
						reserved_device_name = memcmp(device_name, "conin$", 6) == 0;
					}
					else if (device_name_length == 7)
					{
						reserved_device_name = memcmp(device_name, "conout$", 7) == 0;
					}

					if (memchr(component_start, ':', component_length) != NULL ||
						(component_length > 0 &&
						 (component_start[component_length - 1] == '.' || component_start[component_length - 1] == ' ')) ||
						reserved_device_name)
					{
						printf("Refusing to extract unsafe ZIP entry: %s\n", full_name);
						goto cleanup;
					}
				}
#endif
				if (*cursor == '\0')
					break;
				component_start = cursor + 1;
			}
		}

		if (zip_file_get_external_attributes(z_archive, i, 0, &opsys, &attrib) != 0)
		{
			printf("Unable to read attributes for ZIP entry %s: %s\n", full_name, zip_strerror(z_archive));
			goto cleanup;
		}
		if (is_symlink(opsys, attrib))
		{
			printf("Refusing to extract symbolic link ZIP entry: %s\n", full_name);
			goto cleanup;
		}

		zip_stat_init(&st);
		if (zip_stat_index(z_archive, i, 0, &st) != 0 || (st.valid & ZIP_STAT_SIZE) == 0)
		{
			printf("Unable to read size for ZIP entry %s: %s\n", full_name, zip_strerror(z_archive));
			goto cleanup;
		}
		if (st.size > PREMAKE_ZIP_EXTRACT_MAX_ENTRY_SIZE ||
			st.size > PREMAKE_ZIP_EXTRACT_MAX_TOTAL_SIZE - total_size)
		{
			printf("ZIP extraction size limit exceeded by entry: %s\n", full_name);
			goto cleanup;
		}

		destination_length = strlen(destination);
		if (destination_length > (size_t)-1 - name_length - 2)
		{
			printf("ZIP extraction path is too long\n");
			goto cleanup;
		}
		output_path = (char *)malloc(destination_length + name_length + 2);
		if (output_path == NULL)
		{
			printf("Out of memory while extracting ZIP archive\n");
			goto cleanup;
		}
		memcpy(output_path, destination, destination_length);
		output_offset = destination_length;
		if (output_offset > 0 && output_path[output_offset - 1] != '/' && output_path[output_offset - 1] != '\\')
			output_path[output_offset++] = '/';
		memcpy(output_path + output_offset, normalized_name, name_length + 1);
		do_translate(output_path, '/');

		entry_is_directory = is_directory(opsys, attrib) || normalized_name[name_length - 1] == '/';

		if (!do_mkdir(L, destination))
		{
			printf("Unable to create ZIP extraction destination: %s\n", destination);
			goto cleanup;
		}

		last_separator = strrchr(output_path, '/');
		directory_end = entry_is_directory ? output_path + output_offset + name_length : last_separator;
		if (entry_is_directory && directory_end[-1] == '/')
			--directory_end;
		if (directory_end != NULL && directory_end >= output_path + output_offset)
		{
			/* Create one archive-controlled component at a time so do_mkdir() never
			 * recurses through an attacker-controlled number of missing parents. */
			for (cursor = output_path + output_offset; cursor <= directory_end; ++cursor)
			{
				if (*cursor == '/' || cursor == directory_end)
				{
					char separator = *cursor;
					*cursor = '\0';
					mkdir_result = do_mkdir(L, output_path);
					*cursor = separator;
					if (!mkdir_result)
					{
						printf("Unable to create directory for ZIP entry: %s\n", output_path);
						goto cleanup;
					}
				}
			}
		}

		if (!entry_is_directory)
		{
			zf = zip_fopen_index(z_archive, i, 0);
			if (zf == NULL)
			{
				printf("Unable to open ZIP entry %s: %s\n", full_name, zip_strerror(z_archive));
				goto cleanup;
			}

			/* mark as read-write, so we can overwrite the file if it already exists. */
			chmod(output_path, 0666);

#if PLATFORM_WINDOWS
			{
				const wchar_t *wpath = luaL_convertlstring(L, output_path, strlen(output_path), NULL);
				if (!wpath)
				{
					printf("Unable to encode ZIP output path: %s\n", output_path);
					goto cleanup;
				}
				fp = _wfopen(wpath, L"wb");
				lua_pop(L, 1); /* pop converted wide path */
			}
#else
			fp = fopen(output_path, "wb");
#endif
			if (fp == NULL)
			{
				printf("Error creating file:\n  %s\n", output_path);
				goto cleanup;
			}

			entry_size = 0;
			for (;;)
			{
				bytes_read = zip_fread(zf, buffer, sizeof(buffer));
				if (bytes_read < 0)
				{
					printf("Error reading ZIP entry %s: %s\n", full_name, zip_file_strerror(zf));
					goto cleanup;
				}
				if (bytes_read == 0)
					break;
				if ((zip_uint64_t)bytes_read > PREMAKE_ZIP_EXTRACT_MAX_ENTRY_SIZE - entry_size ||
					(zip_uint64_t)bytes_read > PREMAKE_ZIP_EXTRACT_MAX_TOTAL_SIZE - total_size ||
					(zip_uint64_t)bytes_read > st.size - entry_size)
				{
					printf("ZIP extraction size limit exceeded by entry: %s\n", full_name);
					goto cleanup;
				}

				written = fwrite(buffer, sizeof(char), (size_t)bytes_read, fp);
				if (written != (size_t)bytes_read)
				{
					printf("Writing data to %s failed\n", output_path);
					goto cleanup;
				}

				entry_size += (zip_uint64_t)bytes_read;
				total_size += (zip_uint64_t)bytes_read;
			}
			if (entry_size != st.size)
			{
				printf("ZIP entry size does not match its metadata: %s\n", full_name);
				goto cleanup;
			}

			zip_file_close_result = fclose(fp);
			fp = NULL;
			if (zip_file_close_result != 0)
			{
				printf("Error closing extracted file: %s\n", output_path);
				goto cleanup;
			}

			/* mark read-only, but maintain the other properties. */
			if (opsys == ZIP_OPSYS_UNIX)
				chmod(output_path, (attrib >> 16) & ~0222);
			else
				chmod(output_path, 0444);
		}

		if (zf != NULL)
		{
			zip_file_close_result = zip_fclose(zf);
			zf = NULL;
			if (zip_file_close_result != 0)
			{
				printf("Error closing ZIP entry: %s\n", full_name);
				goto cleanup;
			}
		}
		free(output_path);
		output_path = NULL;
		free(normalized_name);
		normalized_name = NULL;
	}

	status = 0;

cleanup:
	if (fp != NULL)
		fclose(fp);
	if (zf != NULL)
		zip_fclose(zf);
	free(output_path);
	free(normalized_name);
	if (z_archive != NULL)
		zip_discard(z_archive);
	return status;
}


int zip_extract(lua_State* L)
{
	const char* src = luaL_checkstring(L, 1);
	const char* dst = luaL_checkstring(L, 2);

	int res = extract(L, src, dst);

	lua_pushnumber(L, (lua_Number)res);
	return 1;
}

#endif
