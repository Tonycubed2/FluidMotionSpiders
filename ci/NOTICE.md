# NOTICE

FluidMotionSpiders is a modified version of **Fluid Motion: Pathfinding Overhaul** by **megakuutti**,
released under the GNU General Public License v3.0 (see `LICENSE`).

Original work Copyright (C) megakuutti.
Modifications Copyright (C) 2026 Tony Torres (tonycubed2).

## What was changed

- Only the spider features were kept: procedural IK legs for frostbite and dwarven spiders and
  wall / ceiling crawling in combat. Everything else lives in FluidMotionNPC or was removed.
- Built as its own SKSE plugin that requires FluidMotionNPC and talks to it through FluidMotionNPC's
  public API (traversal state, claim, post-graph and frame callbacks) instead of linking into it.
- Wall crawling is part of the spider feature (no separate toggle).
- Added a skeleton fit check (non-vanilla spider skeletons keep vanilla legs) and a race exclusion list.
- Removed the AE-only pre-render hook, crash breadcrumbs and diagnostic probes.
- Builds for Skyrim SE 1.5.97, AE 1.6.1170 / 1.7.99 / 1.7.104 and Skyrim VR 1.4.15.

## Third-party code

Built with CommonLibSSE-NG (GPL-3.0-or-later WITH Modding Exception AND GPL-3.0 Linking Exception);
see `CommonLibSSE-NG-EXCEPTIONS.md` and `THIRD_PARTY_NOTICES.md`.
