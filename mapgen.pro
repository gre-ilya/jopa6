# Standalone build of the mapgen command-line tool with qmake.
#
#     qmake && make
#
# The reusable renderer comes from tilemap.pri; this .pro only adds the
# CLI-specific parts (argument parsing and the CSV/JSON/GeoJSON loader).

TEMPLATE = app
TARGET = mapgen

QT -= widgets
CONFIG += c++17 console
CONFIG -= app_bundle

include($$PWD/tilemap.pri)

HEADERS += $$PWD/src/pointsio.h

SOURCES += \
    $$PWD/src/main.cpp \
    $$PWD/src/pointsio.cpp
