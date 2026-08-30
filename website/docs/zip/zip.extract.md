```lua
zip.extract(sourceZip, destinationDir)
```

### Parameters ###
- `sourceZip` is the zip file which has to be extracted
- `destinationDir` is the destination directory for the extracted files

### Return Value ###

Returns `0` when the archive is extracted successfully, or `-1` when extraction fails. Extraction is not transactional: files written before an error are not rolled back.

### Security ###

Archive entry names are checked before extraction. Absolute paths, parent-directory components, Windows drive paths, reserved device names and other platform-specific escape forms are rejected.

Entries marked as symbolic links or Windows reparse points are rejected. Existing symbolic links, junctions or reparse points already present below `destinationDir` are still followed by the operating system, so callers that require strict containment must use a trusted destination tree.

By default, extraction is limited to 100,000 entries, 1 GiB per entry and 4 GiB in total. Embedders can override these limits at compile time.

### Availability ###

Premake 5.0.0 alpha 12 or later.
