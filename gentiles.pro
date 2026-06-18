# Standalone build of the gentiles test helper with qmake.
#
#     qmake gentiles.pro && make
#
# Generates synthetic {z}/{x}/{y}.png tiles so the renderer can be exercised
# without a real tile cache. Reuses the projection math from tilemap.pri and
# the points loader from the CLI.

TEMPLATE = app
TARGET = gentiles

QT -= widgets
CONFIG += c++17 console
CONFIG -= app_bundle

include($$PWD/tilemap.pri)

HEADERS += $$PWD/src/pointsio.h

SOURCES += \
    $$PWD/tests/gen_test_tiles.cpp \
    $$PWD/src/pointsio.cpp
