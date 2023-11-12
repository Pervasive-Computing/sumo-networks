#include <iostream>
// #include <libsumo/Simulation.h>
#include <libsumo/libtraci.h>

using namespace libtraci;

auto main(int argc, char **argv) -> int {


    const int port = 10000;
    Simulation::init(port, 5, "localhost");
//   Simulation::start({"sumo-gui", "-c", "esbjerg/esbjerg.sumocfg"});
  const double dt = Simulation::getDeltaT();
  for (int i = 0; i < 500000; ++i) {
    Simulation::step();
    //   std::this_thread::sleep_for(100ms);
  }

  Simulation::close();

  std::cerr << "Success!";
  return 0;
}
