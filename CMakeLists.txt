cmake_minimum_required(VERSION 3.23 FATAL_ERROR)
project(sumo-sim-data-publisher VERSION 0.1.0 LANGUAGES CXX) # ${PROJECT_NAME}

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

# If compiler is clang or gcc
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang" OR CMAKE_CXX_COMPILER_ID MATCHES "GNU")
    # Enable compiler warnings
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

message(STATUS "$SUMO_HOME: $ENV{SUMO_HOME}")

# Force compiler to output in color when you use ninja as generator
# https://stackoverflow.com/questions/73349743/ninja-build-system-gcc-clang-doesnt-output-diagnostic-colors
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fdiagnostics-color=always")

# Conan is used for dependency management
set(external_library_targets)
find_package(fmt REQUIRED)
list(APPEND external_library_targets fmt::fmt)
# find_package(uwebsockets REQUIRED)
# list(APPEND external_library_targets uwebsockets::uwebsockets)
find_package(spdlog REQUIRED)
list(APPEND external_library_targets spdlog::spdlog)
find_package(argparse REQUIRED)
list(APPEND external_library_targets argparse::argparse)
find_package(nlohmann_json REQUIRED)
list(APPEND external_library_targets nlohmann_json::nlohmann_json)
find_package(pugixml REQUIRED)
list(APPEND external_library_targets pugixml::pugixml)
find_package(phmap REQUIRED)
list(APPEND external_library_targets phmap)
find_package(tl-expected REQUIRED)
list(APPEND external_library_targets tl::expected)
find_package(indicators REQUIRED)
list(APPEND external_library_targets indicators::indicators)
find_package(cppzmq REQUIRED)
list(APPEND external_library_targets cppzmq)
find_package(tomlplusplus REQUIRED)
list(APPEND external_library_targets tomlplusplus::tomlplusplus)
find_package(bshoshany-thread-pool REQUIRED)
list(APPEND external_library_targets bshoshany-thread-pool::bshoshany-thread-pool)
find_package(SQLite3 REQUIRED)
list(APPEND external_library_targets SQLite::SQLite3)
# find_package(date REQUIRED)


message(STATUS "external_library_targets:")
foreach(external_library_target ${external_library_targets})
    message(STATUS "  ${external_library_target}")
endforeach()

add_library(streetlamp STATIC src/streetlamp.cpp)
target_link_libraries(streetlamp PRIVATE ${external_library_targets})

add_executable(${PROJECT_NAME} src/sumo-sim-data-publisher.cpp)
target_link_libraries(${PROJECT_NAME} PRIVATE streetlamp)
target_link_libraries(${PROJECT_NAME} PRIVATE ${external_library_targets})
# Link with SUMO's libtraci
# g++ -o test -std=c++11 -I$SUMO_HOME/src test.cpp -L$SUMO_HOME/bin -ltracicpp
target_include_directories(${PROJECT_NAME} PRIVATE $ENV{SUMO_HOME}/src) # Equivalent to -I$SUMO_HOME/src
if (WIN32)
    target_include_directories(${PROJECT_NAME} PRIVATE $ENV{SUMO_HOME}/tools/include)
endif()

# target_include_directories(${PROJECT_NAME} PRIVATE )
# target_link_libraries(${PROJECT_NAME} PRIVATE $ENV{SUMO_HOME}/tools/libtraci/_libtraci.so) # Equivalent to -ltracicpp
if (WIN32)
else()
    target_link_libraries(${PROJECT_NAME} PRIVATE tracicpp)
endif()

target_link_directories(${PROJECT_NAME} PRIVATE $ENV{SUMO_HOME}/bin) # Equivalent to -L$SUMO_HOME/bin

add_executable(zmq-client-demo src/zmq-client-demo.cpp)
target_link_libraries(zmq-client-demo PRIVATE ${external_library_targets})

add_executable(parse-streetlamps-from-osm src/parse-streetlamps-from-osm.cpp)
target_link_libraries(parse-streetlamps-from-osm PRIVATE ${external_library_targets})


add_executable(streetlamp-timeseries-server src/streetlamp-timeseries-server.cpp)
target_link_libraries(streetlamp-timeseries-server PRIVATE streetlamp)
target_link_libraries(streetlamp-timeseries-server PRIVATE ${external_library_targets})

find_package(Catch2 REQUIRED)
add_executable(test-ringbuf tests/ringbuf.cpp)
target_include_directories(test-ringbuf PRIVATE src)
target_link_libraries(test-ringbuf PRIVATE Catch2::Catch2WithMain)
