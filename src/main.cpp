// Floor Light Fix - SKSE plugin
// Copyright (C) 2026 izzydoingit
// GPL-3.0-or-later; see LICENSE.
//
// The game erases a light from the scene's active list in two places without taking it off the floors it lit; only the light's
// destructor does that, so while anything else still holds the light the floors keep drawing it. Before each of those erases this
// calls the game's own ClearGeometries, the call the same pass already makes for a hidden light.
#include "PCH.h"

#include "RuntimeGuard.h"
#include "SiteMath.h"

namespace
{
	using Lights = RE::BSTArray<RE::NiPointer<RE::BSLight>>;
	namespace Game = SiteMath::Game1170;

	REL::Relocation<void(RE::BSLight*)> gClearGeometries{};

	// both sites were checked to be the game's own call onto the same erase, so one thunk and one original serve both
	struct Erase
	{
		static void Thunk(Lights* a_array, std::uint32_t a_index, std::uint32_t a_count)
		{
			// both sites pass a count of 1 (read in the exe), and the site check pins that code
			if (a_array && a_index < a_array->size()) {
				if (auto* light = (*a_array)[a_index].get(); light && !light->geomList.empty()) {
					gClearGeometries(light);
				}
			}
			original(a_array, a_index, a_count);
		}
		static inline REL::Relocation<decltype(&Thunk)> original{};
	};

	void Install()
	{
		if (const auto game = REL::Module::get().version(); game != REL::Version{ 1, 6, 1170, 0 }) {
			SKSE::log::warn("not installed: written for Skyrim 1.6.1170 only, and this game is {}", game.string("."));
			return;
		}
		const auto                                   erase = REL::Offset(Game::kErase).address();
		const std::pair<std::uintptr_t, const char*> sites[]{ { REL::Offset(Game::kPass).address() + Game::kPassCall, "the per-light pass" },
			{ REL::Offset(Game::kDrain).address() + Game::kDrainCall, "the RemoveLight queue drain" } };
		for (const auto& [site, name] : sites) {
			std::array<std::uint8_t, 5> bytes{};
			std::memcpy(bytes.data(), reinterpret_cast<const void*>(site), bytes.size());
			if (SiteMath::CallTarget(bytes, site) != erase) {
				SKSE::log::warn("not installed: {} no longer holds the game's call onto the erase (another plugin's hook there?)", name);
				return;
			}
		}
		gClearGeometries = REL::Relocation<void(RE::BSLight*)>{ REL::Offset(Game::kClear).address() };
		auto& trampoline = SKSE::GetTrampoline();
		Erase::original = trampoline.write_call<5>(sites[0].first, Erase::Thunk);
		static_cast<void>(trampoline.write_call<5>(sites[1].first, Erase::Thunk));
		SKSE::log::info("installed at {} and {}", sites[0].second, sites[1].second);
	}
}

SKSEPluginLoad(const SKSE::LoadInterface* a_skse)
{
	SKSE::InitInfo info{};
	info.trampoline = true;
	info.trampolineSize = 64;
	SKSE::Init(a_skse, info);
	// the wrong game: loaded but idle (a false here adds SKSE's own "exit game?" box over the guard's)
	if (!RuntimeGuard::RightGame("Floor Light Fix", "Floor Light Fix", "")) {
		return true;
	}
	// a log line can throw on low memory, and nothing may leave the plugin into the game
	try {
		Install();
	} catch (...) {
		try {
			SKSE::log::error("the install stopped on an exception (low memory?): the fix may be in at one site, or none");
		} catch (...) {
		}
	}
	return true;
}
