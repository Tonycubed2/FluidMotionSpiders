// FluidMotionSpiders - procedural IK legs and wall/ceiling crawling for frostbite and dwarven spiders.
// Split out of "Fluid Motion: Pathfinding Overhaul" by megakuutti (GPL-3.0). Requires FluidMotionNPC.

#include "Config.h"
#include "Host.h"
#include "SpiderClimb.h"
#include "SpiderLegs.h"

namespace
{
	bool g_registered = false;

	void OnFrame(float a_dt)
	{
		SpiderClimb::OnFrame(a_dt);
		SpiderLegs::OnFrame(a_dt);
	}

	void OnPostGraph(RE::Actor* a_actor, int a_src)
	{
		SpiderLegs::Post(a_actor, a_src);
	}

	void ApplyMovement()
	{
		const auto& c = Config::Get().spider;
		SpiderLegs::ApplyMovement(false);
		SpiderLegs::ApplyMovement(Host::Connected() && c.enabled && c.omni);
	}

	void OnMessage(SKSE::MessagingInterface::Message* a_msg)
	{
		switch (a_msg->type) {
		case SKSE::MessagingInterface::kDataLoaded:
			Config::Load();
			if (Host::Connect()) {
				g_registered = Host::RegisterFrame(&OnFrame) && Host::RegisterPostGraph(&OnPostGraph);
				if (!g_registered) {
					logger::error("FluidMotionSpiders: could not register with FluidMotionNPC - inactive");
				}
			}
			ApplyMovement();
			break;
		case SKSE::MessagingInterface::kPostLoadGame:
		case SKSE::MessagingInterface::kNewGame:
			Config::Load();
			SpiderClimb::Reset();
			SpiderLegs::Reset();
			ApplyMovement();
			break;
		default:
			break;
		}
	}
}

SKSEPluginLoad(const SKSE::LoadInterface* a_skse)
{
	SKSE::Init(a_skse, { .logPattern = "[%H:%M:%S.%e] [%l] %v" });

	const auto ver = a_skse->RuntimeVersion();
	logger::info("FluidMotionSpiders loaded, game runtime {}", ver.string());
	const bool checked = ver == SKSE::RUNTIME_SSE_1_5_97 || ver == SKSE::RUNTIME_SSE_1_6_1170 || ver == SKSE::RUNTIME_SSE_1_7_99 ||
	                     ver == SKSE::RUNTIME_SSE_1_7_104 || ver == SKSE::RUNTIME_VR_1_4_15;
	if (!checked) {
		logger::warn("FluidMotionSpiders was checked against 1.5.97, 1.6.1170, 1.7.99, 1.7.104 and VR 1.4.15");
	}

	SKSE::GetMessagingInterface()->RegisterListener(OnMessage);
	return true;
}
