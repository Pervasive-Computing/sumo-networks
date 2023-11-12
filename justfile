builddir := "build"

default:
    @just --list

alias i := install-dependencies
install-dependencies:
    conan install . --output-folder={{builddir}} --build=missing

alias c := configure
configure:
    cmake -S . -B {{builddir}} -DCMAKE_TOOLCHAIN_FILE=conan_toolchain.cmake -DCMAKE_BUILD_TYPE=Release -G Ninja

alias b := build
build:
    cmake --build {{builddir}}


doit: install-dependencies configure build
