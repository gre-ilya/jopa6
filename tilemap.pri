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

INCLUDEPATH += $$PWD/src
DEPENDPATH  += $$PWD/src

HEADERS += \
    $$PWD/src/geopoint.h \
    $$PWD/src/mercator.h \
    $$PWD/src/tilemaprenderer.h

SOURCES += \
    $$PWD/src/tilemaprenderer.cpp
