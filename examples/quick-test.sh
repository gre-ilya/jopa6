#!/usr/bin/env bash
#
# quick-test.sh — build the program and render an example route end-to-end,
# WITHOUT needing any real map tiles or a network connection.
#
# It builds mapgen + gentiles with qmake (into a throwaway ./.quicktest dir),
# uses gentiles to synthesise placeholder tiles for the chosen points, then
# renders the route with mapgen. The output image is opened if possible.
#
# Usage:
#   examples/quick-test.sh                         # default: examples/points.json
#   examples/quick-test.sh examples/points.csv     # any CSV / JSON / GeoJSON file
#   ZOOM=14 examples/quick-test.sh examples/points.geojson
#
# To test against YOUR real tiles instead of synthetic ones, skip this script
# and run mapgen directly (see README), e.g.:
#   ./mapgen --tiles /path/to/your/tiles --points examples/points.json --out route.png

set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD="${ROOT}/.quicktest"
TILES="${BUILD}/tiles"
POINTS="${1:-${ROOT}/examples/points.json}"
OUT="${2:-${BUILD}/route.png}"
ZOOM="${ZOOM:-15}"

# Run headless so it works over SSH / on a server with no display.
export QT_QPA_PLATFORM="${QT_QPA_PLATFORM:-offscreen}"

JOBS="$(nproc 2>/dev/null || echo 2)"
mkdir -p "${BUILD}/mapgen" "${BUILD}/gentiles"

echo ">> [1/3] Building mapgen + gentiles with qmake (${JOBS} jobs)..."
( cd "${BUILD}/mapgen"   && qmake "${ROOT}/mapgen.pro"   >/dev/null && make -j"${JOBS}" >/dev/null )
( cd "${BUILD}/gentiles" && qmake "${ROOT}/gentiles.pro" >/dev/null && make -j"${JOBS}" >/dev/null )

echo ">> [2/3] Synthesising placeholder tiles (zoom ${ZOOM}) for ${POINTS}..."
"${BUILD}/gentiles/gentiles" --points "${POINTS}" --tiles "${TILES}" --zoom "${ZOOM}"

echo ">> [3/3] Rendering route -> ${OUT}..."
"${BUILD}/mapgen/mapgen" --tiles "${TILES}" --points "${POINTS}" --out "${OUT}"

echo
echo "Done. Result: ${OUT}"
if command -v xdg-open >/dev/null 2>&1 && [ -n "${DISPLAY:-}" ]; then
    xdg-open "${OUT}" >/dev/null 2>&1 || true
fi
