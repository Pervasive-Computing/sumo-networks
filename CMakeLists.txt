cmake_minimum_required(VERSION 3.23 FATAL_ERROR)
project(sumo-network-sim VERSION 0.1.0 LANGUAGES CXX) # ${PROJECT_NAME}

# Set default build type to release
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Choose Release or Debug" FORCE)
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# create compile_commands.json
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Force compiler to output in color when you use ninja as generator
# https://stackoverflow.com/questions/73349743/ninja-build-system-gcc-clang-doesnt-output-diagnostic-colors
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fdiagnostics-color=always")


# Conan is used for dependency management
find_package(fmt REQUIRED)
# find_package(cxxopts REQUIRED)
find_package(uwebsockets REQUIRED)
find_package(spdlog REQUIRED)
find_package(argparse REQUIRED)

set(external_library_targets fmt::fmt uwebsockets::uwebsockets spdlog::spdlog argparse::argparse)

# add_executable(${PROJECT_NAME} traci-sim.cpp)

add_executable(uwebsockets-test uwebsockets.cpp)
target_link_libraries(uwebsockets-test PRIVATE ${external_library_targets})


# include(FetchContent)

# # Install raylib

# set(RAYLIB_VERSION 4.5.0)

# FetchContent_Declare(
# 				raylib
# 				DOWNLOAD_EXTRACT_TIMESTAMP OFF
# 				URL https://github.com/raysan5/raylib/archive/refs/tags/${RAYLIB_VERSION}.tar.gz
# 		)
# FetchContent_GetProperties(raylib)
# if (NOT raylib_POPULATED) # Have we downloaded raylib yet?
#   set(FETCHCONTENT_QUIET NO)
#   FetchContent_Populate(raylib)
#   set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE) # don't build the supplied examples
#   add_subdirectory(${raylib_SOURCE_DIR} ${raylib_BINARY_DIR})
# endif()

# target_link_libraries(${PROJECT_NAME} PRIVATE raylib)
