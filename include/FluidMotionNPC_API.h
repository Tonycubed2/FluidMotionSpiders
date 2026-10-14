#pragma once

// FluidMotionNPC public API (version 1).
//
// Lets other SKSE plugins work alongside FluidMotionNPC without fighting over the same actor:
//  - ask whether FluidMotionNPC is moving an actor right now (jump arc, route leg, climb),
//  - claim an actor so FluidMotionNPC leaves it alone for a while,
//  - get called after each actor's animation graph update and once per frame.
//
// Usage (no link-time dependency, resolve at kPostLoad or later):
//
//   #include "FluidMotionNPC_API.h"
//   FMNPC::API api;
//   if (!api.Load()) { /* FluidMotionNPC.dll missing or too old */ }
//   if (api.IsTraversing(actor->GetFormID())) { ... }
//
// All functions must be called on the main thread, except the post-graph callback which FluidMotionNPC
// itself calls from the game's animation worker threads (src 0) or the main thread (src 1).

#include <cstdint>

// Needs CommonLibSSE-NG (REX::W32) to be included first, e.g. from your PCH.

extern "C"
{
	using FMNPC_PostGraphFn = void (*)(RE::Actor* a_actor, int a_src);
	using FMNPC_FrameFn = void (*)(float a_dt);
}

namespace FMNPC
{
	inline constexpr int kVersion = 1;

	struct API
	{
		int (*GetAPIVersion)() = nullptr;
		bool (*IsEnabled)() = nullptr;
		bool (*IsTraversing)(std::uint32_t a_formID) = nullptr;
		void (*Claim)(std::uint32_t a_formID, float a_seconds) = nullptr;
		bool (*OnNavmesh)(RE::Actor* a_actor, RE::Actor* a_target) = nullptr;
		void (*NoteDriven)(std::uint32_t a_formID, float a_speed) = nullptr;
		void (*SetMaxTurnRate)(std::uint32_t a_formID, float a_degPerSec, float a_seconds) = nullptr;
		bool (*RegisterPostGraph)(FMNPC_PostGraphFn a_fn) = nullptr;
		bool (*RegisterFrame)(FMNPC_FrameFn a_fn) = nullptr;

		// Resolves the exports from an already loaded FluidMotionNPC.dll. Returns false if it is not loaded
		// or older than this header.
		bool Load()
		{
			const auto mod = REX::W32::GetModuleHandleW(L"FluidMotionNPC.dll");
			if (!mod) {
				return false;
			}
			auto get = [&](const char* a_name) { return (REX::W32::GetProcAddress(mod, a_name)); };
			GetAPIVersion = reinterpret_cast<decltype(GetAPIVersion)>(get("FMNPC_GetAPIVersion"));
			if (!GetAPIVersion || GetAPIVersion() < kVersion) {
				return false;
			}
			IsEnabled = reinterpret_cast<decltype(IsEnabled)>(get("FMNPC_IsEnabled"));
			IsTraversing = reinterpret_cast<decltype(IsTraversing)>(get("FMNPC_IsTraversing"));
			Claim = reinterpret_cast<decltype(Claim)>(get("FMNPC_Claim"));
			OnNavmesh = reinterpret_cast<decltype(OnNavmesh)>(get("FMNPC_OnNavmesh"));
			NoteDriven = reinterpret_cast<decltype(NoteDriven)>(get("FMNPC_NoteDriven"));
			SetMaxTurnRate = reinterpret_cast<decltype(SetMaxTurnRate)>(get("FMNPC_SetMaxTurnRate"));
			RegisterPostGraph = reinterpret_cast<decltype(RegisterPostGraph)>(get("FMNPC_RegisterPostGraph"));
			RegisterFrame = reinterpret_cast<decltype(RegisterFrame)>(get("FMNPC_RegisterFrame"));
			return IsEnabled && IsTraversing && Claim && OnNavmesh && NoteDriven && SetMaxTurnRate && RegisterPostGraph && RegisterFrame;
		}
	};
}
