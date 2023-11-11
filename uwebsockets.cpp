#include <chrono>
using namespace std::chrono_literals;
#include <cmath>
#include <filesystem>
#include <queue>
namespace fs = std::filesystem;
// #include <expected>
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
	const std::string& dir, const std::function<bool(const fs::directory_entry&)>& filter =
								[](const auto& _) { return true; }) -> std::vector<std::string> {
	std::vector<std::string> files;
	for (const auto& entry : fs::directory_iterator(dir)) {
		if (! filter(entry)) {
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
// for convenience
using json = nlohmann::json;
using namespace nlohmann::literals; // for ""_json
#include <parallel_hashmap/phmap.h>
#include <pugixml.hpp>
#include <spdlog/spdlog.h>
#include <uWebSockets/App.h>
#include <tl/expected.hpp>

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

// template <typename T>
// using Vec = std::vector<T>;
// using String = std::string;

// template <typename T>
// using Option = std::optional<T>;

struct Car {
	int	   x;
	int	   y;
	double heading;
	bool   alive = false;
};

enum class get_sumo_home_directory_path_error {
	environment_variable_not_set,
	environment_variable_not_a_directory,
};

auto get_sumo_home_directory_path() -> tl::expected<fs::path, get_sumo_home_directory_path_error> {
	// Check that $SUMO_HOME is set
	if (const char* sumo_home = std::getenv("SUMO_HOME")) {
		// $SUMO_HOME is set
		const fs::path sumo_home_path = sumo_home;
		// Check that $SUMO_HOME points to a directory that exists on the disk
		if (! fs::exists(sumo_home_path)) {
			return tl::unexpected(
				get_sumo_home_directory_path_error::environment_variable_not_a_directory);
		}
		return sumo_home_path;
	} else {
		// $SUMO_HOME is not set
		return tl::unexpected(get_sumo_home_directory_path_error::environment_variable_not_set);
	}
}

auto main(int argc, char** argv) -> int {
	int	 verbosity = 0;
	auto argv_parser = argparse::ArgumentParser(__FILE__, "0.1.0");

	argv_parser.add_argument("-V", "--verbose")
		.action([&](const auto&) { ++verbosity; })
		.append()
		.default_value(false)
		.implicit_value(true)
		.nargs(0);

	argv_parser.add_argument("-p", "--port")
		.default_value(9001)
		.scan<'i', int>()
		.help(fmt::format("Port used for the websocket server, constraints: 0 < port <= {}",
						  std::pow(2, 16) - 1));

	argv_parser.add_argument("--gui").default_value(false).implicit_value(true).nargs(0).help(
		"Use `sumo-gui` instead of `sumo` to run the simulation");

	argv_parser.add_argument("--simulation-steps")
		.default_value(10000)
		.scan<'i', int>()
		.help("Number of update steps to perform in the simulation, constraints: 0 < "
			  "simulation-steps");

	argv_parser.add_argument("sumocfg").required().help("SUMO configuration file");

	try {
		argv_parser.parse_args(argc, argv);
	} catch (const std::exception& err) {
		fmt::println("{}", err.what());
		std::cerr << argv_parser;
		return 2;
	}

	const auto sumo_home_path = [&]() {
		auto result = get_sumo_home_directory_path();
		if (result) {
			const auto sumo_home_path = *result; // If there is a value, use it.
			spdlog::info("SUMO_HOME: {}", sumo_home_path.string());
			return sumo_home_path;
		} else {
			// Handle the error case
			const auto& err = result.error();
			if (err == get_sumo_home_directory_path_error::environment_variable_not_set) {
				spdlog::error("Environment variable SUMO_HOME is not set");
			} else if (err == get_sumo_home_directory_path_error::environment_variable_not_a_directory) {
				spdlog::error("Environment variable $SUMO_HOME ({}) does not point to a valid directory", std::getenv("SUMO_HOME"));
			}
			std::exit(2);
		}
	}();


	const fs::path sumocfg_path = argv_parser.get<std::string>("sumocfg");
	if (! fs::exists(sumocfg_path)) {
		spdlog::error("SUMO configuration file not found: {}", sumocfg_path.filename().string());
		std::cerr << argv_parser;
		return 2;
	}

	if (! (sumocfg_path.has_extension()) || sumocfg_path.extension().string() != ".sumocfg") {
		spdlog::error("SUMO configuration file must have extension .sumocfg, not {}",
					  sumocfg_path.extension().string());
		std::cerr << argv_parser;
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

	pugi::xml_parse_result result = sumocfg_xml_doc.load_file(sumocfg_path.string().c_str());
	if (! result) {
		spdlog::error("Failed to parse SUMO configuration file: {}", result.description());
		return 1;
	}

	const std::string route_files = sumocfg_xml_doc.child("configuration")
										.child("input")
										.child("route-files")
										.attribute("value")
										.as_string();
	std::cerr << route_files << std::endl;

	const fs::path route_files_path = fs::path(route_files);
	if (! fs::exists(route_files_path)) {
		spdlog::error("Route files not found: {}", route_files_path.string());
		return 1;
	}

	pugi::xml_document route_files_xml_doc;

	result = route_files_xml_doc.load_file(route_files_path.string().c_str());
	if (! result) {
		spdlog::error("Failed to parse route files: {}", result.description());
		return 1;
	}

	phmap::flat_hash_map<std::string, Car> cars;
	std::mutex							   cars_mutex;

	const auto vehicles = route_files_xml_doc.child("routes").children("vehicle");
	for (const auto& vehicle : vehicles) {
		const std::string vehicle_id = vehicle.attribute("id").as_string();
		spdlog::info("vehicle: {}", vehicle_id);

		cars[vehicle_id] = Car {};
	}

	// std::exit(0);

	// If sumocfg is not set in the command line, search for all files in
	// cwd/**/*.sumocfg and print them
	const int port = argv_parser.get<int>("port");
	if (port < 0 || port > std::pow(2, 16) - 1) {
		spdlog::error("Port must be between 0 and {}", std::pow(2, 16) - 1);
		return 1;
	}

	const i32 simulation_steps = argv_parser.get<int>("simulation-steps");
	if (simulation_steps <= 0) {
		spdlog::error("simulation-steps must be positive");
		return 1;
	}

	spdlog::info("simulation-steps: {}", simulation_steps);
	spdlog::info("port: {}", port);

	auto sumo_simulation_thread = std::thread([&]() -> void {
		std::vector<std::string> cmd = {
			argv_parser.get<bool>("gui") ? "sumo-gui" : "sumo",
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
			auto			 vehicles_ids = Vehicle::getIDList();
			std::scoped_lock lock(cars_mutex);

			for (const auto& id : vehicles_ids) {
				auto& car = cars[id];

				const auto position = Vehicle::getPosition(id);
				const int  x = position.x;
				const int  y = position.y;

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
						[](auto* res, auto* req) {
							res->writeHeader("Content-Type", "text/plain");
							//  res->
							res->end("Hello World!");
							fmt::print("Hello World!\n");
						})
				   .get("/cars",
						[&](auto* res, auto* req) {
							// Take all cars and convert them to json
							// The structure is
							// {
							//  "vehicle_id_1": {
							//    "x": 0,
							//    "y": 0,
							//    "heading": 0
							//  },
							//  "vehicle_id_2": {
							//    "x": 0,
							//    "y": 0,
							//    "heading": 0
							//  },
							//  ...
							// }

							std::scoped_lock lock(cars_mutex);
							json			 j;
							for (const auto& item : cars) {

								const Car& car = item.second;
								if (! car.alive) {
									continue;
								}
								const std::string& vehicle_id = item.first;
								json			   car_as_json = {
									  {"x",		item.second.x		 },
									  {"y",		item.second.y		 },
									  {"heading", item.second.heading},
								  };
								j[vehicle_id] = car_as_json;
							}

							// auto car = Car{0, 0};
							// json j = {
							//     {"x", car.x},
							//     {"y", car.y},
							//     {"heading", car.heading},
							// };
							std::string payload = j.dump();
							res->writeHeader("Content-Type", "application/json; charset=utf-8")
								->end(payload);
							// res->end(payload);
							// Set status code

							counter++;
						})

				   .listen(port, [=](us_listen_socket_t* listen_socket) {
					   if (listen_socket) {
						   spdlog::info("Listening for connections on port {}\n", port);
					   } else {
						   spdlog::error("Failed to listen on port {}", port);
					   }
				   });

	app.run();

	sumo_simulation_thread.join();

	return 0;
}
