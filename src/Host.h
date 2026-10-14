#pragma once

// Bridge to FluidMotionNPC's public API. Everything here is a no-op / false until Host::Connect() succeeded.
namespace Host
{
	bool Connect();    // resolve FluidMotionNPC.dll exports; false if missing or too old
	bool Connected();
	bool Enabled();    // FluidMotionNPC loaded and its [General] bEnabled is on
	bool Traversing(RE::FormID a_id);  // FluidMotionNPC is jumping / routing / climbing this actor
	void Claim(RE::FormID a_id, float a_seconds);
	bool OnNavmesh(RE::Actor* a_actor, RE::Actor* a_target);
	void NoteDriven(RE::FormID a_id, float a_speed);
	void SetMaxTurn(RE::FormID a_id, float a_degPerSec, float a_seconds);
	bool RegisterPostGraph(void (*a_fn)(RE::Actor*, int));
	bool RegisterFrame(void (*a_fn)(float));
}
