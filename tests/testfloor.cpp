// Floor Light Fix - tests for src/SiteMath.h (no game needed).
// Copyright (C) 2026 izzydoingit
// GPL-3.0-or-later; see LICENSE.
//
// PC Runner\plugin build.py builds and runs them (xmake target testfloor), then again under clang's address and
// undefined-behavior checkers. The run fails when a check fails, AND when fewer checks ran than kFloor (a test that stopped
// part way proves nothing).

#include "SiteMath.h"

#include <array>
#include <cstdint>
#include <cstdio>
#include <limits>
#include <optional>
#include <utility>

namespace
{
	int gFailed = 0;
	int gPassed = 0;

	void Check(bool a_ok, const char* a_what, int a_line)
	{
		if (a_ok) {
			++gPassed;
		} else {
			++gFailed;
			std::printf("FAIL line %d: %s\n", a_line, a_what);
		}
	}
#define CHECK(x) Check((x), #x, __LINE__)

	// the fewest checks a whole run makes (raise it when checks are added)
	constexpr int kFloor = 25;
	// the 1.6.1170 exe's preferred base, where the offsets in SiteMath::Game1170 count from
	constexpr std::uintptr_t kBase = 0x140000000;

	using SiteMath::CallTarget;

	std::array<std::uint8_t, 5> Call(std::int32_t a_rel, std::uint8_t a_op = 0xE8)
	{
		const auto u = static_cast<std::uint32_t>(a_rel);
		return { a_op, static_cast<std::uint8_t>(u), static_cast<std::uint8_t>(u >> 8), static_cast<std::uint8_t>(u >> 16),
			static_cast<std::uint8_t>(u >> 24) };
	}

	void Targets()
	{
		constexpr std::uintptr_t site = 0x1414A1C72;
		// forward and backward: the target counts from the end of the 5-byte call
		CHECK(CallTarget(Call(0x100), site) == site + 5 + 0x100);
		CHECK(CallTarget(Call(-0x10), site) == site + 5 - 0x10);
		CHECK(CallTarget(Call(0), site) == site + 5);
		// the farthest a rel32 reaches, both ways
		CHECK(CallTarget(Call(std::numeric_limits<std::int32_t>::max()), site) == site + 5 + 0x7FFFFFFF);
		CHECK(CallTarget(Call(std::numeric_limits<std::int32_t>::min()), site) == site + 5 - 0x80000000ull);
		// anything but E8 is not a call this was written for: a jump, a nop, a breakpoint
		CHECK(!CallTarget(Call(0x100, 0xE9), site).has_value());
		CHECK(!CallTarget(Call(0x100, 0x90), site).has_value());
		CHECK(!CallTarget(Call(0x100, 0xCC), site).has_value());
	}

	// the 5 bytes at each hooked call site, copied out of the unpacked 1.6.1170 exe at its preferred base: each must read as a call
	// onto the erase, so a wrong offset in SiteMath::Game1170 fails here
	void RealSites()
	{
		using namespace SiteMath::Game1170;
		constexpr std::array<std::uint8_t, 5> passBytes{ 0xE8, 0x79, 0x67, 0x00, 0x00 };
		constexpr std::array<std::uint8_t, 5> drainBytes{ 0xE8, 0x47, 0x6F, 0x00, 0x00 };
		// the addresses themselves, so all the offsets moving together cannot pass the relative checks below
		CHECK(kBase + kPass + kPassCall == 0x1414A1C72);
		CHECK(kBase + kDrain + kDrainCall == 0x1414A14A4);
		CHECK(kBase + kErase == 0x1414A83F0);
		CHECK(CallTarget(passBytes, kBase + kPass + kPassCall) == kBase + kErase);
		CHECK(CallTarget(drainBytes, kBase + kDrain + kDrainCall) == kBase + kErase);
	}

	// the lights the hook clears: the ones the game's erase takes, clamped to the array (the game does not clamp; both sites pass 1)
	void Ranges()
	{
		using SiteMath::ErasedRange;
		using R = std::pair<std::uint32_t, std::uint32_t>;
		// what both hooked sites pass: one light
		CHECK(ErasedRange(3, 1, 8) == R(3, 4));
		CHECK(ErasedRange(0, 1, 1) == R(0, 1));
		CHECK(ErasedRange(7, 1, 8) == R(7, 8));
		// a count equal to the size takes the whole array, whatever the index
		CHECK(ErasedRange(2, 5, 5) == R(0, 5));
		CHECK(ErasedRange(8, 8, 8) == R(0, 8));
		CHECK(ErasedRange(std::numeric_limits<std::uint32_t>::max(), 8, 8) == R(0, 8));
		// a range running past the end stops at it; nothing past it, nothing for a count of 0 or an empty array
		CHECK(ErasedRange(6, 4, 8) == R(6, 8));
		CHECK(ErasedRange(9, 1, 8) == R(8, 8));
		CHECK(ErasedRange(3, 0, 8) == R(3, 3));
		CHECK(ErasedRange(0, 0, 0) == R(0, 0));
		CHECK(ErasedRange(std::numeric_limits<std::uint32_t>::max(), 2, 8) == R(8, 8));
		CHECK(ErasedRange(4, std::numeric_limits<std::uint32_t>::max(), 8) == R(4, 8));
	}
}

int main()
{
	Targets();
	RealSites();
	Ranges();
	const bool enough = gPassed + gFailed >= kFloor;
	std::printf("testfloor: %d passed, %d failed, %d checks (floor %d)%s\n", gPassed, gFailed, gPassed + gFailed, kFloor,
		enough ? "" : " - FEWER CHECKS RAN THAN THE FLOOR");
	return gFailed == 0 && enough ? 0 : 1;
}
