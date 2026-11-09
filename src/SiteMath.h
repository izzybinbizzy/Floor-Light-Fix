// Floor Light Fix - SKSE plugin
// Copyright (C) 2026 izzydoingit
// GPL-3.0-or-later; see LICENSE.
//
// The game-free parts: reading what a hooked call site holds before anything is written there, and which entries an erase
// takes out. Tested without the game by tests/testfloor.cpp.
#pragma once

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <optional>
#include <utility>

namespace SiteMath
{
	// what a call site holds
	enum class Site
	{
		// the 5-byte call onto the game's own function
		kGame,
		// a 5-byte call into another module: another plugin's hook of the same call
		kChained,
		// anything else: another game version, or a site rewritten some other way
		kOther
	};

	// where the 5-byte relative call (E8 rel32) at a_site goes; nothing when the bytes are not one
	[[nodiscard]] inline std::optional<std::uintptr_t> CallTarget(const std::array<std::uint8_t, 5>& a_bytes, std::uintptr_t a_site) noexcept
	{
		if (a_bytes[0] != 0xE8) {
			return std::nullopt;
		}
		std::int32_t rel = 0;
		std::memcpy(&rel, a_bytes.data() + 1, sizeof(rel));
		return a_site + 5 + static_cast<std::uintptr_t>(static_cast<std::intptr_t>(rel));
	}

	// a_imageStart / a_imageEnd: the game's own module, so a call that leaves it is another plugin's hook
	[[nodiscard]] inline Site Judge(std::optional<std::uintptr_t> a_target, std::uintptr_t a_expected, std::uintptr_t a_imageStart,
		std::uintptr_t a_imageEnd) noexcept
	{
		if (!a_target) {
			return Site::kOther;
		}
		if (*a_target == a_expected) {
			return Site::kGame;
		}
		const bool inGame = *a_target >= a_imageStart && *a_target < a_imageEnd;
		return inGame ? Site::kOther : Site::kChained;
	}

	// the entries [first, last) an erase of a_count from a_index takes out of an array of a_size, kept inside the array
	[[nodiscard]] constexpr std::pair<std::uint32_t, std::uint32_t> Erased(std::uint32_t a_index, std::uint32_t a_count,
		std::uint32_t a_size) noexcept
	{
		const std::uint64_t first = std::min<std::uint64_t>(a_index, a_size);
		const std::uint64_t last = std::min<std::uint64_t>(std::uint64_t{ a_index } + a_count, a_size);
		return { static_cast<std::uint32_t>(first), static_cast<std::uint32_t>(std::max(first, last)) };
	}
}
