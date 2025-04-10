cmake_minimum_required(VERSION 2.8...3.23.3)

# C++11 Standard verwenden
set(CMAKE_CXX_STANDARD 11)

# zusaetzliche Compiler-Optionen
add_compile_options(-O0 -pedantic -Wfatal-errors -Wall)
add_compile_options(-Wextra -Wshadow -Wconversion -Wno-unused)

# Set the project name
project(c64_tap_tool)

# Add the executable
add_executable(c64_tap_tool main.cpp command_line_class.cpp command_line_class.h)