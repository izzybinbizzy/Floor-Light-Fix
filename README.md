# Floor Light Fix - SKSE plugin

Copyright (C) 2026 izzydoingit. GPL-3.0-or-later, see `LICENSE`.

A small engine fix for Skyrim: no plugin file, no scripts, no settings, nothing written into the save.

## The bug
When a light leaves the scene (a torch put away, a spell's hand light gone, a character removed), the game takes it out of
the scene's list of lights but leaves it on the light lists of the surfaces it lit; only the light's own destructor clears
those. So while anything still holds the light - the game itself, or a mod that keeps a reference to it - the floors and
walls it touched keep drawing it: a glow left on the ground where nothing is. It shows on the game's own lighting and on ENB;
Community Shaders draws lights another way and never shows it.

## The fix
Two call hooks on the game's own erase calls where a light leaves the scene's lists (the per-light pass and the drain of
the RemoveLight queue). Before the erase, the plugin calls the game's own `BSLight::ClearGeometries` on that light - the
call the same pass already makes for a hidden light. Each site is checked before anything is written there: anything
unexpected leaves the game untouched and says why in `FloorLightFix.log`. Nothing runs while no light leaves the scene.

## Requires
SKSE and Address Library for SKSE Plugins. Skyrim 1.6.1170 only: on any other version (SE 1.5.97 or another 1.6 build) it
loads, installs nothing and says why in its log, and there is no VR build. The addresses for other versions have not been
read.

## Building
`build.bat` (git, xmake 3 and the Visual Studio C++ tools): CommonLib at the pinned commit, the DLL, then the off-game
tests (`tests/testfloor.cpp`, the game-free reading of a call site in `src/SiteMath.h`).
