# tilemap.pri — the reusable route-rendering module for qmake projects.
#
# Integrate it into a host project by including this file from your .pro:
#
#     include(/path/to/jopa6/tilemap.pri)
#
# It contributes the TileMapRenderer sources/headers, adds src/ to the
# include path and pulls in the Qt Gui module (QImage / QPainter / QFont).
# Paths are anchored with $$PWD so the location of the host .pro does not
# matter.
#
# This module deliberately excludes the CLI-only parts (pointsio, main.cpp):
# the host program is expected to build a GeoPath itself and call
# TileMapRenderer::render() directly.

QT += gui
CONFIG += c++17

# MSVC-style compilers interpret source files in the system code page unless
# told otherwise, which mangles any non-ASCII string literal. The module's own
# sources are pure ASCII, but enforce UTF-8 so a host project that adds
# Cyrillic/UTF-8 literals (e.g. point names) stays correct. This covers both
# cl.exe (win32-msvc) and the Clang/MSVC kit (win32-clang-msvc, e.g. clang-cl
# in Qt Creator). No effect on GCC or MinGW/Clang (GNU) targets.
win32-msvc*|win32-clang-msvc {
    QMAKE_CXXFLAGS += /utf-8
}

INCLUDEPATH += $$PWD/src
DEPENDPATH  += $$PWD/src

HEADERS += \
    $$PWD/src/geopoint.h \
    $$PWD/src/mercator.h \
    $$PWD/src/tilemaprenderer.h

SOURCES += \
    $$PWD/src/tilemaprenderer.cpp
