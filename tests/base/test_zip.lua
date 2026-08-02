--
-- tests/base/test_zip.lua
-- Tests the native zip API.
-- Copyright (c) 2026 Jess Perkins and the Premake project
--

if zip == nil or zip.extract == nil or zip.list == nil then
	return
end

local suite = test.declare("premake_zip")

-- Save the real function before the test runner installs its stub.
local real_io_open = io.open

local root
local archive
local destination

local function le16(value)
	return string.char(value % 256, math.floor(value / 256) % 256)
end

local function le32(value)
	return le16(value % 65536) .. le16(math.floor(value / 65536) % 65536)
end

local function writeZip(name, data, options)
	data = data or ""
	options = options or {}

	-- Construct the archive in memory so malformed/security-sensitive fields stay
	-- visible in source instead of being hidden in an opaque binary fixture.
	local method = options.method or 0
	local crc = options.crc or 0
	local compressedSize = options.compressedSize or #data
	local uncompressedSize = options.uncompressedSize or #data
	local versionMadeBy = options.versionMadeBy or 20
	local externalAttributes = options.externalAttributes or 0

	-- Local file header followed immediately by the entry payload. All integer
	-- fields in ZIP records are little-endian.
	local localHeader =
		le32(0x04034b50) ..       -- local file header signature
		le16(20) ..               -- minimum ZIP version needed to extract (2.0)
		le16(0) ..                -- general-purpose flags
		le16(method) ..           -- compression method: 0=stored, 8=deflate
		le16(0) .. le16(0) ..     -- DOS modification time and date
		le32(crc) ..              -- CRC-32 of the uncompressed payload
		le32(compressedSize) ..   -- payload bytes stored in this archive
		le32(uncompressedSize) .. -- expected bytes after decompression
		le16(#name) .. le16(0) .. -- file name length and extra-field length
		name .. data

	-- The central directory repeats the entry metadata and adds creator OS and
	-- external attributes; libzip uses those fields to recognize symbolic links.
	local centralDirectory =
		le32(0x02014b50) ..       -- central directory file header signature
		le16(versionMadeBy) ..    -- creator version; high byte identifies its OS
		le16(20) ..               -- minimum ZIP version needed to extract (2.0)
		le16(0) ..                -- general-purpose flags
		le16(method) ..           -- compression method
		le16(0) .. le16(0) ..     -- DOS modification time and date
		le32(crc) ..              -- CRC-32 of the uncompressed payload
		le32(compressedSize) ..   -- compressed size
		le32(uncompressedSize) .. -- uncompressed size
		le16(#name) ..            -- file name length
		le16(0) .. le16(0) ..     -- extra-field length and comment length
		le16(0) .. le16(0) ..     -- starting disk and internal file attributes
		le32(externalAttributes) .. -- Unix mode or DOS file attributes
		le32(0) ..                -- relative offset of the local file header
		name

	-- End-of-central-directory record for one entry, with no archive comment.
	local endOfCentralDirectory =
		le32(0x06054b50) ..       -- end-of-central-directory signature
		le16(0) .. le16(0) ..     -- this disk and central-directory start disk
		le16(1) .. le16(1) ..     -- entry count on this disk and in the archive
		le32(#centralDirectory) .. -- byte size of the central directory
		le32(#localHeader) ..      -- byte offset where the central directory starts
		le16(0)                    -- archive comment length

	local file = assert(real_io_open(archive, "wb"))
	file:write(localHeader, centralDirectory, endOfCentralDirectory)
	file:close()
end

local function readFile(filename)
	local file = assert(real_io_open(filename, "rb"))
	local contents = file:read("*a")
	file:close()
	return contents
end

function suite.setup()
	root = os.tmpname()
	if root:startswith("\\") then
		root = "." .. root
	end
	os.remove(root)
	assert(os.mkdir(root))

	archive = root .. "/input.zip"
	destination = root .. "/output"
end

function suite.teardown()
	if root ~= nil and os.isdir(root) then
		os.rmdir(root)
	end
	root = nil
end

function suite.extractsNestedFile()
	writeZip("nested/file.txt", "hello", { crc = 0x3610a686 })

	test.isequal(0, zip.extract(archive, destination))
	test.isequal("hello", readFile(destination .. "/nested/file.txt"))
end

function suite.extractsFileWithCurrentDirectoryComponents()
	writeZip("./nested/./file.txt", "hello", { crc = 0x3610a686 })

	test.isequal(0, zip.extract(archive, destination))
	test.isequal("hello", readFile(destination .. "/nested/file.txt"))
end

function suite.listsFirstEntryAtLuaIndexOne()
	writeZip("first.txt")

	local entries, err = zip.list(archive)
	test.isnil(err)
	test.isequal(1, #entries)
	test.isequal("first.txt", entries[1])
end

function suite.returnsEmptyListAndErrorWhenArchiveCannotBeOpened()
	local entries, err = zip.list(archive)

	test.isequal(0, #entries)
	test.isequal("string", type(err))
end

function suite.extractsDeeplyNestedFile()
	local name = string.rep("a/", 80) .. "file.txt"
	writeZip(name, "hello", { crc = 0x3610a686 })

	test.isequal(0, zip.extract(archive, destination))
	test.isequal("hello", readFile(destination .. "/" .. name))
end

function suite.followsExistingDestinationDirectoryLink()
	local linkTarget = root .. "/link-target"
	assert(os.mkdir(destination))
	assert(os.mkdir(linkTarget))
	test.istrue(os.linkdir(linkTarget, destination .. "/linked"))
	writeZip("linked/file.txt", "hello", { crc = 0x3610a686 })

	test.isequal(0, zip.extract(archive, destination))
	test.isequal("hello", readFile(linkTarget .. "/file.txt"))
end

function suite.rejectsParentTraversal()
	writeZip("../escaped.txt")

	test.isequal(-1, zip.extract(archive, destination))
	test.isfalse(os.isfile(root .. "/escaped.txt"))
end

function suite.rejectsBackslashParentTraversal()
	writeZip("..\\escaped.txt")

	test.isequal(-1, zip.extract(archive, destination))
	test.isfalse(os.isfile(root .. "/escaped.txt"))
end

function suite.rejectsWindowsParentTraversalWithTrailingSpace()
	if not os.ishost("windows") then
		return
	end

	writeZip(".. /escaped.txt")

	test.isequal(-1, zip.extract(archive, destination))
	test.isfalse(os.isfile(root .. "/escaped.txt"))
end

function suite.rejectsWindowsReservedDeviceNames()
	if not os.ishost("windows") then
		return
	end

	for _, name in ipairs({
		"NUL", "con.txt", "COM1", "lPt9.log", "COM\194\185", "CoNoUt$.txt",
		"CON .txt", "NUL .txt", "COM1 .txt", "LPT9 .log",
	}) do
		writeZip(name, "ignored")
		test.isequal(-1, zip.extract(archive, destination))
	end
end

function suite.rejectsAbsolutePath()
	writeZip("/escaped.txt")

	test.isequal(-1, zip.extract(archive, destination))
end

function suite.rejectsDrivePath()
	writeZip("C:/escaped.txt")

	test.isequal(-1, zip.extract(archive, destination))
end

function suite.rejectsTraversalAfterLongPath()
	writeZip(string.rep("segment/", 80) .. "../escaped.txt")

	test.isequal(-1, zip.extract(archive, destination))
	test.isfalse(os.isfile(root .. "/escaped.txt"))
end

function suite.rejectsOversizedEntry()
	writeZip("large.bin", "", { uncompressedSize = 1024 * 1024 * 1024 + 1 })

	test.isequal(-1, zip.extract(archive, destination))
	test.isfalse(os.isfile(destination .. "/large.bin"))
end

function suite.rejectsCorruptCompressedData()
	writeZip("corrupt.bin", "\0", { method = 8, uncompressedSize = 1 })

	test.isequal(-1, zip.extract(archive, destination))
end

function suite.rejectsSymbolicLinkEntry()
	-- Creator OS 3 means Unix; mode 0120777 marks this entry as a symlink.
	writeZip("link", "../outside", {
		versionMadeBy = 3 * 256 + 20,
		externalAttributes = 0xa1ff * 65536,
	})

	test.isequal(-1, zip.extract(archive, destination))
	test.isfalse(os.islink(destination .. "/link"))
end

function suite.rejectsMacOsSymbolicLinkEntry()
	writeZip("macos-link", "../outside", {
		versionMadeBy = 19 * 256 + 20,
		externalAttributes = 0xa1ff * 65536,
	})

	test.isequal(-1, zip.extract(archive, destination))
	test.isfalse(os.islink(destination .. "/macos-link"))
end

function suite.rejectsWindowsReparsePointEntry()
	-- DOS external attribute 0x400 is FILE_ATTRIBUTE_REPARSE_POINT.
	writeZip("reparse-point", "ignored", { externalAttributes = 0x400 })

	test.isequal(-1, zip.extract(archive, destination))
	test.isfalse(os.isfile(destination .. "/reparse-point"))
end

function suite.rejectsWindowsNtfsReparsePointEntry()
	writeZip("ntfs-reparse-point", "ignored", {
		versionMadeBy = 10 * 256 + 20,
		externalAttributes = 0x400,
	})

	test.isequal(-1, zip.extract(archive, destination))
	test.isfalse(os.isfile(destination .. "/ntfs-reparse-point"))
end
