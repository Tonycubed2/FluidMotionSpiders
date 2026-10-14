#pragma once

namespace SpiderLegs
{
	void OnFrame(float a_delta);
	void Reset();
	bool Has(RE::FormID a_id);                   // has IK legs right now (rig found, skeleton fits, feature on)
	void Post(RE::Actor* a_actor, int a_src);    // after the actor's animation graph update
	void ApplyMovement(bool a_on);               // omni movement: patched copies of the spider races' movement types
}
