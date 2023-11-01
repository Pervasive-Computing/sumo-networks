cmake_minimum_required(VERSION 3.27.5 FATAL_ERROR)
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

add_executable(${PROJECT_NAME} traci-sim.cpp)

include(FetchContent)

# Install raylib

set(RAYLIB_VERSION 4.5.0)

FetchContent_Declare(
				raylib
				DOWNLOAD_EXTRACT_TIMESTAMP OFF
				URL https://github.com/raysan5/raylib/archive/refs/tags/${RAYLIB_VERSION}.tar.gz
		)
FetchContent_GetProperties(raylib)
if (NOT raylib_POPULATED) # Have we downloaded raylib yet?
  set(FETCHCONTENT_QUIET NO)
  FetchContent_Populate(raylib)
  set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE) # don't build the supplied examples
  add_subdirectory(${raylib_SOURCE_DIR} ${raylib_BINARY_DIR})
endif()

target_link_libraries(${PROJECT_NAME} PRIVATE raylib)
