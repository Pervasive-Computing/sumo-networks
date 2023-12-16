builddir := "build"
buildmode := "Release"
generator := "Ninja"
cxx := "/usr/bin/g++"

alias c := configure
alias i := install-dependencies
alias b := build

default: && system-info
    just --list
    just --variables

system-info:
	@echo "This is an {{arch()}} machine".
	@echo "It has {{num_cpus()}} logical CPUs"
	@echo "Operating System is {{os()}}"

install-dependencies:
    conan install . --output-folder={{builddir}} --build=missing

[unix]
configure:
    cmake -S . -B {{builddir}} -DCMAKE_TOOLCHAIN_FILE="conan_toolchain.cmake" -DCMAKE_BUILD_TYPE={{buildmode}} -DCMAKE_CXX_COMPILER={{cxx}} -G {{generator}}

[windows]
configure:
    cmake -S . -B {{builddir}} -DCMAKE_TOOLCHAIN_FILE="conan_toolchain.cmake" -DCMAKE_BUILD_TYPE={{buildmode}}

build:
    cmake --build {{builddir}} --config {{buildmode}}

doit: install-dependencies configure build
