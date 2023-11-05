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

auto main(int argc, char **argv) -> int {

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

  argument_parser.add_argument("sumocfg").required().help("SUMO configuration file");

  try {
    argument_parser.parse_args(argc, argv);
  } catch (const std::exception &err) {
    fmt::println("{}", err.what());
    std::cerr << argument_parser;
    return 1;
  }

  const fs::path cwd = fs::current_path();

  // If sumocfg is not set in the command line, search for all files in
  // cwd/**/*.sumocfg and print them if (!parser["sumocfg"]) {
  //     for (const auto& entry : fs::recursive_directory_iterator(cwd)) {
  //         if (entry.path().extension() == ".sumocfg") {
  //             fmt::println("Found SUMO configuration file: {}",
  //             entry.path());
  //         }
  //     }
  // }

  const int port = argument_parser.get<int>("port");
  if (port < 0 || port > std::pow(2, 16) - 1) {
    fmt::println("Port must be between 0 and {}", std::pow(2, 16) - 1);
    return 1;
  }
  
  fmt::println("Listening on port {}", port);

  int counter = 0;

  auto app = uWS::App()
      .get("/*",
           [](auto *res, auto *req) {
             res->end("Hello World!");
             fmt::print("Hello World!\n");
           })
      .get("/cars",
           [&](auto *res, auto *req) {
             res->end(fmt::format("counter = {}", counter));
             counter++;
           })
      .listen(9001,
              [](us_listen_socket_t *listen_socket) {
                if (listen_socket) {
                  fmt::print("Listening for connections...\n");
                }
              });

  app.run();

  fmt::print(
      "Shoot! We failed to listen and the App fell through, exiting now!\n");

  return 0;
}
