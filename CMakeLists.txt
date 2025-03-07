cmake_minimum_required(VERSION 3.10)
# setze CMake Version 3.0 voraus
cmake_minimum_required(VERSION 3.0)

# C++11 Standard verwenden
set(CMAKE_CXX_STANDARD 11)

# zusaetzliche Compiler-Optionen
add_compile_options(-O0 -pedantic -Wfatal-errors -Wall)
add_compile_options(-Wextra -Wshadow -Wconversion -Wno-unused)

# Set the project name
project(c64_tap_tool)

# Add the executable
add_executable(c64_tap_tool main.cpp)