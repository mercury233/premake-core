Get list of file paths contained in an archive.

```lua
local entries, err = zip.list(sourceZip)
```

### Parameters ###
- `sourceZip` is the zip file which has to be extracted

### Return Value ###

A 1-based sequence containing the paths in archive order, followed by an error string. On success, index `1` contains the first archive entry and the error value is `nil`.

### Availability ###

Premake 5.0.0 or later.
