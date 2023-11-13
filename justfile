builddir := "build"
buildmode := "Release"
generator := "Ninja"

default:
    @just --list

alias i := install-dependencies
install-dependencies:
    conan install . --output-folder={{builddir}} --build=missing

alias c := configure
configure:
    cmake -S . -B {{builddir}} -DCMAKE_TOOLCHAIN_FILE=conan_toolchain.cmake -DCMAKE_BUILD_TYPE={{buildmode}} -G {{generator}}

alias b := build
build:
    cmake --build {{builddir}}

[unix]
doit: install-dependencies configure build
    # @which timg >/dev/null && timg -C "https://external-content.duckduckgo.com/iu/?u=https%3A%2F%2Fcdn.wallpapersafari.com%2F53%2F76%2FNpMC7D.jpg&f=1&nofb=1&ipt=267cb65dd2b78c1a9afc0db16085b2e70695316f869ab25c9052b92d3ddf5655&ipo=images"
    @which timg >/dev/null && timg -C "https://media2.giphy.com/media/87xihBthJ1DkA/giphy.gif?cid=ecf05e47ff7awhi7f41d2zj6xugilu9lpk5o2kkj0n1ksric&ep=v1_gifs_search&rid=giphy.gif&ct=g"
    # @which timg >/dev/null && timg -C "https://media0.giphy.com/media/jndc0TQq9fvK8/giphy.gif?cid=ecf05e473mk06dd25ougu93dcqfvjbtkxrh1iptthlijt40z&ep=v1_gifs_related&rid=giphy.gif&ct=g"
