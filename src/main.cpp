// Floor Light Fix - SKSE plugin
// Copyright (C) 2026 izzydoingit
// GPL-3.0-or-later; see LICENSE.
//
// The game takes a light out of the scene's active lists in two places without taking it off the floors it lit: the per-light
// pass, when nothing but the light object holds its NiLight any more (AE 106335 +0x732), and the drain of RemoveLight's queue
// (AE 106333 +0x134). Only the light object's destructor clears those floors, so while anything else still holds the light the
// floors keep drawing it. Before each of those two erases this calls the game's own ClearGeometries (AE 108285), the call the same
// pass already makes for a hidden light. Nothing of this runs while no light is leaving the scene.
#include "PCH.h"

#include "RuntimeGuard.h"
#include "SiteMath.h"

namespace
{
	using Lights = RE::BSTArray<RE::NiPointer<RE::BSLight>>;
	using ClearFn = void(RE::BSLight*);

	namespace Game = SiteMath::Game1170;

	// the only game these were read in; on any other nothing is looked up or written
	constexpr REL::Version kReadIn{ 1, 6, 1170, 0 };

	std::atomic<std::uint64_t> gCleared{ 0 };
	REL::Relocation<ClearFn>   gClearGeometries{};

	// nothing here can throw: a bounds-checked read, the game's own call and an atomic add
	void ClearFirst(Lights* a_array, std::uint32_t a_index, std::uint32_t a_count) noexcept
	{
		if (!a_array) {
			return;
		}
		// both sites erase one light (r8d 1, read in the exe); a longer erase is cleared whole all the same
		const auto [first, last] = SiteMath::Erased(a_index, a_count, a_array->size());
		for (auto i = first; i < last; ++i) {
			auto* light = (*a_array)[i].get();
			if (light && !light->geomList.empty()) {
				gClearGeometries(light);
				gCleared.fetch_add(1, std::memory_order_relaxed);
			}
		}
	}

	template <int N>
	struct Erase
	{
		static void Thunk(Lights* a_array, std::uint32_t a_index, std::uint32_t a_count)
		{
			ClearFirst(a_array, a_index, a_count);
			original(a_array, a_index, a_count);
		}
		static inline REL::Relocation<decltype(&Thunk)> original{};
	};

	// the game's whole image, from its own PE header: the site's call is judged against it
	std::pair<std::uintptr_t, std::uintptr_t> GameImage()
	{
		const auto  base = REL::Module::get().base();
		const auto* dos = reinterpret_cast<const REX::W32::IMAGE_DOS_HEADER*>(base);
		const auto* nt = reinterpret_cast<const REX::W32::IMAGE_NT_HEADERS64*>(base + static_cast<std::uintptr_t>(dos->lfanew));
		return { base, base + nt->optionalHeader.imageSize };
	}

	// the site must still be a 5-byte relative call: onto the game's erase, or out of the game's image, which is kept as the next call
	const char* CheckSite(std::uintptr_t a_site, std::uintptr_t a_erase, std::pair<std::uintptr_t, std::uintptr_t> a_image)
	{
		std::array<std::uint8_t, 5> bytes{};
		std::memcpy(bytes.data(), reinterpret_cast<const void*>(a_site), bytes.size());
		switch (SiteMath::Judge(SiteMath::CallTarget(bytes, a_site), a_erase, a_image.first, a_image.second)) {
		case SiteMath::Site::kGame:
			return "the game's erase";
		case SiteMath::Site::kChained:
			return "a call out of the game's image, most likely another plugin's hook of the same call (chained)";
		default:
			return nullptr;
		}
	}

	// a_written counts each hook the moment it is in, so a throw at any point is reported as what it left behind
	void Install(int& a_written)
	{
		const auto game = REL::Module::get().version();
		if (game != kReadIn) {
			SKSE::log::warn("not installed: the call sites were read in Skyrim {} only, and this game is {}", kReadIn.string("."),
				game.string("."));
			return;
		}
		const auto                                   erase = REL::Offset(Game::kErase).address();
		const std::pair<const char*, std::uintptr_t> sites[] = { { "the per-light pass", REL::Offset(Game::kPass).address() + Game::kPassCall },
			{ "the RemoveLight queue drain", REL::Offset(Game::kDrain).address() + Game::kDrainCall } };
		const auto                                   image = GameImage();
		const char*                                  what[2]{};
		for (int i = 0; i < 2; ++i) {
			what[i] = CheckSite(sites[i].second, erase, image);
			if (!what[i]) {
				SKSE::log::warn("not installed: {} does not hold the call this was written for", sites[i].first);
				return;
			}
		}
		gClearGeometries = REL::Relocation<ClearFn>{ REL::Offset(Game::kClear).address() };
		auto& trampoline = SKSE::GetTrampoline();
		Erase<0>::original = trampoline.write_call<5>(sites[0].second, Erase<0>::Thunk);
		++a_written;
		Erase<1>::original = trampoline.write_call<5>(sites[1].second, Erase<1>::Thunk);
		++a_written;
		SKSE::log::info("installed at {} (onto {}) and {} (onto {})", sites[0].first, what[0], sites[1].first, what[1]);
	}

	void OnMessage(SKSE::MessagingInterface::Message* a_msg)
	{
		if (!a_msg || a_msg->type != SKSE::MessagingInterface::kSaveGame) {
			return;
		}
		// a log line can throw (low memory): it never leaves into the game
		try {
			SKSE::log::info("lights taken off the floors as they left the scene so far: {}", gCleared.load(std::memory_order_relaxed));
		} catch (...) {
		}
	}
}

SKSEPluginLoad(const SKSE::LoadInterface* a_skse)
{
	SKSE::InitInfo info{};
	// two call hooks, 14 bytes each
	info.trampoline = true;
	info.trampolineSize = 64;
	SKSE::Init(a_skse, info);
	// the wrong game: loaded but idle - a false here makes SKSE add its own "exit game?" box over the guard's
	if (!RuntimeGuard::RightGame("Floor Light Fix", "Floor Light Fix", "")) {
		return true;
	}
	// the install's log lines are the only work here that can throw (low memory): none leaves the plugin
	int written = 0;
	try {
		Install(written);
	} catch (...) {
		if (written < 2) {
			try {
				SKSE::log::error("the install stopped on an exception (low memory?) with {} of its 2 hooks written", written);
			} catch (...) {
			}
		}
	}
	// the count on each save only where the fix runs, so a log of a game it is not in never reads as if it were
	if (auto* messaging = SKSE::GetMessagingInterface(); written > 0 && messaging) {
		messaging->RegisterListener(OnMessage);
	}
	return true;
}
