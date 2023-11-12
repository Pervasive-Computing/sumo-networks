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

# Check that $SUMO_HOME is set
if(DEFINED $ENV{SUMO_HOME})
    message(FATAL_ERROR "Environment variable SUMO_HOME is not set")
endif()

# Check that $SUMO_HOME points to a directory that exists on the disk
if(NOT EXISTS $ENV{SUMO_HOME})
    message(FATAL_ERROR "Environment variable SUMO_HOME does not point to a valid directory")
endif()

message(STATUS "$SUMO_HOME: $ENV{SUMO_HOME}")

# Force compiler to output in color when you use ninja as generator
# https://stackoverflow.com/questions/73349743/ninja-build-system-gcc-clang-doesnt-output-diagnostic-colors
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fdiagnostics-color=always")

# Conan is used for dependency management
find_package(fmt REQUIRED)

# find_package(cxxopts REQUIRED)
find_package(uwebsockets REQUIRED)
find_package(spdlog REQUIRED)
find_package(argparse REQUIRED)
find_package(nlohmann_json REQUIRED)
find_package(pugixml REQUIRED)
find_package(phmap REQUIRED)
find_package(tl-expected REQUIRED)
find_package(indicators REQUIRED)

set(external_library_targets fmt::fmt uwebsockets::uwebsockets spdlog::spdlog argparse::argparse nlohmann_json::nlohmann_json pugixml::pugixml phmap tl::expected indicators::indicators)

add_executable(${PROJECT_NAME} uwebsockets.cpp)
target_link_libraries(${PROJECT_NAME} PRIVATE ${external_library_targets})

# Link with SUMO's libtraci
# g++ -o test -std=c++11 -I$SUMO_HOME/src test.cpp -L$SUMO_HOME/bin -ltracicpp
target_include_directories(${PROJECT_NAME} PRIVATE $ENV{SUMO_HOME}/src) # Equivalent to -I$SUMO_HOME/src

# target_include_directories(${PROJECT_NAME} PRIVATE )
# target_link_libraries(${PROJECT_NAME} PRIVATE $ENV{SUMO_HOME}/tools/libtraci/_libtraci.so) # Equivalent to -ltracicpp
target_link_libraries(${PROJECT_NAME} PRIVATE tracicpp)
target_link_directories(${PROJECT_NAME} PRIVATE $ENV{SUMO_HOME}/bin) # Equivalent to -L$SUMO_HOME/bin


target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra -Wpedantic)
