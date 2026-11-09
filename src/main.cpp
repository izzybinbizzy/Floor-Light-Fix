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

	// the only game the call sites were read in: on any other the Address Library ids are never looked up
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
		const std::uint64_t end = std::min<std::uint64_t>(std::uint64_t{ a_index } + a_count, a_array->size());
		for (std::uint64_t i = a_index; i < end; ++i) {
			auto* light = (*a_array)[static_cast<std::uint32_t>(i)].get();
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

	// the game's own module, from its section table: a call that leaves it is another plugin's hook
	std::pair<std::uintptr_t, std::uintptr_t> GameImage()
	{
		const auto&    module = REL::Module::get();
		std::uintptr_t end = module.base();
		for (std::size_t i = 0; i < REL::Segment::total; ++i) {
			const auto segment = module.segment(static_cast<REL::Segment::Name>(i));
			end = std::max(end, segment.address() + segment.size());
		}
		return { module.base(), end };
	}

	// the site must still be a 5-byte relative call: onto the game's erase, or onto another plugin's hook of the same call (a chain)
	const char* CheckSite(std::uintptr_t a_site, std::uintptr_t a_erase)
	{
		std::array<std::uint8_t, 5> bytes{};
		std::memcpy(bytes.data(), reinterpret_cast<const void*>(a_site), bytes.size());
		const auto [start, end] = GameImage();
		switch (SiteMath::Judge(SiteMath::CallTarget(bytes, a_site), a_erase, start, end)) {
		case SiteMath::Site::kGame:
			return "the game's erase";
		case SiteMath::Site::kChained:
			return "another plugin's hook of the same call (chained)";
		default:
			return nullptr;
		}
	}

	void Install()
	{
		const auto game = REL::Module::get().version();
		if (game != kReadIn) {
			SKSE::log::warn("not installed: the call sites were read in Skyrim {} only, and this game is {}", kReadIn.string("."),
				game.string("."));
			return;
		}
		// every id is looked up before anything is judged or written (1.6.1170's Address Library holds all four)
		const auto                                   erase = REL::ID(106419).address();
		const std::pair<const char*, std::uintptr_t> sites[] = { { "the per-light pass", REL::ID(106335).address() + 0x732 },
			{ "the RemoveLight queue drain", REL::ID(106333).address() + 0x134 } };
		gClearGeometries = REL::Relocation<ClearFn>{ REL::ID(108285) };
		const char* what[2]{};
		for (int i = 0; i < 2; ++i) {
			what[i] = CheckSite(sites[i].second, erase);
			if (!what[i]) {
				SKSE::log::warn("not installed: {} does not hold the call this was written for", sites[i].first);
				return;
			}
		}
		auto& trampoline = SKSE::GetTrampoline();
		Erase<0>::original = trampoline.write_call<5>(sites[0].second, Erase<0>::Thunk);
		Erase<1>::original = trampoline.write_call<5>(sites[1].second, Erase<1>::Thunk);
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
	// the start line and the install's log lines are the only work here that can throw (low memory): none leaves the plugin
	try {
		// the version the DLL declares (xmake.lua set_version), never a typed number
		const auto* decl = SKSE::PluginDeclaration::GetSingleton();
		const auto  ver = decl ? std::format("{}.{}", decl->GetVersion().major(), decl->GetVersion().minor()) : std::string("?");
		SKSE::log::info("Floor Light Fix {} - a light leaving the scene is taken off the floors it lit", ver);
	} catch (...) {
	}
	try {
		Install();
	} catch (...) {
		// no call in the install throws but a log line; say that it stopped, if the log still can
		try {
			SKSE::log::error("the install stopped on an exception (low memory?): the fix may not be in place");
		} catch (...) {
		}
	}
	SKSE::GetMessagingInterface()->RegisterListener(OnMessage);
	return true;
}
