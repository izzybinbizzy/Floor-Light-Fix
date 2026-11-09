-- Floor Light Fix - SKSE plugin. GPL-3.0-or-later, see LICENSE.
-- Two call-site hooks: before the scene erases a light from its active lists, the game's own ClearGeometries runs on it.
set_xmakever("3.0.0")
set_project("FloorLightFix")
set_version("1.0.0")
set_license("GPL-3.0-or-later")
set_arch("x64")
set_languages("c++23")
-- the DLL carries its own Visual C++ runtime, so an older runtime on a player's PC cannot stop it loading
set_runtimes("MT")
add_rules("mode.releasedbg")
set_defaultmode("releasedbg")

-- the call sites were read in the 1.6.1170 exe only: on any other version the DLL loads and does nothing; no VR build until VR is read
set_config("skyrim_se", true)
set_config("skyrim_ae", true)
set_config("skyrim_vr", false)

-- REPRODUCIBLE BUILDS: the same source gives the same DLL, byte for byte, in whatever folder it is built. MSVC only.
add_cxflags("/Brepro", "/experimental:deterministic", "/pathmap:" .. os.projectdir() .. "=.", { force = true, tools = "cl" })
add_shflags("/Brepro", "/PDBALTPATH:%_PDB%", { force = true, tools = "link" })

includes("lib/commonlibsse-ng")

target("FloorLightFix", function()
    add_deps("commonlibsse-ng")
    add_defines("NOMINMAX")  -- windows.h min/max macros break std::min/max and numeric_limits::max
    add_rules("commonlibsse-ng.plugin", {
        name = "FloorLightFix",
        author = "izzydoingit",
        description = "Floor Light Fix - a light that leaves the scene is taken off the floors it lit",
    })
    add_files("src/*.cpp")
    add_headerfiles("src/*.h")
    add_includedirs("src")
    set_pcxxheader("src/PCH.h")
    -- every warning in OUR files is an error
    set_warnings("allextra", "error")
end)

-- the game-free part (src/SiteMath.h), tested without the game: xmake build testfloor && xmake run testfloor
-- (PC Runner\plugin build.py builds and runs it, then again under clang's address + undefined-behavior checkers)
target("testfloor", function()
    set_kind("binary")
    set_default(false)
    add_files("tests/testfloor.cpp")
    add_includedirs("src")
    set_warnings("allextra", "error")
end)
