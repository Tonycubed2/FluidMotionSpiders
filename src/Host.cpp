#include "Host.h"

#include "../include/FluidMotionNPC_API.h"

namespace Host
{
	namespace
	{
		FMNPC::API g_api;
		bool       g_ok = false;
	}

	bool Connect()
	{
		g_ok = g_api.Load();
		if (g_ok) {
			logger::info("Host: connected to FluidMotionNPC API v{}", g_api.GetAPIVersion());
		} else {
			logger::error("Host: FluidMotionNPC.dll not loaded or older than API v{} - FluidMotionSpiders stays inactive. Install/update FluidMotionNPC.", FMNPC::kVersion);
		}
		return g_ok;
	}

	bool Connected() { return g_ok; }
	bool Enabled() { return g_ok && g_api.IsEnabled(); }
	bool Traversing(RE::FormID a_id) { return g_ok && g_api.IsTraversing(a_id); }

	void Claim(RE::FormID a_id, float a_seconds)
	{
		if (g_ok) {
			g_api.Claim(a_id, a_seconds);
		}
	}

	bool OnNavmesh(RE::Actor* a_actor, RE::Actor* a_target) { return g_ok && g_api.OnNavmesh(a_actor, a_target); }

	void NoteDriven(RE::FormID a_id, float a_speed)
	{
		if (g_ok) {
			g_api.NoteDriven(a_id, a_speed);
		}
	}

	void SetMaxTurn(RE::FormID a_id, float a_degPerSec, float a_seconds)
	{
		if (g_ok) {
			g_api.SetMaxTurnRate(a_id, a_degPerSec, a_seconds);
		}
	}

	bool RegisterPostGraph(void (*a_fn)(RE::Actor*, int)) { return g_ok && g_api.RegisterPostGraph(a_fn); }
	bool RegisterFrame(void (*a_fn)(float)) { return g_ok && g_api.RegisterFrame(a_fn); }
}
