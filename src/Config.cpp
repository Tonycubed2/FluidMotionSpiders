#include "Config.h"

namespace Config
{
	namespace
	{
		Data g_data;

		std::vector<std::string> SplitList(std::string_view a_s)
		{
			std::vector<std::string> out;
			std::size_t              i = 0;
			while (i <= a_s.size()) {
				const auto  j = a_s.find(',', i);
				const auto  end = j == std::string_view::npos ? a_s.size() : j;
				std::string t(a_s.substr(i, end - i));
				const auto  b = t.find_first_not_of(" \t");
				const auto  e = t.find_last_not_of(" \t");
				if (b != std::string::npos) {
					out.push_back(t.substr(b, e - b + 1));
				}
				if (j == std::string_view::npos) {
					break;
				}
				i = j + 1;
			}
			return out;
		}
	}

	const Data& Get() { return g_data; }

	void Load()
	{
		CSimpleIniA ini;
		ini.SetUnicode();
		const auto rc = ini.LoadFile("Data/SKSE/Plugins/FluidMotionSpiders.ini");
		if (rc < 0) {
			logger::warn("Config: FluidMotionSpiders.ini not found, using defaults");
		}
		Data d;

		{
			auto& sp = d.spider;
			auto  spf = [&](const char* a_key, float& a_v) { a_v = static_cast<float>(ini.GetDoubleValue("Spider", a_key, a_v)); };
			sp.enabled = ini.GetBoolValue("Spider", "bEnabled", sp.enabled);
			sp.maxSpiders = std::clamp(static_cast<int>(ini.GetLongValue("Spider", "iMaxSpiders", sp.maxSpiders)), 1, 30);
			sp.maxPerFrame = std::clamp(static_cast<int>(ini.GetLongValue("Spider", "iMaxStartsPerFrame", sp.maxPerFrame)), 1, 8);
			spf("fRadius", sp.radius);
			spf("fStepDist", sp.stepDist);
			spf("fStepDistSpeed", sp.stepDistSpeed);
			spf("fStepDistMax", sp.stepDistMax);
			spf("fOvershoot", sp.overshoot);
			spf("fStepHeight", sp.stepHeight);
			spf("fSwingFrac", sp.swingFrac);
			spf("fSwingFracSlow", sp.swingFracSlow);
			spf("fSwingMin", sp.swingMin);
			spf("fSwingMax", sp.swingMax);
			spf("fSwingSlow", sp.swingSlow);
			spf("fFootSpeed", sp.footSpeed);
			spf("fSideStep", sp.sideStep);
			spf("fBodyTime", sp.bodyTime);
			spf("fMaxTiltDeg", sp.maxTiltDeg);
			spf("fFade", sp.fade);
			sp.recommit = ini.GetBoolValue("Spider", "bRecommit", sp.recommit);
			sp.omni = ini.GetBoolValue("Spider", "bOmni", sp.omni);
			spf("fMaxTurnDeg", sp.maxTurnDeg);
			spf("fSideSpeed", sp.sideSpeed);
			spf("fBackSpeed", sp.backSpeed);
			spf("fFitMaxDeg", sp.fitMaxDeg);
			spf("fFitWarnSec", sp.fitWarnSec);
			sp.races = SplitList(ini.GetValue("Spider", "sRaces", "Spider"));
			sp.excludeRaces = SplitList(ini.GetValue("Spider", "sExcludeRaces", ""));
			sp.log = ini.GetBoolValue("Spider", "bLog", sp.log);
		}

		{
			auto& cl = d.climb;
			auto  clf = [&](const char* a_key, float& a_v) { a_v = static_cast<float>(ini.GetDoubleValue("Crawl", a_key, a_v)); };
			clf("fSpeed", cl.speed);
			clf("fAccel", cl.accel);
			clf("fTurnDeg", cl.turnDeg);
			clf("fEngage", cl.engage);
			clf("fMinRise", cl.minRise);
			clf("fStuckSec", cl.stuckSec);
			clf("fStopDist", cl.stopDist);
			clf("fHover", cl.hover);
			clf("fStand", cl.stand);
			clf("fMaxAngleDeg", cl.maxAngleDeg);
			clf("fSideFrac", cl.sideFrac);
			clf("fBackFrac", cl.backFrac);
			clf("fSepRadius", cl.sepRadius);
			clf("fMaxSeconds", cl.maxSeconds);
			clf("fCooldown", cl.cooldown);
			cl.log = ini.GetBoolValue("Crawl", "bLog", cl.log);
		}

		g_data = std::move(d);
		logger::info("Config: spiders {} (max {}, radius {:.0f}, omni {}), {} exclusion pattern(s)", g_data.spider.enabled ? "on" : "off", g_data.spider.maxSpiders,
			g_data.spider.radius, g_data.spider.omni, g_data.spider.excludeRaces.size());
	}
}
