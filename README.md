# FluidMotionSpiders

Procedural IK legs and wall / ceiling crawling for frostbite and dwarven spiders.
Split out of Fluid Motion: Pathfinding Overhaul (megakuutti, GPL-3.0).

**Requires FluidMotionNPC** (v6 or newer, API v1). Without it the plugin logs an error and does nothing.

- Every leg is placed on the ground by IK and steps on its own; the body tilts with the terrain.
- In combat a spider whose target is above it, out of navmesh reach, or that is stuck climbs walls and ceilings.
- Omni movement (on by default): spiders strafe and back up. Overrides spider movement speeds from other mods while on.
- Skeleton fit check: spider skeletons that don't match vanilla keep vanilla legs.
- `sExcludeRaces` in the INI leaves chosen races alone.
- FluidMotionNPC leaves a crawling spider alone (API claim), so the two never fight over it.

Supported runtimes: Skyrim SE 1.5.97, AE 1.6.1170, 1.7.99, 1.7.104, Skyrim VR 1.4.15.

Built by GitHub Actions (`.github/workflows/build.yml`); output is published to the `ci-out` branch.
