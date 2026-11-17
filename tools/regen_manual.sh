#!/usr/bin/env bash
# Regenerates modules/tracy/<version>/overlay/profiler/generated/tracy.md, the
# user manual embedded in tracy-profiler.
#
# Upstream generates it at build time from manual/tracy.tex with a pinned
# pandoc and manual/latex2md.py (cmake/manual.cmake). Here it is generated once
# per Tracy version and checked in, so the Bazel build needs neither pandoc
# nor Python. SOURCE_DATE_EPOCH is pinned to the release date, which makes the
# output reproducible (the manual's \today otherwise changes daily).
#
# Usage: tools/regen_manual.sh   (Linux or macOS, x86_64 or arm64; needs
#                                 curl, tar/unzip, python3 and a fetched @tracy)
set -euo pipefail

TRACY_VERSION=0.15.0
PANDOC_VERSION=3.12.1
# Tracy v0.15.0 release date (2026-11-05T20:57:35Z).
export SOURCE_DATE_EPOCH=1793912255

root="$(cd "$(dirname "$0")/.." && pwd)"
out="$root/modules/tracy/$TRACY_VERSION/overlay/profiler/generated/tracy.md"

case "$(uname -s)-$(uname -m)" in
  Linux-x86_64) asset=pandoc-$PANDOC_VERSION-linux-amd64.tar.gz
    sha=d0c90410e90204c9ca83b8539fac5c7aed01fd537207e4585849f8abc5df20b8 ;;
  Linux-aarch64) asset=pandoc-$PANDOC_VERSION-linux-arm64.tar.gz
    sha=445d96fd08801fe636cab1158b4f017ee320ac3b7446328b4fe2f902117b19b9 ;;
  Darwin-arm64) asset=pandoc-$PANDOC_VERSION-arm64-macOS.zip
    sha=c2146cf33a583f3ac93eccddf1bb662371d5cfcce1aad1c87ad860cca1435d46 ;;
  Darwin-x86_64) asset=pandoc-$PANDOC_VERSION-x86_64-macOS.zip
    sha=81ed3e2cfecf096df15d407152f89a684fd65221082b919963f738e977fad3bc ;;
  *) echo "unsupported host $(uname -s)-$(uname -m)" >&2; exit 1 ;;
esac

work="$(mktemp -d)"
trap 'rm -rf "$work"' EXIT
curl -sfL "https://github.com/jgm/pandoc/releases/download/$PANDOC_VERSION/$asset" -o "$work/$asset"
echo "$sha  $work/$asset" | shasum -a 256 -c - >/dev/null
case "$asset" in
  *.tar.gz) tar -xzf "$work/$asset" -C "$work" ;;
  *.zip) unzip -q "$work/$asset" -d "$work" ;;
esac
pandoc="$(find "$work" -type f -name pandoc -perm -u+x | head -1)"

# Use the exact upstream sources Bazel fetched for @tracy.
(cd "$root" && bazel build @tracy//:LICENSE >/dev/null 2>&1 || true)
src="$(cd "$root" && bazel info output_base)/external/tracy+"
python3 -I "$src/manual/latex2md.py" \
  --tex "$src/manual/tracy.tex" \
  --out "$out" \
  --pandoc "$pandoc" \
  --icons "$src/profiler/src/profiler/IconsFontAwesome7.h"
echo "wrote $out"
echo "now run: python3 tools/sync_overlay.py tracy $TRACY_VERSION && bazel shutdown"
