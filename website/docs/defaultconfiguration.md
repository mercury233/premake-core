Specifies the default build configuration for a workspace.

```lua
defaultconfiguration ("configuration_name")
```

If `configuration_name` has not been defined using [`configurations`](configurations.md), the default configuration will not change from the first one when sorted alphabetically by configuration name.

### Parameters ###

`configuration_name` - The name of the build configuration to use as default.

### Applies To ###

Workspace configurations.

### Availability ###

Premake 5.0.0 or later.

### Examples ###

```lua
workspace "MyWorkspace"
  configurations { "Debug", "Release" }
  defaultconfiguration "Release"
```

When combined with [`defaultplatform`](defaultplatform.md), Premake will prefer the configuration/platform pair that matches both settings.

```lua
workspace "MyWorkspace"
  configurations { "Debug", "Release" }
  platforms { "x86", "x64" }
  defaultconfiguration "Release"
  defaultplatform "x64"
```

### See Also ###

* [configurations](configurations.md)
* [defaultplatform](defaultplatform.md)
