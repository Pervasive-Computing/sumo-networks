#include <cmath>
#include <filesystem>
#include <functional>
#include <iostream>
#include <optional>
#include <string>
#include <thread>
#include <vector>

namespace fs = std::filesystem;

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

#include <argparse/argparse.hpp>
#include <fmt/core.h>
#include <uWebSockets/App.h>
#include <spdlog/spdlog.h>
#include <nlohmann/json.hpp>

// for convenience
using json = nlohmann::json;
using namespace nlohmann::literals; // for ""_json

#include <libsumo/libtraci.h>

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
};

auto main(int argc, char **argv) -> int {
    //   spdlog::info("Welcome to spdlog!");
    // spdlog::error("Some error message with arg: {}", 1);
    
    // spdlog::warn("Easy padding in numbers like {:08d}", 12);
    // spdlog::critical("Support for int: {0:d};  hex: {0:x};  oct: {0:o}; bin: {0:b}", 42);
    // spdlog::info("Support for floats {:03.2f}", 1.23456);
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
      .help("use `sumo-gui` instead of `sumo` to run the simulation");

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
  if (const char* sumo_home = std::getenv("SUMO_HOME")) {
      // $SUMO_HOME is set
      sumo_home_path = sumo_home;
      // Check that $SUMO_HOME points to a directory that exists on the disk
      if (!fs::exists(sumo_home_path)) {
          spdlog::error("Environment variable $SUMO_HOME ({}) does not point to a valid directory", sumo_home);
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
    spdlog::error("SUMO configuration file must have extension .sumocfg, not {}",
                 sumocfg_path.extension().string());
                 std::cerr << argument_parser;
    return 2;
  }

  const fs::path cwd = fs::current_path();

  if (0 < verbosity) {
    spdlog::debug("cwd: {}", cwd.string());
    spdlog::debug("sumocfg: {}", sumocfg_path.string());
  }

  if (sumocfg_path.has_parent_path()  && sumocfg_path.parent_path() != cwd) {
    // The SUMO configuration file is not in the current directory
    // Change cwd to the parent directory of sumocfg, as 
    // <net-file value="{...}.net.xml"/>
    // <route-files value="{...}.rou.xml"/>
    // <additional-files value="{...}.poly.xml"/>
    // Most likely are defined relative to the parent directory of the sumocfg file.
    fs::current_path(sumocfg_path.parent_path());
    if (0 < verbosity) {
      fmt::println("cwd: {}", fs::current_path().string());
    }
  }

  // If sumocfg is not set in the command line, search for all files in
  // cwd/**/*.sumocfg and print them
  const int port = argument_parser.get<int>("port");
  if (port < 0 || port > std::pow(2, 16) - 1) {
    fmt::println("Port must be between 0 and {}", std::pow(2, 16) - 1);
    return 1;
  }

  auto sumo_simulation_thread = std::thread([&]() {
    using namespace libtraci;
    std::vector<std::string> cmd = {
        argument_parser.get<bool>("gui") ? "sumo-gui" : "sumo",
        "--configuration-file",
        sumocfg_path.string(),
    };

    Simulation::start(cmd);

    // TODO: figure out how to subscribe an extract data from the simulation
    for (int i = 0; i < 10000; i++) {
      Simulation::step();
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
                        auto car = Car{0, 0};
                        json j = {
                            {"x", car.x},
                            {"y", car.y},
                        };
                        std::string payload = j.dump();
                        res->end(payload);
                        //  res->end(fmt::format("counter = {}", counter));
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
