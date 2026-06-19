--
-- tests/base/test_config_getdefault.lua
-- Unit tests for the config.getdefault() function.
-- Tests the priority order and fallback behavior for default configurations.
--

	local p = premake
	local suite = test.declare("config_getdefault")


--
-- Setup/teardown
--

	local wks, prj

	local function assertDefault(cfg, buildcfg, platform)
		test.isequal(buildcfg, cfg.buildcfg)
		test.isequal(platform, cfg.platform)
	end

	function suite.setup()
		wks = workspace("MyWorkspace")
		configurations { "Release", "Debug", "Profile" }
		prj = project("MyProject")
		kind "ConsoleApp"
	end


--
-- Verify project default behavior: returns first project configuration in
-- alphabetic order.
--

	function suite.returnsFirstAlphabetically_onProjectNoDefaults()
		prj = test.getproject(wks, 1)
		local cfg = p.config.getdefault(prj)
		assertDefault(cfg, "Debug", nil)
	end


--
-- Verify matching defaultConfiguration only
--

	function suite.matchesDefaultConfiguration_whenSpecified()
		defaultConfiguration "Release"
		prj = test.getproject(wks, 1)
		local cfg = p.config.getdefault(prj)
		assertDefault(cfg, "Release", nil)
	end


--
-- Verify matching defaultplatform only
--

	function suite.matchesDefaultPlatform_whenSpecified()
		platforms { "x86", "x64" }
		defaultplatform "x64"
		prj = test.getproject(wks, 1)
		local cfg = p.config.getdefault(prj)
		assertDefault(cfg, "Debug", "x64")
	end


--
-- Verify matching both defaultConfiguration and defaultplatform
-- Priority: both match > configuration match > platform match > first

	function suite.prefersBothMatching_overPartial()
		platforms { "x86", "x64" }
		defaultConfiguration "Profile"
		defaultplatform "x64"
		prj = test.getproject(wks, 1)
		local cfg = p.config.getdefault(prj)
		assertDefault(cfg, "Profile", "x64")
	end


--
-- Verify that invalid defaultConfiguration falls back to first
--

	function suite.fallsBackToFirst_onInvalidConfiguration()
		defaultConfiguration "NonExistent"
		prj = test.getproject(wks, 1)
		local cfg = p.config.getdefault(prj)
		assertDefault(cfg, "Debug", nil)
	end


--
-- Verify that invalid defaultplatform falls back to first
--

	function suite.fallsBackToFirst_onInvalidPlatform()
		platforms { "x86", "x64" }
		defaultplatform "ARM"
		prj = test.getproject(wks, 1)
		local cfg = p.config.getdefault(prj)
		assertDefault(cfg, "Debug", "x64")
	end


--
-- Verify case-insensitivity of defaultConfiguration matching
--

	function suite.caseInsensitive_forConfiguration()
		defaultConfiguration "RELEASE"
		prj = test.getproject(wks, 1)
		local cfg = p.config.getdefault(prj)
		assertDefault(cfg, "Release", nil)
	end


--
-- Verify case-insensitivity of defaultplatform matching
--

	function suite.caseInsensitive_forPlatform()
		platforms { "x86", "x64" }
		defaultplatform "X64"
		prj = test.getproject(wks, 1)
		local cfg = p.config.getdefault(prj)
		assertDefault(cfg, "Debug", "x64")
	end


--
-- Verify priority: when both defaultConfiguration is invalid but defaultplatform is valid,
-- return the defaultplatform match
--

	function suite.prefersValidPlatform_whenConfigInvalid()
		platforms { "x86", "x64" }
		defaultConfiguration "NonExistent"
		defaultplatform "x64"
		prj = test.getproject(wks, 1)
		local cfg = p.config.getdefault(prj)
		assertDefault(cfg, "Debug", "x64")
	end


--
-- Verify priority: when defaultConfiguration is valid but defaultplatform is invalid,
-- return the defaultConfiguration match
--

	function suite.prefersValidConfiguration_whenPlatformInvalid()
		platforms { "x86", "x64" }
		defaultConfiguration "Release"
		defaultplatform "ARM"
		prj = test.getproject(wks, 1)
		local cfg = p.config.getdefault(prj)
		assertDefault(cfg, "Release", "x64")
	end


--
-- Verify with many configurations that the correct one is chosen
--

	function suite.selectsCorrectConfig_withMany()
		configurations { "Debug", "Release", "Profile", "MinSize", "CustomDebug" }
		platforms { "x86", "x64", "ARM" }
		defaultConfiguration "Profile"
		defaultplatform "ARM"
		prj = test.getproject(wks, 1)
		local cfg = p.config.getdefault(prj)
		assertDefault(cfg, "Profile", "ARM")
	end


--
-- Verify workspace default behavior: returns first workspace configuration in
-- alphabetic order.
--

	function suite.returnsFirstAlphabetically_onWorkspaceNoDefaults()
		local wks2 = workspace("WorkspaceForTest")
		configurations { "Release", "Debug", "Profile" }
		wks2 = test.getWorkspace(wks2)
		local cfg = p.config.getdefault(wks2)
		assertDefault(cfg, "Debug", nil)
	end


--
-- Verify workspace level defaultConfiguration.
--

	function suite.matchesDefaultConfiguration_onWorkspace()
		local wks2 = workspace("WorkspaceForDefaultConfigurationTest")
		configurations { "Release", "Debug", "Profile" }
		defaultConfiguration "Release"
		wks2 = test.getWorkspace(wks2)
		local cfg = p.config.getdefault(wks2)
		assertDefault(cfg, "Release", nil)
	end
