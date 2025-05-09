cmake_minimum_required(VERSION 3.28)
project(gtow-equity-calc LANGUAGES CXX)

# Set a default build type if none was specified
# https://blog.kitware.com/cmake-and-the-default-build-type/
set(default_build_type "Release")  # "Debug" / "Release" / "MinSizeRel" / "RelWithDebInfo"
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    message(STATUS "Setting build type to '${default_build_type}' as none was specified.")
    set(CMAKE_BUILD_TYPE "${default_build_type}" CACHE STRING "Choose the type of build." FORCE)
endif()

# Set the C++ standard to C++23
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Generate position-independent code (-fPIC / -fPIE)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Set the visibility of symbols to hidden by default (-fvisibility=hidden)
# pybind11 sets the visibility to hidden, and clang complains if the visibility differs
set(CMAKE_CXX_VISIBILITY_PRESET hidden)

# Enable colored diagnostics for Ninja
if(CMAKE_GENERATOR STREQUAL "Ninja")
    set(CMAKE_COLOR_DIAGNOSTICS ON)
    add_link_options($<IF:$<STREQUAL:$<CXX_COMPILER_ID>,GNU>,-fdiagnostics-color=always,-fcolor-diagnostics>)
endif()

# Add the pybind11 submodule
execute_process(COMMAND git submodule update --init --recursive)
add_subdirectory(pybind11 EXCLUDE_FROM_ALL SYSTEM)

# Add compiler warning flags
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    add_compile_options(-Wall -Wextra -Wnon-virtual-dtor -pedantic-errors)
endif()

# Python bindings
pybind11_add_module(_core src/eqcalc.cc)
set_target_properties(_core PROPERTIES OUTPUT_NAME "eqcalc._core")

# Update timestamp when successfully configured
message(STATUS "Updating .timestamp")
file(TOUCH "${CMAKE_BINARY_DIR}/.timestamp")
