cmake_minimum_required(VERSION 3.16)

project(jopa6 VERSION 0.2.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Automatically run moc when needed (cheap, harmless even without Q_OBJECT).
set(CMAKE_AUTOMOC ON)

# Generate compile_commands.json for IDE/tooling integration.
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Default to an optimised build when the user didn't pick a configuration.
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# Only the GUI module is needed: QImage / QPainter / QFont, no widgets.
find_package(Qt5 REQUIRED COMPONENTS Gui)

# --- Reusable rendering library (the part meant to be embedded elsewhere) ---
add_library(tilemap STATIC
    src/tilemaprenderer.cpp
    src/tilemaprenderer.h
    src/mercator.h
    src/geopoint.h
)
target_include_directories(tilemap PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(tilemap PUBLIC Qt5::Gui)

# Compile sources as UTF-8 under MSVC so non-ASCII string data is correct.
# PUBLIC so the flag propagates to anything linking the module. No-op elsewhere.
# NB: do NOT add /permissive- here — Qt 5.15's qvector.h/qlist.h rely on the
# MSVC 'stdext' extension and fail to compile under strict conformance mode.
target_compile_options(tilemap PUBLIC
    $<$<CXX_COMPILER_ID:MSVC>:/utf-8>
)

# --- Command-line test harness ---------------------------------------------
add_executable(mapgen
    src/main.cpp
    src/pointsio.cpp
    src/pointsio.h
)
target_link_libraries(mapgen PRIVATE tilemap Qt5::Gui)

# --- Optional test/helper tools --------------------------------------------
option(BUILD_TESTING "Build test and helper tools (e.g. the tile generator)" ON)
if(BUILD_TESTING)
    add_executable(gentiles
        tests/gen_test_tiles.cpp
        src/pointsio.cpp
        src/pointsio.h
    )
    target_link_libraries(gentiles PRIVATE tilemap Qt5::Gui)
endif()
