#include <chrono>
#include <cmath>
// #include <libsumo/Simulation.h>
// #include <libsumo/Vehicle.h>
using namespace std::chrono_literals;

#include <filesystem>
namespace fs = std::filesystem;
#include <functional>
#include <iostream>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>

#include "debug-macro.hpp"
#include "pretty-printers.hpp"

auto walkdir(
    const std::string &dir,
    const std::function<bool(const fs::directory_entry &)> &filter =
        [](const auto &_) { return true; }) -> std::vector<std::string> {
  std::vector<std::string> files;
  for (const auto &entry : fs::directory_iterator(dir)) {
    if (!filter(entry)) {
      continue;
    }
    files.push_back(entry.path().string());
  }
  return files;
}

// 3rd party libraries
#include <argparse/argparse.hpp>
#include <fmt/core.h>
#include <nlohmann/json.hpp>
#include <parallel_hashmap/phmap.h>
#include <pugixml.hpp>
#include <spdlog/spdlog.h>
#include <uWebSockets/App.h>

// for convenience
using json = nlohmann::json;
using namespace nlohmann::literals; // for ""_json

#include <libsumo/libtraci.h>
using namespace libtraci;

using u8 = std::uint8_t;
using u16 = std::uint16_t;
using u32 = std::uint32_t;
using u64 = std::uint64_t;

using i8 = std::int8_t;
using i16 = std::int16_t;
using i32 = std::int32_t;
using i64 = std::int64_t;

using f32 = float;
using f64 = double;

template <typename T> using Vec = std::vector<T>;
using String = std::string;

template <typename T> using Option = std::optional<T>;

struct Car {
  int x;
  int y;
  double heading;
  bool alive = false;
};

auto main(int argc, char **argv) -> int {
  //   spdlog::info("Welcome to spdlog!");
  // spdlog::error("Some error message with arg: {}", 1);

  // spdlog::warn("Easy padding in numbers like {:08d}", 12);
  // spdlog::critical("Support for int: {0:d};  hex: {0:x};  oct: {0:o}; bin:
  // {0:b}", 42); spdlog::info("Support for floats {:03.2f}", 1.23456);
  // spdlog::info("Positional args are {1} {0}..", "too", "supported");
  // spdlog::info("{:<30}", "left aligned");

  // spdlog::set_level(spdlog::level::debug); // Set global log level to debug
  // spdlog::debug("This message should be displayed..");

  // // change log pattern
  // spdlog::set_pattern("[%H:%M:%S %z] [%n] [%^---%L---%$] [thread %t] %v");

  // DEBUG_PRINTF("hello\n");

  // pprint_string("foo");
  // pprint_bool(false);

  int verbosity = 0;
  auto argument_parser = argparse::ArgumentParser(__FILE__, "0.1.0");
  argument_parser.add_argument("-V", "--verbose")
      .action([&](const auto &) { ++verbosity; })
      .append()
      .default_value(false)
      .implicit_value(true)
      .nargs(0);

  argument_parser.add_argument("-p", "--port")
      .default_value(9001)
      .scan<'i', int>()
      .help(fmt::format("0 < port <= {}", std::pow(2, 16) - 1));

  argument_parser.add_argument("--gui")
      .default_value(false)
      .implicit_value(true)
      .nargs(0)
      .help("Use `sumo-gui` instead of `sumo` to run the simulation");

  argument_parser.add_argument("--simulation-steps")
      .default_value(10000)
      .scan<'i', int>()
      .help("Number of update steps to perform in the simulation");
  // argument_parser

  argument_parser.add_argument("sumocfg").required().help(
      "SUMO configuration file");

  try {
    argument_parser.parse_args(argc, argv);
  } catch (const std::exception &err) {
    fmt::println("{}", err.what());
    std::cerr << argument_parser;
    return 2;
  }

  fs::path sumo_home_path;
  // Check that $SUMO_HOME is set
  if (const char *sumo_home = std::getenv("SUMO_HOME")) {
    // $SUMO_HOME is set
    sumo_home_path = sumo_home;
    // Check that $SUMO_HOME points to a directory that exists on the disk
    if (!fs::exists(sumo_home_path)) {
      spdlog::error("Environment variable $SUMO_HOME ({}) does not point to a "
                    "valid directory",
                    sumo_home);
      return 2;
    }
    spdlog::info("SUMO_HOME: {}", sumo_home_path.string());
  } else {
    // $SUMO_HOME is not set
    spdlog::error("Environment variable SUMO_HOME is not set");
    return 2;
  }

  const fs::path sumocfg_path = argument_parser.get<std::string>("sumocfg");
  if (!fs::exists(sumocfg_path)) {
    spdlog::error("SUMO configuration file not found: {}",
                  sumocfg_path.filename().string());
    std::cerr << argument_parser;
    return 2;
  }

  if (!(sumocfg_path.has_extension()) ||
      sumocfg_path.extension().string() != ".sumocfg") {
    spdlog::error(
        "SUMO configuration file must have extension .sumocfg, not {}",
        sumocfg_path.extension().string());
    std::cerr << argument_parser;
    return 2;
  }

  const fs::path cwd = fs::current_path();

  if (0 < verbosity) {
    spdlog::debug("cwd: {}", cwd.string());
    spdlog::debug("sumocfg: {}", sumocfg_path.string());
  }

  if (sumocfg_path.has_parent_path() && sumocfg_path.parent_path() != cwd) {
    // The SUMO configuration file is not in the current directory
    // Change cwd to the parent directory of sumocfg, as
    // <net-file value="{...}.net.xml"/>
    // <route-files value="{...}.rou.xml"/>
    // <additional-files value="{...}.poly.xml"/>
    // Most likely are defined relative to the parent directory of the sumocfg
    // file.
    fs::current_path(sumocfg_path.parent_path());
    if (0 < verbosity) {
      fmt::println("cwd: {}", fs::current_path().string());
    }
  }

  spdlog::info("cwd: {}", fs::current_path().string());

  // Parse the SUMO configuration file
  pugi::xml_document sumocfg_xml_doc;

  pugi::xml_parse_result result =
      sumocfg_xml_doc.load_file(sumocfg_path.string().c_str());
  if (!result) {
    spdlog::error("Failed to parse SUMO configuration file: {}",
                  result.description());
    return 1;
  }

  const std::string route_files = sumocfg_xml_doc.child("configuration")
                                      .child("input")
                                      .child("route-files")
                                      .attribute("value")
                                      .as_string();
  std::cerr << route_files << std::endl;

  const fs::path route_files_path = fs::path(route_files);
  if (!fs::exists(route_files_path)) {
    spdlog::error("Route files not found: {}", route_files_path.string());
    return 1;
  }

  pugi::xml_document route_files_xml_doc;

  result = route_files_xml_doc.load_file(route_files_path.string().c_str());
  if (!result) {
    spdlog::error("Failed to parse route files: {}", result.description());
    return 1;
  }

  phmap::flat_hash_map<std::string, Car> cars;

  const auto vehicles = route_files_xml_doc.child("routes").children("vehicle");
  for (const auto &vehicle : vehicles) {
    const std::string vehicle_id = vehicle.attribute("id").as_string();
    spdlog::info("vehicle: {}", vehicle_id);

    cars[vehicle_id] = Car{};
  }

  // std::exit(0);

  // If sumocfg is not set in the command line, search for all files in
  // cwd/**/*.sumocfg and print them
  const int port = argument_parser.get<int>("port");
  if (port < 0 || port > std::pow(2, 16) - 1) {
    spdlog::error("Port must be between 0 and {}", std::pow(2, 16) - 1);
    return 1;
  }

  const i32 simulation_steps = argument_parser.get<int>("simulation-steps");
  if (simulation_steps <= 0) {
    spdlog::error("simulation-steps must be positive");
    return 1;
  }

  spdlog::info("simulation-steps: {}", simulation_steps);
  spdlog::info("port: {}", port);

  auto sumo_simulation_thread = std::thread([&]() -> void {
    std::vector<std::string> cmd = {
        argument_parser.get<bool>("gui") ? "sumo-gui" : "sumo",
        "--configuration-file",
        sumocfg_path.string(),
    };

    // const auto simulation_start_result = Simulation::start(cmd, 19000);
    // spdlog::info(".first: {} .second: {}", simulation_start_result.first,
    // simulation_start_result.second);

    const int num_retries = 100;
    const int sumo_sim_port = 10000;
    Simulation::init(sumo_sim_port, num_retries, "localhost");
    const double dt = Simulation::getDeltaT();

    spdlog::info("dt: {}", dt);

    // TODO: figure out how to subscribe an extract data from the simulation
    for (int i = 0; i < simulation_steps; ++i) {
      Simulation::step();

      // TODO: Get (x,y, theta) of all vehicles
      auto vehicles_ids = Vehicle::getIDList();

      for (const auto &id : vehicles_ids) {
        auto &car = cars[id];

        const auto position = Vehicle::getPosition(id);
        const int x = position.x;
        const int y = position.y;

        const auto heading = Vehicle::getAngle(id);
        car.x = x;
        car.y = y;
        car.heading = heading;
        car.alive = true;

        spdlog::info("id: {} x: {} y: {} heading: {}", id, x, y, heading);

        // item.second.x = x;
        // item.second.y = y;
        // item.second.heading = heading;

        //   const auto position = Vehicle::getPosition(id);
        //   const int x = position.x;
        //   const int y = position.y;

        //   const auto heading = Vehicle::getAngle(id);

        //   spdlog::info("id: {} x: {} y: {} heading: {}", id, x, y, heading);
      }

      // for (auto& item : cars) {

      // }

      // fmt::println("step: {}", i);
      if (i % 100 == 0) {
        // fmt::println("{} vehicles", vehicles_ids.size());
        // pprint(vehicles_ids);
      }

      // std::this_thread::sleep_for(10ms);
    }

    Simulation::close();
  });

  int counter = 0;

  auto app = uWS::App()
                 .get("/*",
                      [](auto *res, auto *req) {
                        res->end("Hello World!");
                        fmt::print("Hello World!\n");
                      })
                 .get("/cars",
                      [&](auto *res, auto *req) {

                        // Take all cars and convert them to json
                        json j;
                        for (const auto& item : cars) {
                          const Car& car = item.second;
                          if (!car.alive) {
                            continue;
                          }
                            json carJson = {
                                {"x", item.second.x},
                                {"y", item.second.y},
                                {"heading", item.second.heading},
                            };
                            j.push_back(carJson);
                        }

                        // auto car = Car{0, 0};
                        // json j = {
                        //     {"x", car.x},
                        //     {"y", car.y},
                        //     {"heading", car.heading},
                        // };
                        std::string payload = j.dump();
                        res->end(payload);
                        counter++;
                      })
                 .listen(port, [=](us_listen_socket_t *listen_socket) {
                   if (listen_socket) {
                     fmt::print("Listening for connections on port {}\n", port);
                   }
                 });

  app.run();

  fmt::print(
      "Shoot! We failed to listen and the App fell through, exiting now!\n");

  sumo_simulation_thread.join();

  return 0;
}
