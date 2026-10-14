#pragma once

namespace Config
{
	// Procedural IK legs (frostbite + dwarven spiders).
	struct Spider
	{
		bool  enabled = true;
		int   maxSpiders = 6;
		float radius = 2500.0f;
		float stepDist = 20.0f;
		float stepDistMax = 32.0f;
		float stepDistSpeed = 0.07f;
		float overshoot = 0.6f;
		float stepHeight = 12.0f;
		float swingFrac = 0.45f;
		float swingFracSlow = 0.33f;
		float swingMin = 0.08f;
		float swingMax = 0.80f;
		float swingSlow = 30.0f;
		float sideStep = 0.5f;
		float footSpeed = 0.0f;
		float bodyTime = 0.10f;
		float maxTiltDeg = 25.0f;
		float fade = 0.15f;
		int   maxPerFrame = 6;
		bool  recommit = true;
		bool  omni = true;
		float maxTurnDeg = 75.0f;
		float sideSpeed = 0.6f;
		float backSpeed = 0.5f;
		float fitMaxDeg = 35.0f;
		float fitWarnSec = 3.0f;
		std::vector<std::string> races;
		std::vector<std::string> excludeRaces;
		bool  log = false;
	};

	// Wall / ceiling crawling in combat (part of the legs feature, no separate toggle).
	struct Climb
	{
		float speed = 230.0f;
		float accel = 600.0f;
		float turnDeg = 90.0f;
		float engage = 2500.0f;
		float minRise = 80.0f;
		float stuckSec = 1.5f;
		float stopDist = 105.0f;
		float hover = 30.0f;
		float stand = 34.0f;
		float maxAngleDeg = 100.0f;
		float sideFrac = 0.6f;
		float backFrac = 0.5f;
		float sepRadius = 60.0f;
		float maxSeconds = 40.0f;
		float cooldown = 3.0f;
		bool  log = false;
	};

	struct Data
	{
		Spider spider;
		Climb  climb;
	};

	const Data& Get();
	void        Load();
}
