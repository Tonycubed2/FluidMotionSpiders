#include "SpiderClimb.h"
#include "AnimMath.h"
#include "Config.h"
#include "Host.h"
#include "Raycast.h"
#include "SpiderLegs.h"

namespace SpiderClimb
{
	using namespace AnimMath;

	namespace
	{
		constexpr float kDeg = kPi / 180.0f;

		struct Crawler
		{
			RE::ActorHandle handle, target;
			RE::FormID      id = 0;
			std::string     name;
			V3              goal{};
			bool            pursue = false, hold = false, arrived = false;
			V3              A{}, Av{}, n{ 0, 0, 1 }, nT{ 0, 0, 1 }, fwd{ 0, 1, 0 };
			float           offset = 0, speed = 0, scale = 1, t = 0, stuck = 0, progT = 0, progDist = 1e9f, logT = 0, yaw = 0;
			bool            radiusLogged = false;
			float           slide = 1.0f;
			V3              goodA{}, goodN{ 0, 0, 1 };
			bool            haveGood = false;
			float           flip = 0, goodT = 0, recovT = -9.0f;
			int             recov = 0;
			bool            descending = false;  // v3: gave up on the target, climbing down to the floor
			V3              prevN{ 0, 0, 1 };    // v4: surface it was on before the last big turn, and when that was
			float           flipT = -9.0f;
			V3              via{};               // v4: wall to walk to first when the target is straight overhead
			bool            viaOn = false;
		};

		std::vector<Crawler>                  g_c;
		struct Other
		{
			V3         pos;
			float      r;
			RE::FormID id;
		};
		std::vector<Other> g_others;
		struct Grace
		{
			RE::FormID id;
			float      until;
		};
		std::vector<Grace>                    g_grace;
		std::unordered_map<RE::FormID, float> g_cool;
		struct Memo
		{
			RE::FormID target = 0;
			float      best = 1e9f, bestT = 0, offT = -1.0f;
			V3         failPos{}, failTgt{};  // v3: where the last crawl failed, and where its target was
			float      failT = -1e9f;
		};
		std::unordered_map<RE::FormID, Memo>  g_memo;
		float                                 g_time = 0, g_scanT = 0, g_statT = 0;
		int                                   g_endLogs = 0;

		// Crawling is part of the spider legs feature (no separate toggle).
		bool On() { return Config::Get().spider.enabled && Host::Enabled(); }

		// While crawling, FluidMotionNPC is told to leave the spider alone (no jumps/routes/climbs).
		constexpr float kClaimSec = 0.5f;

		Crawler* FindC(RE::FormID a_id)
		{
			for (auto& c : g_c) {
				if (c.id == a_id) {
					return &c;
				}
			}
			return nullptr;
		}

		V3 Face(const V3& a_n, const V3& a_toward) { return Norm(a_n.Dot(a_toward) >= 0.0f ? a_n : a_n * -1.0f, a_toward); }

		V3 OnPlane(const V3& a_v, const V3& a_n, const V3& a_fallback)
		{
			const V3 p = a_v - a_n * a_v.Dot(a_n);
			return Norm(p, a_fallback);
		}

		V3 DesiredDir(const V3& a_d, const V3& a_n, const V3& a_fwd)
		{
			const float g = a_d.Dot(a_n);
			const V3    ds = a_d - a_n * g;
			const float dl = ds.Length();
			V3          m = dl > 4.0f ? ds * (1.0f / dl) : OnPlane(a_fwd, a_n, a_fwd);
			const V3    up = OnPlane({ 0, 0, 1 }, a_n, { 0, 0, 0 });
			const float upl = up.Length();
			if (upl > 0.3f) {
				const V3 upD = up * (1.0f / upl);
				if (g < 0.0f) {
					const float al = Smooth(-g, 0.0f, 60.0f);
					m = Norm(m * (1.0f - al) + upD * al, m);
				} else if (dl < 6.0f) {
					m = upD * -1.0f;
				}
			}
			return m;
		}

		float Hor(const V3& a_n) { return std::sqrt((std::max)(0.0f, 1.0f - a_n.z * a_n.z)); }

		// v3: cages, cells, grates, gates... (sNoCrawlModels, by model path). Their collision has holes, so a spider
		// on them kept finding "surface" one step and nothing the next and got stuck or fell through.
		bool NoCrawl(RE::TESObjectREFR* a_ref)
		{
			auto* base = a_ref ? a_ref->GetBaseObject() : nullptr;
			if (!base) {
				return false;
			}
			static std::unordered_map<RE::TESBoundObject*, bool> cache;
			if (auto it = cache.find(base); it != cache.end()) {
				return it->second;
			}
			bool        no = false;
			auto*       model = base->As<RE::TESModel>();
			const char* path = model ? model->GetModel() : nullptr;
			if (path && *path) {
				std::string lower(path);
				std::ranges::transform(lower, lower.begin(), [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
				for (auto pat : Config::Get().climb.noCrawl) {
					std::ranges::transform(pat, pat.begin(), [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
					if (!pat.empty() && lower.find(pat) != std::string::npos) {
						no = true;
						break;
					}
				}
			}
			return cache[base] = no;
		}

		struct Step
		{
			V3 A, n;
			int kind = 0;
		};

		int g_backface = 0, g_inside = 0, g_hidden = 0, g_probes = 0, g_tunnel = 0;

		bool Advance(const Raycast::Caster& r, const Crawler& c, const V3& a_m, float a_s, Step& a_out)
		{
			const auto& cfg = Config::Get().climb;
			const float sc = c.scale, H = cfg.hover * sc, body = 22.0f * sc;
			const V3&   A = c.A;
			const V3&   n = c.nT;
			const float minNz = std::cos(cfg.maxAngleDeg * kDeg);
			auto cast = [&](const V3& from, const V3& to, Raycast::Hit& hit) {
				if (!r.CastSolid(from, to, hit) || Raycast::IsActorRef(hit.ref)) {
					return false;
				}
				// v2: tree trunks are not a surface to crawl on (unless bClimbTrees): with a tree between the spider
				// and its target it went up the trunk and down the other side instead of walking around it. A tree
				// hit fails the step, so the sideways tries below steer it around the trunk on the ground.
				if (!cfg.climbTrees && (hit.layer == RE::COL_LAYER::kTrees ||
										   (hit.ref && hit.ref->GetBaseObject() && hit.ref->GetBaseObject()->Is(RE::FormType::Tree)))) {
					return false;
				}
				if (NoCrawl(hit.ref)) {
					return false;
				}
				++g_probes;
				const V3 dir = Norm(to - from, { 0, 0, -1 });
				if ((hit.point - from).Length() < 1.5f) {
					++g_inside;
					return false;
				}
				if (Norm(hit.normal, dir * -1.0f).Dot(dir) > 0.15f) {
					++g_backface;
					return false;
				}
				return true;
			};
			auto exposed = [&](const V3& P, const V3& N) {
				Raycast::Hit h;
				if (!r.CastSolid(P + N * (120.0f * sc), P - N * 2.0f, h) || Raycast::IsActorRef(h.ref)) {
					return true;
				}
				const bool ok = (h.point - P).Length() < 8.0f;
				g_hidden += ok ? 0 : 1;
				return ok;
			};
			Raycast::Hit hit;

			{
				const V3 o = A + n * (H * 0.5f);
				if (cast(o, o + a_m * (a_s + body), hit)) {
					const V3 n2 = Norm(hit.normal, a_m * -1.0f);
					if (n2.Dot(n) < 0.9f) {
						if (n2.z < minNz) {
							return false;
						}
						if (exposed(hit.point, n2)) {
							a_out = { hit.point, n2, 1 };
							return true;
						}
					}
				}
			}
			const V3 p1 = A + a_m * a_s;
			{
				const V3 from = p1 + n * H;
				if (cast(from, p1 - n * (20.0f * sc), hit)) {
					const V3 nh = Norm(hit.normal, n);
					if (nh.Dot(n) >= 0.5f && nh.z >= minNz && (nh.Dot(n) > 0.95f || exposed(hit.point, nh))) {
						a_out = { hit.point, nh, 2 };
						return true;
					}
				}
			}
			for (float back : { 8.0f, 24.0f }) {
				const V3 from = p1 - n * (back * sc);
				if (cast(from, from - a_m * (H + 24.0f * sc), hit)) {
					const V3 nh = Norm(hit.normal, from - hit.point);
					if (nh.Dot(n) < 0.7f && nh.z >= minNz && exposed(hit.point, nh)) {
						a_out = { hit.point, nh, 3 };
						return true;
					}
				}
			}
			return false;
		}

		void ReleaseController(RE::Actor* a, bool a_air)
		{
			if (auto* cc = a->GetCharController()) {
				const float s = RE::bhkWorld::GetWorldScale();
				cc->context.currentState = a_air ? RE::hkpCharacterStateType::kInAir : RE::hkpCharacterStateType::kOnGround;
				cc->SetLinearVelocityImpl(RE::hkVector4(0, 0, 0, 0));
				(void)s;
				cc->fallStartHeight = a->GetPosition().z;
				cc->fallTime = 0.0f;
			}
		}

		void End(Crawler& c, RE::Actor* a, const char* a_reason)
		{
			if (a && c.n.z < 0.75f && !a->IsDead()) {
				g_grace.push_back({ c.id, g_time + 2.5f });
				ReleaseController(a, true);
			}
			Host::Claim(c.id, 0.0f);
			const bool bad = std::strcmp(a_reason, "arrived") != 0 && std::strcmp(a_reason, "done") != 0 && std::strcmp(a_reason, "pounce") != 0;
			if (bad || c.descending) {
				auto& m = g_memo[c.id];
				if (m.failT < g_time - 0.5f) {
					m.failPos = c.A;
					if (auto tp = c.target.get(); tp.get()) {
						m.failTgt = tp->GetPosition();
					}
					m.failT = g_time;
				}
			}
			g_cool[c.id] = g_time + (bad ? Config::Get().climb.cooldown : 1.5f);
			if (Config::Get().climb.log && g_endLogs++ < 200) {
				logger::info("CLIMB end {:08X} '{}' {} after {:.1f}s n.z={:.2f} pos=({:.0f},{:.0f},{:.0f}) goal=({:.0f},{:.0f},{:.0f})", c.id, c.name, a_reason, c.t, c.n.z,
					c.A.x, c.A.y, c.A.z, c.goal.x, c.goal.y, c.goal.z);
			}
		}

		void Drive(RE::Actor* a, Crawler& c)
		{
			const V3 pos = c.Av + c.n * c.offset;
			a->SetPosition(pos, true);
			if (Hor(c.fwd) > 0.3f) {
				c.yaw = std::atan2(c.fwd.x, c.fwd.y);
			}
			a->SetHeading(c.yaw);
			if (auto* cc = a->GetCharController()) {
				cc->context.currentState = RE::hkpCharacterStateType::kOnGround;
				cc->SetLinearVelocityImpl(RE::hkVector4(0, 0, 0, 0));
				cc->fallStartHeight = pos.z;
				cc->fallTime = 0.0f;
				if (!c.radiusLogged && Config::Get().climb.log) {
					c.radiusLogged = true;
					logger::info("CLIMB capsule {:08X} radius={:.2f} (x70 = {:.1f}) height={:.1f} scale={:.2f} stand={:.1f}", c.id, cc->radius, cc->radius * 70.0f, cc->actorHeight,
						c.scale, Config::Get().climb.stand * c.scale);
				}
			}
			Host::NoteDriven(c.id, c.speed);
			Host::Claim(c.id, kClaimSec);
		}

		// v3: instead of letting go of a wall it is stuck on (and then trying the same wall again a few seconds later),
		// a stuck spider climbs down to the floor below it first. Only once; if that fails too it lets go.
		bool Descend(Crawler& c, const Raycast::Caster& r, RE::Actor* a)
		{
			if (c.descending || c.n.z > 0.75f) {
				return false;
			}
			Raycast::Hit h;
			const V3     from = c.A + c.nT * (Config::Get().climb.hover * c.scale);
			if (!r.CastSolid(from, from - V3{ 0, 0, 3000.0f }, h) || Raycast::IsActorRef(h.ref) || h.normal.z < 0.6f) {
				return false;
			}
			auto& m = g_memo[c.id];
			m.failPos = c.A;
			if (auto tp = c.target.get(); tp.get()) {
				m.failTgt = tp->GetPosition();
			}
			m.failT = g_time;
			c.descending = true;
			c.pursue = false;
			c.hold = false;
			c.goal = h.point;
			c.stuck = 0.0f;
			c.recov = 0;
			c.flip = 0.0f;
			c.progT = 0.0f;
			c.progDist = 1e9f;
			if (Config::Get().climb.log) {
				logger::info("CLIMB {:08X} '{}' stuck at ({:.0f},{:.0f},{:.0f}) n.z={:.2f} - climbing down to the floor ({:.0f} below)", c.id, c.name, c.A.x, c.A.y, c.A.z,
					c.n.z, c.A.z - h.point.z);
			}
			(void)a;
			return true;
		}

		bool UpdateOne(Crawler& c, float dt)
		{
			const auto& cfg = Config::Get().climb;
			auto        ptr = c.handle.get();
			RE::Actor*  a = ptr.get();
			const char* drop = nullptr;
			if (!a || a->IsDead() || !a->Is3DLoaded() || a->IsInRagdollState()) {
				drop = "lost";
			} else if (a->AsActorState() && a->AsActorState()->GetKnockState() != RE::KNOCK_STATE_ENUM::kNormal) {
				drop = "knocked";
			} else if (c.t > cfg.maxSeconds) {
				drop = "timeout";
			} else if (Host::Traversing(c.id)) {
				drop = "other";
			}
			RE::Actor* tgt = nullptr;
			if (!drop) {
				if (auto tp = c.target.get(); tp.get()) {
					tgt = tp.get();
					if (tgt->IsDead() || !tgt->Is3DLoaded()) {
						drop = "targetgone";
					}
				} else if (c.pursue) {
					drop = "targetgone";
				}
				if (!drop && c.pursue && !a->IsInCombat()) {
					drop = "calm";
				}
			}
			Raycast::Caster r;
			if (!drop && !r.Init(a)) {
				drop = "noworld";
			}
			if (drop) {
				End(c, a, drop);
				return false;
			}
			c.t += dt;
			c.scale = (a->Get3D(false) && a->Get3D(false)->world.scale > 0.05f) ? a->Get3D(false)->world.scale : 1.0f;
			const float sc = c.scale;
			if (c.pursue && tgt) {
				c.goal = tgt->GetPosition() + V3{ 0, 0, 0.3f * (std::max)(tgt->GetHeight(), 40.0f) };
			}

				// v3: pounce. On a wall or ceiling right next to its target, drop down onto it: the fight happens on the
			// floor, where the spider can be hit (on a wall the game's collision capsule stays upright and doesn't
			// match the body, so it was very hard to hit there).
			if (c.pursue && tgt && !c.descending && cfg.pounceDist > 0.0f && c.n.z < 0.3f) {
				const V3    tp = tgt->GetPosition();
				const float hd = std::hypot(tp.x - c.A.x, tp.y - c.A.y);
				if (hd < cfg.pounceDist * c.scale && c.A.z - tp.z > 50.0f * c.scale) {
					End(c, a, "pounce");
					return false;
				}
			}
			const V3    d = c.goal - c.A;
			const float dist3 = d.Length();
			const float stop = c.pursue ? cfg.stopDist * sc : 18.0f * sc;
			c.arrived = dist3 <= stop;
			if (c.arrived && !c.hold && (!c.pursue || c.n.z > 0.85f)) {
				End(c, a, c.pursue ? "arrived" : "done");
				return false;
			}
			// v4: target overhead - walk to the chosen wall first; once on it (or next to it) steer for the target again
			if (c.viaOn && (c.n.z < 0.75f || std::hypot(c.via.x - c.A.x, c.via.y - c.A.y) < 30.0f * sc)) {
				c.viaOn = false;
			}
			V3       m = DesiredDir(c.viaOn ? c.via - c.A : d, c.nT, c.fwd);
			V3 corr{};
			{
				V3          sep{};
				const float myR = cfg.sepRadius * sc;
				for (const auto& o : g_others) {
					if (o.id == c.id) {
						continue;
					}
					const V3    dd = c.A - o.pos;
					const float dist = dd.Length(), minD = (myR + o.r) * 0.9f;
					if (dist >= minD * 1.6f || dist < 0.01f) {
						continue;
					}
					const V3 dir = OnPlane(dd, c.nT, { 0, 0, 0 });
					const V3 dn = dir.Length() > 0.05f ? Norm(dir, dir) : OnPlane(c.fwd, c.nT, c.fwd) * -1.0f;
					const float k = 1.0f - dist / (minD * 1.6f);
					sep = sep + dn * (k * k);
					if (dist < minD) {
						corr = corr + dn * ((minD - dist) * 0.5f);
					}
				}
				if (sep.Length() > 0.01f && !c.arrived) {
					m = Norm(m + sep * 1.5f, m);
				}
			}

			V3 faceDir = m;
			if (tgt && !c.viaOn) {
				faceDir = OnPlane(tgt->GetPosition() - c.A, c.n, m);
			}
			{
				const float ang = std::atan2(c.n.Dot(c.fwd.Cross(faceDir)), c.fwd.Dot(faceDir));
				float       rate = cfg.turnDeg;
				if (const float cap = Config::Get().spider.maxTurnDeg; cap > 0.0f) {
					rate = (std::min)(rate, cap);
				}
				const float lim = rate * kDeg * dt;
				c.fwd = OnPlane(AxisAngle(c.n, Clamp(ang, -lim, lim)) * c.fwd, c.n, m);
			}

			const bool  struggling = c.flip > 3.0f || c.stuck > 0.5f;
			c.flip = (std::max)(0.0f, c.flip - 1.5f * dt);
			const float cs = c.fwd.Dot(m);
			const float factor = cs >= 0.0f ? cfg.sideFrac + (1.0f - cfg.sideFrac) * cs : cfg.sideFrac + (cfg.backFrac - cfg.sideFrac) * -cs;
			float       want = c.arrived ? 0.0f : cfg.speed * factor * (c.pursue ? Smooth(dist3, stop, stop + 120.0f * sc) : Smooth(dist3, 18.0f * sc, 120.0f * sc)) * (struggling ? 0.4f : 1.0f);
			c.speed = want > c.speed ? (std::min)(want, c.speed + cfg.accel * dt) : (std::max)(want, c.speed - 2.0f * cfg.accel * dt);

			if (c.speed > 1.0f) {
				Step        st;
				const float s = c.speed * dt;
				auto crosses = [&](const Step& t) {
					const float    hv = cfg.hover * c.scale * 0.7f;
					Raycast::Hit   h;
					auto blocked = [&](const V3& a_from, const V3& a_to) { return r.CastSolid(a_from, a_to, h) && !Raycast::IsActorRef(h.ref); };
					bool x = blocked(c.A + c.nT * hv, t.A + t.n * hv);
					// v3: a step that turns the body by more than ~80 degrees wraps around an edge. It must really go
					// AROUND it: out past the edge along the current surface, then over to the new one. A spider on a
					// ceiling used to find the floor of the room above through a seam and walk on top of it.
					if (!x && t.n.Dot(c.nT) < 0.2f) {
						const V3    p0 = c.A + c.nT * hv;
						const V3    along = OnPlane(t.A - c.A, c.nT, c.fwd);
						const V3    p1 = p0 + along * ((t.A - c.A).Length() + 2.0f * hv);
						x = blocked(p0, p1) || blocked(p1, t.A + t.n * hv);
					}
					g_tunnel += x ? 1 : 0;
					return x;
				};
				// v4: no flip-flopping at an inside corner (floor <-> wall): a step straight back onto the surface it left
				// less than 0.6s ago is refused, so it commits to the wall (or the floor) instead of shaking between them.
				auto backflip = [&](const Step& t) { return t.n.Dot(c.nT) < 0.85f && c.t - c.flipT < 0.6f && t.n.Dot(c.prevN) > 0.95f; };
				bool        ok = Advance(r, c, m, s, st) && !crosses(st) && !backflip(st);
				for (int k = 0; k < 6 && !ok; ++k) {
					const float deg = (k < 2 ? 35.0f : k < 4 ? 70.0f : 105.0f) * ((k & 1) ? -1.0f : 1.0f) * c.slide;
					const V3    m2 = Norm(AxisAngle(c.nT, deg * kDeg) * m, m);
					if (Advance(r, c, m2, s, st) && !crosses(st) && !backflip(st)) {
						ok = true;
						if (deg < 0.0f) {
							c.slide = -c.slide;
						}
					}
				}
				if (ok) {
					if (st.n.Dot(c.nT) < 0.85f) {
						c.prevN = c.nT;
						c.flipT = c.t;
						c.flip += 1.0f;
					}
					c.A = st.A;
					c.nT = st.n;
					c.stuck = (std::max)(0.0f, c.stuck - dt);
					if (!struggling && c.t - c.goodT > 0.3f) {
						c.goodT = c.t;
						c.goodA = c.A;
						c.goodN = c.nT;
						c.haveGood = true;
					}
				} else {
					c.stuck += dt;
					c.speed *= 0.5f;
				}
			}
			if (corr.Length() > 0.01f) {
				const float lim = 150.0f * sc * dt;
				c.A = c.A + corr * ((std::min)(corr.Length(), lim) / corr.Length());
			}
			if (struggling && c.haveGood && c.flip + c.stuck * 4.0f > 5.0f && c.t - c.recovT > 1.0f) {
				c.recovT = c.t;
				if (++c.recov >= 3) {
					if (Descend(c, r, a)) {
						return true;
					}
					End(c, a, "wedged");
					return false;
				}
				c.A = c.goodA;
				c.nT = c.goodN;
				c.n = c.goodN;
				c.flip = 0.0f;
				c.stuck = 0.0f;
				c.slide = -c.slide;
				c.speed = 0.0f;
			}
			c.n = Norm(c.n + (c.nT - c.n) * (1.0f - std::exp(-dt / (struggling ? 0.3f : 0.08f))), c.nT);
			c.fwd = OnPlane(c.fwd, c.n, m);
			const float k = 1.0f - std::exp(-dt / (struggling ? 0.22f : 0.05f));
			c.Av = (c.Av - c.A).Length() > 80.0f * sc ? c.A : c.Av + (c.A - c.Av) * k;
			c.offset = cfg.stand * sc * Hor(c.n);

			if (!c.arrived && want > 20.0f) {
				c.progT += dt;
				if (c.progT >= 1.5f) {
					if (dist3 > c.progDist - 12.0f * sc) {
						c.stuck += 1.0f;
					}
					c.progDist = dist3;
					c.progT = 0.0f;
				}
			}
			if (c.stuck > 1.2f) {
				if (Descend(c, r, a)) {
					return true;
				}
				End(c, a, "blocked");
				return false;
			}
			Drive(a, c);
			if (cfg.log && c.t - c.logT > 2.0f) {
				c.logT = c.t;
				logger::info("CLIMB {:08X} '{}' t={:.1f} dist={:.0f} speed={:.0f} n=({:.2f},{:.2f},{:.2f}) fwd.m={:.2f} stuck={:.2f}", c.id, c.name, c.t, dist3, c.speed, c.n.x, c.n.y, c.n.z, cs, c.stuck);
			}
			return true;
		}

		bool Begin(RE::Actor* a, const V3& a_goal, RE::Actor* a_target, bool a_pursue, bool a_hold)
		{
			if (!On() || !a || !a->Is3DLoaded() || a->IsDead() || FindC(a->GetFormID()) || !SpiderLegs::Has(a->GetFormID()) ||
				Host::Traversing(a->GetFormID())) {
				return false;
			}
			Raycast::Caster r;
			if (!r.Init(a)) {
				return false;
			}
			Crawler c;
			c.handle = a->GetHandle();
			c.id = a->GetFormID();
			c.name = a->GetName() ? a->GetName() : "";
			c.pursue = a_pursue;
			c.hold = a_hold;
			c.goal = a_goal;
			if (a_target) {
				c.target = a_target->GetHandle();
			}
			c.A = c.Av = a->GetPosition();
			const float yaw = a->data.angle.z;
			c.fwd = { std::sin(yaw), std::cos(yaw), 0.0f };
			c.yaw = yaw;
			c.scale = (a->Get3D(false) && a->Get3D(false)->world.scale > 0.05f) ? a->Get3D(false)->world.scale : 1.0f;
			Raycast::Hit hit;
			if (r.CastSolid(c.A + V3{ 0, 0, 30.0f * c.scale }, c.A - V3{ 0, 0, 60.0f * c.scale }, hit) && !Raycast::IsActorRef(hit.ref) && hit.normal.z > 0.3f) {
				c.n = c.nT = Norm(hit.normal, { 0, 0, 1 });
				c.A = c.Av = hit.point;
			}
			c.fwd = OnPlane(c.fwd, c.n, { 0, 1, 0 });
			// v4/v5: target almost straight above (on a bridge, a ledge): there is no direction "toward" it on the floor,
			// so the spider just walked back and forth underneath. Head for the nearest wall, pillar or bank instead
			// (then up it, then over the edge toward the target).
			if (a_target && c.n.z > 0.75f) {
				const V3    td = a_target->GetPosition() - c.A;
				const float tdh = std::hypot(td.x, td.y);
				if (td.z > Config::Get().climb.minRise && tdh < 2.5f * Config::Get().climb.stopDist * c.scale) {
					float best = 1e9f;
					V3    bestDir{}, bestPt{};
					// v5: only WIDE walls count (a river bank, a cliff, a building) - three parallel rays 80u apart must all
					// hit a steep surface at about the same distance. Thin bridge posts are skipped: a post leads up to the
					// underside of the deck, not onto it (Fr2596448yigong2: spider stayed in the river under the bridge).
					const float sc = c.scale;
					auto wallAt = [&](const V3& a_from, const V3& a_dir, float& a_dist, V3& a_pt) {
						Raycast::Hit wh;
						if (!r.CastSolid(a_from, a_from + a_dir * (1500.0f * sc), wh) || Raycast::IsActorRef(wh.ref) || std::fabs(wh.normal.z) > 0.6f) {
							return false;
						}
						a_dist = (wh.point - a_from).Length();
						a_pt = wh.point;
						return true;
					};
					for (int k = 0; k < 16; ++k) {
						const float ang = k * (2.0f * kPi / 16.0f);
						const V3    dir{ std::sin(ang), std::cos(ang), 0.0f };
						const V3    side{ dir.y, -dir.x, 0.0f };
						const V3    from = c.A + V3{ 0, 0, 40.0f * sc };
						float       dm, dl, dr;
						V3          pm, pl, pr;
						if (wallAt(from, dir, dm, pm) && wallAt(from + side * (40.0f * sc), dir, dl, pl) && wallAt(from - side * (40.0f * sc), dir, dr, pr) &&
							std::fabs(dl - dm) < 60.0f * sc && std::fabs(dr - dm) < 60.0f * sc) {
							if (dm < best) {
								best = dm;
								bestDir = dir;
								bestPt = pm;
							}
						}
					}
					if (best < 1e8f) {
						c.fwd = OnPlane(bestDir, c.n, c.fwd);
						c.yaw = std::atan2(c.fwd.x, c.fwd.y);
						c.via = bestPt;
						c.viaOn = true;
						if (Config::Get().climb.log) {
							logger::info("CLIMB {:08X} target {:.0f} above, only {:.0f} away sideways - heading for the nearest wall {:.0f} away", c.id, td.z, tdh, best);
						}
					} else if (Config::Get().climb.log) {
						logger::info("CLIMB {:08X} target {:.0f} above, only {:.0f} away sideways - no wide wall within 1500 to climb", c.id, td.z, tdh);
					}
				}
			}
			if (Config::Get().climb.log) {
				logger::info("CLIMB start {:08X} '{}' {} goal=({:.0f},{:.0f},{:.0f}) dist={:.0f}", c.id, c.name, a_pursue ? "pursue" : "goto", a_goal.x, a_goal.y, a_goal.z,
					(a_goal - c.A).Length());
			}
			Host::Claim(c.id, kClaimSec);
			g_c.push_back(std::move(c));
			return true;
		}

		void Scan(float a_now)
		{
			const auto& cfg = Config::Get().climb;
			auto*       player = RE::PlayerCharacter::GetSingleton();
			auto*       lists = RE::ProcessLists::GetSingleton();
			if (!player || !lists) {
				return;
			}
			const V3 pp = player->GetPosition();
			for (auto& h : lists->highActorHandles) {
				auto  ptr = h.get();
				auto* a = ptr.get();
				if (!a || a == player || a->IsDead() || !a->Is3DLoaded() || !a->IsInCombat()) {
					continue;
				}
				const RE::FormID id = a->GetFormID();
				if (FindC(id) || !SpiderLegs::Has(id)) {
					continue;
				}
				if (auto it = g_cool.find(id); it != g_cool.end() && a_now < it->second) {
					continue;
				}
				auto* tgt = a->GetActorRuntimeData().currentCombatTarget.get().get();
				if (!tgt || tgt->IsDead() || !tgt->Is3DLoaded() || (a->GetPosition() - pp).Length() > 2500.0f) {
					continue;
				}
				const V3    d = tgt->GetPosition() - a->GetPosition();
				const float d3 = d.Length(), dh = std::hypot(d.x, d.y);
				auto&       memo = g_memo[id];
				if (memo.target != tgt->GetFormID()) {
					memo = { tgt->GetFormID(), d3, a_now };
				}
				if (d3 < memo.best - 40.0f) {
					memo.best = d3;
					memo.bestT = a_now;
				}
				bool        off = false;
				if (d3 > 250.0f && !Host::OnNavmesh(a, tgt)) {
					if (memo.offT < 0.0f) {
						memo.offT = a_now;
					}
					off = a_now - memo.offT > 0.6f;
				} else {
					memo.offT = -1.0f;
				}
				// v3: the last crawl from here toward this target failed: don't start the same one again right away.
				if (g_time - memo.failT < cfg.failWait && (a->GetPosition() - memo.failPos).Length() < 300.0f &&
					(tgt->GetPosition() - memo.failTgt).Length() < 300.0f) {
					continue;
				}
				const bool high = d.z >= cfg.minRise && d3 < cfg.engage;
				const bool stuck = a_now - memo.bestT > cfg.stuckSec && d3 < cfg.engage && dh > cfg.stopDist * 1.5f && !Host::Traversing(id);
				// v4: a target high up right above it counts too (it used to need some sideways distance)
				if ((high || stuck || off) && d3 > cfg.stopDist * 1.2f && (dh > cfg.stopDist * 0.8f || high)) {
					if (cfg.log) {
						logger::info("CLIMB trigger {:08X} '{}' {} dz={:.0f} d3={:.0f} noProgress={:.1f}s", id, a->GetName(), high ? "target above" : off ? "off navmesh" : "stuck", d.z, d3, a_now - memo.bestT);
					}
					Begin(a, tgt->GetPosition(), tgt, true, false);
					memo.bestT = a_now;
				}
			}
		}
	}

	bool Enabled() { return On(); }

	bool Active(RE::FormID a_id) { return FindC(a_id) != nullptr; }

	bool Get(RE::FormID a_id, Pose& a_out)
	{
		const Crawler* c = FindC(a_id);
		if (!c) {
			return false;
		}
		a_out.normal = c->n;
		a_out.fwd = c->fwd;
		a_out.anchor = c->Av;
		a_out.offset = c->offset;
		return true;
	}

	bool GoTo(RE::Actor* a_actor, const RE::NiPoint3& a_goal, bool a_hold) { return Begin(a_actor, a_goal, nullptr, false, a_hold); }

	bool Pursue(RE::Actor* a_actor, RE::Actor* a_target) { return a_target && Begin(a_actor, a_target->GetPosition(), a_target, true, false); }

	void Stop(RE::FormID a_id)
	{
		for (auto it = g_c.begin(); it != g_c.end(); ++it) {
			if (it->id == a_id) {
				auto ptr = it->handle.get();
				End(*it, ptr.get(), "stopped");
				g_c.erase(it);
				return;
			}
		}
	}

	void Reset()
	{
		g_c.clear();
		g_grace.clear();
		g_cool.clear();
		g_memo.clear();
		g_endLogs = 0;
	}

	void OnFrame(float a_delta)
	{
		if (a_delta <= 0.0f || a_delta > 0.5f) {
			return;
		}
		g_time += a_delta;
		if (!On()) {
			for (auto& c : g_c) {
				auto ptr = c.handle.get();
				End(c, ptr.get(), "disabled");
			}
			g_c.clear();
			return;
		}
		{
			g_others.clear();
			auto* player = RE::PlayerCharacter::GetSingleton();
			auto* lists = RE::ProcessLists::GetSingleton();
			if (player && lists && !g_c.empty()) {
				g_others.push_back({ player->GetPosition(), (std::max)(player->GetBoundRadius(), 25.0f), player->GetFormID() });
				for (auto& h : lists->highActorHandles) {
					auto  ptr = h.get();
					auto* o = ptr.get();
					if (!o || o == player || o->IsDead() || !o->Is3DLoaded()) {
						continue;
					}
					g_others.push_back({ o->GetPosition(), (std::max)(o->GetBoundRadius(), 25.0f), o->GetFormID() });
				}
			}
			if (Config::Get().climb.log && !g_c.empty() && g_time - g_statT > 5.0f) {
				g_statT = g_time;
				logger::info("CLIMBSTAT crawlers={} probes={} rejected: backface={} insideStart={} notExposed={} tunnel={}", g_c.size(), g_probes, g_backface, g_inside, g_hidden, g_tunnel);
				g_probes = g_backface = g_inside = g_hidden = g_tunnel = 0;
				for (int l = 0; l < 64; ++l) {
					if (Raycast::g_skipped[l]) {
						logger::info("CLIMBSKIP layer {} ({}) skipped {} ray hits (not a surface)", l, RE::CollisionLayerToString(static_cast<RE::COL_LAYER>(l)), Raycast::g_skipped[l]);
						Raycast::g_skipped[l] = 0;
					}
				}
			}
		}
		for (auto it = g_c.begin(); it != g_c.end();) {
			if (UpdateOne(*it, a_delta)) {
				++it;
			} else {
				it = g_c.erase(it);
			}
		}
		for (auto it = g_grace.begin(); it != g_grace.end();) {
			auto* ref = RE::TESForm::LookupByID<RE::Actor>(it->id);
			if (g_time > it->until || !ref || ref->IsDead()) {
				it = g_grace.erase(it);
				continue;
			}
			if (auto* cc = ref->GetCharController()) {
				cc->fallStartHeight = ref->GetPosition().z;
				cc->fallTime = 0.0f;
			}
			++it;
		}
		if (g_time - g_scanT > 0.3f) {
			g_scanT = g_time;
			Scan(g_time);
		}
	}
}
