#include <iostream>
#include <libsumo/libtraci.h>

using namespace libtraci;

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cout << "usage: traci <sumocfg>" << std::endl;
        return 1;
    }
    Simulation::start({"sumo-gui", "-c", argv[1]});
    for (int i = 0; i < 10000; i++) {
        Simulation::step();
    }

    Simulation::close();
}
