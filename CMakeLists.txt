cmake_minimum_required(VERSION 3.16)

project(jopa6 VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Automatically run moc, uic, and rcc when needed.
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

# Generate compile_commands.json for IDE/tooling integration.
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(Qt5 REQUIRED COMPONENTS Widgets)

add_executable(jopa6
    src/main.cpp
    src/mainwindow.cpp
    src/mainwindow.h
)

target_link_libraries(jopa6 PRIVATE Qt5::Widgets)
