#!/usr/bin/env bash
# Builds a release zip from the ci-out branch. Every release and test build ships the INI; refuses otherwise.
# Usage: scripts/package.sh <output.zip>
set -euo pipefail
out="$(realpath -m "$1")"
repo="$(cd "$(dirname "$0")/.." && pwd)"
tmp="$(mktemp -d)"
git -C "$repo" fetch -q origin ci-out
git -C "$repo" archive origin/ci-out SKSE LICENSE NOTICE.md THIRD_PARTY_NOTICES.md CommonLibSSE-NG-EXCEPTIONS.md | tar -x -C "$tmp"
for f in SKSE/Plugins/FluidMotionSpiders.dll SKSE/Plugins/FluidMotionSpiders.ini; do
  [ -f "$tmp/$f" ] || { echo "MISSING $f - not packaging" >&2; exit 1; }
done
rm -f "$out"
(cd "$tmp" && zip -qr "$out" .)
rm -rf "$tmp"
echo "packaged $out (source $(git -C "$repo" show origin/ci-out:source-commit.txt | tr -cd '0-9a-f'))"
