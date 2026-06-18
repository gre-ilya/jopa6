#!/usr/bin/env bash
# SessionStart hook: ensure the Qt5 C++ toolchain is available.
#
# The web execution environment is ephemeral, so build dependencies must be
# (re)installed at the start of every session. This script is idempotent: if
# Qt5 is already present it exits quickly.
set -euo pipefail

if pkg-config --exists Qt5Widgets 2>/dev/null && command -v cmake >/dev/null 2>&1; then
    echo "Qt5 toolchain already present ($(pkg-config --modversion Qt5Core 2>/dev/null))."
    exit 0
fi

echo "Installing Qt5 C++ toolchain..."
export DEBIAN_FRONTEND=noninteractive

# Some unrelated third-party PPAs may be unsigned; don't let them fail the run.
sudo apt-get update -qq 2>/dev/null || true

sudo apt-get install -y -qq \
    build-essential \
    cmake \
    qtbase5-dev \
    qttools5-dev \
    qttools5-dev-tools \
    qt5-qmake

echo "Qt5 toolchain installed: $(pkg-config --modversion Qt5Core 2>/dev/null)"
