# jopa6

A Qt5 / C++ desktop application skeleton, built with CMake.

## Requirements

- C++17 compiler (g++ / clang)
- CMake >= 3.16
- Qt5 (Widgets module) — `qtbase5-dev`

### Installing dependencies (Debian/Ubuntu)

```bash
sudo apt-get update
sudo apt-get install -y build-essential cmake qtbase5-dev qttools5-dev qttools5-dev-tools qt5-qmake
```

## Building

```bash
cmake -S . -B build
cmake --build build
```

## Running

```bash
./build/jopa6
```

> The app is a GUI program. On a headless machine run it under a virtual
> display, e.g. `xvfb-run ./build/jopa6`.

## Project layout

```
.
├── CMakeLists.txt        # Build configuration
├── src/
│   ├── main.cpp          # Application entry point
│   ├── mainwindow.h      # Main window declaration
│   └── mainwindow.cpp    # Main window implementation
└── .claude/              # Claude Code on the web config + SessionStart hook
```
