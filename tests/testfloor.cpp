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
	constexpr int kFloor = 23;

	using SiteMath::CallTarget;
	using SiteMath::Erased;
	using SiteMath::Judge;
	using SiteMath::Site;

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
		// the site of 1.6.1170's per-light pass onto its erase, as read in the exe: 0x1414A1540 + 0x732 -> 0x1414A83F0
		constexpr std::uintptr_t pass = 0x1414A1540 + 0x732;
		constexpr std::uintptr_t erase = 0x1414A83F0;
		CHECK(CallTarget(Call(static_cast<std::int32_t>(erase - (pass + 5))), pass) == erase);
		// anything but E8 is not a call this was written for: a jump, a nop, a breakpoint
		CHECK(!CallTarget(Call(0x100, 0xE9), site).has_value());
		CHECK(!CallTarget(Call(0x100, 0x90), site).has_value());
		CHECK(!CallTarget(Call(0x100, 0xCC), site).has_value());
	}

	void Judging()
	{
		constexpr std::uintptr_t start = 0x140000000;
		constexpr std::uintptr_t end = 0x142400000;
		constexpr std::uintptr_t erase = 0x1414A83F0;
		CHECK(Judge(erase, erase, start, end) == Site::kGame);
		// inside the game but not its erase: refused
		CHECK(Judge(erase + 0x10, erase, start, end) == Site::kOther);
		CHECK(Judge(start, erase, start, end) == Site::kOther);
		CHECK(Judge(end - 1, erase, start, end) == Site::kOther);
		// outside the game's module: another plugin's hook, chained
		CHECK(Judge(end, erase, start, end) == Site::kChained);
		CHECK(Judge(start - 0x1000, erase, start, end) == Site::kChained);
		// no call at all
		CHECK(Judge(std::nullopt, erase, start, end) == Site::kOther);
	}

	void Ranges()
	{
		using Range = std::pair<std::uint32_t, std::uint32_t>;
		// what both hooked sites do: one entry
		CHECK(Erased(3, 1, 10) == Range(3, 4));
		CHECK(Erased(0, 10, 10) == Range(0, 10));
		// never past the array's end, and nothing when the index is already past it
		CHECK(Erased(8, 5, 10) == Range(8, 10));
		CHECK(Erased(10, 1, 10) == Range(10, 10));
		CHECK(Erased(25, 1, 10) == Range(10, 10));
		// index + count past 32 bits does not wrap round to the start
		CHECK(Erased(0xFFFFFFFFu, 2, 10) == Range(10, 10));
		CHECK(Erased(5, 0, 10) == Range(5, 5));
	}
}

int main()
{
	Targets();
	Judging();
	Ranges();
	const bool enough = gPassed + gFailed >= kFloor;
	std::printf("testfloor: %d passed, %d failed, %d checks (floor %d)%s\n", gPassed, gFailed, gPassed + gFailed, kFloor,
		enough ? "" : " - FEWER CHECKS RAN THAN THE FLOOR");
	return gFailed == 0 && enough ? 0 : 1;
}
