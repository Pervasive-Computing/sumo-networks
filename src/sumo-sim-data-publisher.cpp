#include <algorithm>
#include <chrono>
using namespace std::chrono_literals;
#include <cmath>
#include <filesystem>
#include <queue>
#include <functional>
#include <iostream>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>

// 3rd party libraries
#include <argparse/argparse.hpp>
#include <fmt/core.h>
#include <nlohmann/json.hpp>
// for convenience
using json = nlohmann::json;
using namespace nlohmann::literals; // for ""_json
#include <indicators/cursor_control.hpp>
#include <parallel_hashmap/phmap.h>
#include <pugixml.hpp>
#include <spdlog/spdlog.h>
#include <tl/expected.hpp>
// #include <uWebSockets/App.h>
// #include <indicators/progress_bar.hpp>
#include <indicators/block_progress_bar.hpp>

#include <zmq.hpp>

#include <libsumo/libtraci.h>

#include "ansi-escape-codes.hpp"
#include "debug-macro.hpp"
#include "pretty-printers.hpp"
#include "streetlamp.hpp"


// #include "cars.pb.h"

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

/**
 * Calculates the haversine distance between two points on the Earth's surface expressed in lon/lat coordinates.
 * @note https://en.wikipedia.org/wiki/Haversine_formula
 * @note https://stackoverflow.com/questions/639695/how-to-convert-latitude-or-longitude-to-meters/11172685#11172685
 * 
 * @param lon1 The longitude of the first point in degrees.
 * @param lat1 The latitude of the first point in degrees.
 * @param lon2 The longitude of the second point in degrees.
 * @param lat2 The latitude of the second point in degrees.
 * @return The haversine distance between the two points in meters.
 */
inline auto haversine(const double lon1, const double lat1, const double lon2, const double lat2) -> double {
	constexpr double earth_radius = 6371.0; // Earth radius in kilometers
	constexpr double degrees_to_radians = M_PI / 180.0;

	// Convert latitude and longitude from degrees to radians
	const double lon1_rad = lon1 * degrees_to_radians;
	const double lat1_rad = lat1 * degrees_to_radians;
	const double lon2_rad = lon2 * degrees_to_radians;
	const double lat2_rad = lat2 * degrees_to_radians;

	// Calculate the differences between the latitudes and longitudes
	const double delta_lon = lon2_rad - lon1_rad;
	const double delta_lat = lat2_rad - lat1_rad;

	// Calculate the square of half the chord length between the points
	const double a = std::sin(delta_lat / 2.0) * std::sin(delta_lat / 2.0) +
					 std::cos(lat1_rad) * std::cos(lat2_rad) *
					 std::sin(delta_lon / 2.0) * std::sin(delta_lon / 2.0);

	// Calculate the angular distance in radians
	const double c = 2.0 * std::atan2(std::sqrt(a), std::sqrt(1.0 - a));

	// Calculate the distance in meters
	const double distance = earth_radius * c * 1000.0;

	return distance;
}

// [[nodiscard]] auto walkdir(
// 	const std::string& dir, const std::function<bool(const std::filesystem::directory_entry&)>& filter =
// 								[](const auto&) { return true; }) -> std::vector<std::string> {
// 	std::vector<std::string> files;
// 	for (const auto& entry : std::filesystem::directory_iterator(dir)) {
// 		if (! filter(entry)) {
// 			continue;
// 		}
// 		files.push_back(entry.path().string());
// 	}
// 	return files;
// }

// <configuration>
//     <input>
//         <net-file value="esbjerg.net.xml"/>
//         <route-files value="esbjerg.rou.xml"/>
//         <additional-files value="esbjerg.poly.xml"/>
//     </input>
//     <time>
//         <begin value="0"/>
//         <step-length value="0.001"/>
//         <end value="20000"/>
//     </time>
//     <gui_only>
//         <delay value="1"/>
//         <start value="true"/>
//     </gui_only>
// </configuration>


static inline void show_console_cursor(bool const show) {
  std::fputs(show ? "\033[?25h" : "\033[?25l", stdout);
}

static inline void erase_line() {
  std::fputs("\r\033[K", stdout);
}

struct SumoConfiguration {
	std::filesystem::path net_file;
	std::filesystem::path route_files;
	std::filesystem::path additional_files;
};

auto parse_sumocfg(const std::filesystem::path& sumocfg_path) -> tl::expected<SumoConfiguration, std::string> {
	pugi::xml_document sumocfg_xml_doc;
	spdlog::info("sumocfg_path: {}, cwd: {}", sumocfg_path.string(), std::filesystem::current_path().string());
	pugi::xml_parse_result result = sumocfg_xml_doc.load_file(sumocfg_path.string().c_str());
	if (! result) {
		return tl::unexpected(
			fmt::format("Failed to parse SUMO configuration file: {}", result.description()));
	}

	const auto net_file = sumocfg_xml_doc.child("configuration")
							  .child("input")
							  .child("net-file")
							  .attribute("value")
							  .as_string();
	const auto route_files = sumocfg_xml_doc.child("configuration")
								 .child("input")
								 .child("route-files")
								 .attribute("value")
								 .as_string();
	const auto additional_files = sumocfg_xml_doc.child("configuration")
									  .child("input")
									  .child("additional-files")
									  .attribute("value")
									  .as_string();

	return SumoConfiguration {
		.net_file = net_file,
		.route_files = route_files,
		.additional_files = additional_files,
	};
}

struct Car {
	int	   x;
	int	   y;
	double heading;
	bool   alive = false;

	inline auto to_json() const -> json {
		return json {
			{"x",		  this->x		 },
			{"y",		  this->y		 },
			{"heading", this->heading},
		};
	}
};



enum class get_sumo_home_directory_path_error {
	environment_variable_not_set,
	environment_variable_not_a_directory,
};

auto get_sumo_home_directory_path() -> tl::expected<std::filesystem::path, get_sumo_home_directory_path_error> {
	// Check that $SUMO_HOME is set
	if (const char* sumo_home = std::getenv("SUMO_HOME")) {
		// $SUMO_HOME is set
		const std::filesystem::path sumo_home_path = sumo_home;
		// Check that $SUMO_HOME points to a directory that exists on the disk
		if (! std::filesystem::exists(sumo_home_path)) {
			return tl::unexpected(
				get_sumo_home_directory_path_error::environment_variable_not_a_directory);
		}
		return sumo_home_path;
	} else {
		// $SUMO_HOME is not set
		return tl::unexpected(get_sumo_home_directory_path_error::environment_variable_not_set);
	}
}

struct ProgramOptions {
	u16		 port;
	u16		 sumo_port;
	i32		 simulation_steps;
	std::filesystem::path sumocfg_path;
	std::filesystem::path osm_path;
	// bool	 gui = true;
	bool verbose = false;
};

auto pprint(const ProgramOptions& options) -> void {
	using namespace escape_codes;
	fmt::println("({}ProgramOptions{}) {{", color::fg::cyan, reset);
	const auto indent = std::string(4, ' ');
	fmt::println("{}{}.port{} = {},", indent, markup::bold, reset, pformat(options.port));
	fmt::println("{}{}.sumo_port{} = {},", indent, markup::bold, reset, pformat(options.sumo_port));
	fmt::println("{}{}.simulation_steps{} = {},", indent, markup::bold, reset,
				 pformat(options.simulation_steps));
	fmt::println("{}{}.sumocfg_path{} = {},", indent, markup::bold, reset,
				 pformat(options.sumocfg_path));
	fmt::println("{}{}.osm_path{} = {},", indent, markup::bold, reset,
				 pformat(options.osm_path));
	// fmt::println("{}{}.gui{} = {},", indent, markup::bold, reset, pformat(options.gui));
	fmt::println("{}{}.verbose{} = {},", indent, markup::bold, reset, pformat(options.verbose));
	fmt::println("}};");
}

[[nodiscard]]
auto create_argv_parser() -> argparse::ArgumentParser {
	auto argv_parser = argparse::ArgumentParser(__FILE__, "0.1.0");

	argv_parser
		.add_argument("-V", "--verbose")
		// .action([&](const auto&) { ++verbosity; })
		// .append()
		.default_value(false)
		.implicit_value(true)
		.nargs(0);

	argv_parser.add_argument("-p", "--port")
		.default_value(12000)
		.scan<'i', int>()
		.help(fmt::format("Port used for the websocket server, constraints: 0 < port <= {}",
						  std::pow(2, 16) - 1));

	argv_parser.add_argument("--sumo-port")
		.default_value(12001)
		.scan<'i', int>()
		.help(fmt::format(
			"Port used to connect to a running sumo simulation, constraints: 0 < port <= {}",
			std::pow(2, 16) - 1));


	// argv_parser.add_argument("--gui").default_value(false).implicit_value(true).nargs(0).help(
	// 	"Use `sumo-gui` instead of `sumo` to run the simulation");

	argv_parser.add_argument("--simulation-steps")
		.default_value(10000)
		.scan<'i', int>()
		.help("Number of update steps to perform in the simulation, constraints: 0 < "
			  "simulation-steps");

	argv_parser.add_argument("--osm")
		.required()
		.help("OpenStreetMap file of the area");

	argv_parser.add_argument("sumocfg").required().help("SUMO configuration file");
	return argv_parser;
}

auto parse_args(argparse::ArgumentParser argv_parser, int argc, char** argv)
	-> tl::expected<ProgramOptions, std::string> {
	try {
		argv_parser.parse_args(argc, argv);
	} catch (const std::exception& err) {
		return tl::unexpected(fmt::format("{}", err.what()));
	}

	// If sumocfg is not set in the command line, search for all files in
	// cwd/**/*.sumocfg and print them
	const int port = argv_parser.get<int>("port");
	if (port < 0 || port > std::pow(2, 16) - 1) {
		return tl::unexpected(fmt::format("--port must be between 0 and {}", std::pow(2, 16) - 1));
	}

	const int sumo_port = argv_parser.get<int>("sumo-port");
	if (sumo_port < 0 || sumo_port > std::pow(2, 16) - 1) {
		return tl::unexpected(
			fmt::format("--sumo-port must be between 0 and {}", std::pow(2, 16) - 1));
	}

	if (port == sumo_port) {
		return tl::unexpected("--port and --sumo-port must be different");
	}

	const i32 simulation_steps = argv_parser.get<int>("simulation-steps");
	if (simulation_steps <= 0) {
		return tl::unexpected("simulation-steps must be positive");
	}

	const auto sumocfg_path = std::filesystem::absolute(argv_parser.get<std::string>("sumocfg"));
	if (! std::filesystem::exists(sumocfg_path)) {
		return tl::unexpected(
			fmt::format("SUMO configuration file not found: {}", sumocfg_path.filename().string()));
	}

	if (! (sumocfg_path.has_extension()) || sumocfg_path.extension().string() != ".sumocfg") {
		return tl::unexpected(
			fmt::format("SUMO configuration file must have extension .sumocfg, not {}",
						sumocfg_path.extension().string()));
	}

	const auto osm_path = std::filesystem::absolute(argv_parser.get<std::string>("osm"));
	if (! std::filesystem::exists(osm_path)) {
		return tl::unexpected(
			fmt::format("OSM configuration file not found: {}", osm_path.filename().string()));
	}

	if (! (osm_path.has_extension()) || osm_path.extension().string() != ".osm") {
		return tl::unexpected(
			fmt::format("OSM configuration file must have extension .osm, not {}",
						osm_path.extension().string()));
	}

	return ProgramOptions {
		.port = static_cast<u16>(port),
		.sumo_port = static_cast<u16>(sumo_port),
		.simulation_steps = simulation_steps,
		.sumocfg_path = sumocfg_path,
		.osm_path = osm_path,
		// .gui = argv_parser.get<bool>("gui"),
		.verbose = argv_parser.get<bool>("verbose"),
	};
}


auto main(int argc, char** argv) -> int {
	const auto argv_parser = create_argv_parser();
	const auto print_help = [&]() -> void {
		std::cerr << argv_parser;
	};
	const auto options = parse_args(argv_parser, argc, argv)
							 .map_error([&](const auto& err) {
								 spdlog::error("{}", err);
								 print_help();
								 std::exit(2);
							 })
							 .value();

	pprint(options);

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
			} else if (err ==
					   get_sumo_home_directory_path_error::environment_variable_not_a_directory) {
				spdlog::error(
					"Environment variable $SUMO_HOME ({}) does not point to a valid directory",
					std::getenv("SUMO_HOME"));
			}
			std::exit(2);
		}
	}();

	const std::filesystem::path cwd = std::filesystem::current_path();

	if (options.verbose) {
		spdlog::debug("cwd: {}", cwd.string());
		spdlog::debug("sumocfg: {}", options.sumocfg_path.string());
	}


	// Parse the SUMO configuration file
	const auto sumocfg = parse_sumocfg(options.sumocfg_path)
							 .map_error([](const auto& err) {
								 spdlog::error("{}", err);
								 std::exit(1);
							 })
							 .value();


	if (options.sumocfg_path.has_parent_path() && options.sumocfg_path.parent_path() != cwd) {
		// The SUMO configuration file is not in the current directory
		// Change cwd to the parent directory of sumocfg, as
		// <net-file value="{...}.net.xml"/>
		// <route-files value="{...}.rou.xml"/>
		// <additional-files value="{...}.poly.xml"/>
		// Most likely are defined relative to the parent directory of the sumocfg
		// file.
		std::filesystem::current_path(options.sumocfg_path.parent_path());
		spdlog::warn("Changed cwd to {}", std::filesystem::current_path().string());
	}

	pugi::xml_document route_files_xml_doc;

	const auto result = route_files_xml_doc.load_file(sumocfg.route_files.c_str());
	if (! result) {
		spdlog::error("Failed to parse route files: {}", result.description());
		return 1;
	}

	phmap::flat_hash_map<std::string, Car> cars;

	const auto vehicles = route_files_xml_doc.child("routes").children("vehicle");
	for (const auto& vehicle : vehicles) {
		const std::string vehicle_id = vehicle.attribute("id").as_string();
		cars[vehicle_id] = Car {};
	}

	
	zmq::context_t	  zmq_ctx;
	zmq::socket_t	  sock(zmq_ctx, zmq::socket_type::pub);
	const std::string addr = fmt::format("tcp://*:{}", options.port);
	sock.bind(addr);
	spdlog::info("Bound zmq PUB socket to {}", addr);
	const std::string_view m = "Hello, world";
	sock.send(zmq::buffer(m));

	spdlog::info("Found {} vehicles in {}", cars.size(), sumocfg.route_files.string());
	const int num_retries_sumo_sim_connect = 100;
	Simulation::init(options.sumo_port, num_retries_sumo_sim_connect, "localhost");
	const double dt = Simulation::getDeltaT();

	auto streetlamps = extract_streetlamps_from_osm(options.osm_path)
								 .map_error([](const auto& err) {
									if (err == extract_streetlamps_from_osm_error::file_not_found) {
										spdlog::error("OSM file not found");
									} else if (err == extract_streetlamps_from_osm_error::xml_parse_error) {
										spdlog::error("Failed to parse OSM file");
									}
									std::exit(1);
								 })
								 .value();


	
	const auto n_threads = std::thread::hardware_concurrency();
	spdlog::info("n_threads: {}", n_threads);

	// Create a thread pool
	// std::vector<std::thread> threads;
	// threads.reserve(n_threads);



	// AHHH
	// Change each street lamp's lon/lat into x/y
	// We only need to do this once, as the street lamps are static
	// We need to do this since the OpenStreetMap file contains lon/lat coordinates of the street lamps
	// but the SUMO simulation uses x/y coordinates for the vehicles
	// We do this here an not in the parsing step because we need to have the simulation running
	// to convert lon/lat to x/y
	for (auto& lamp : streetlamps) {
		const auto geo = Simulation::convertGeo(lamp.lon, lamp.lat, true);
		lamp.lon = geo.x;
		lamp.lat = geo.y;
	}

	std::for_each(std::begin(streetlamps), std::end(streetlamps), [](const auto& lamp) {
		pprint(lamp);
	});

	spdlog::info("streetlamps.size(): {}", streetlamps.size());

	spdlog::info("dt: {}", dt);

	// Hide cursor
	indicators::show_console_cursor(false);

	auto bar = indicators::BlockProgressBar {
		indicators::option::BarWidth {80}, indicators::option::Start {"["},
		// indicators::option::Fill{"■"},
		// indicators::option::Lead{"■"},
		// indicators::option::Remainder{"-"},
		indicators::option::End {" ]"},
		// indicators::option::PostfixText{""},
		indicators::option::ShowElapsedTime {true},
		indicators::option::ForegroundColor {indicators::Color::yellow},
		indicators::option::FontStyles {
			std::vector<indicators::FontStyle> {indicators::FontStyle::bold}}};



	// TODO: figure out how to subscribe an extract data from the simulation
	for (int i = 0; i < options.simulation_steps; ++i) {
		Simulation::step();
		// Show step/total_steps instead of percent in progress bar
		const double percent_done = static_cast<double>(i) / options.simulation_steps * 100.0;
		// bar.set_progress(percent_done);
		// bar.set_option(
		// 	indicators::option::PostfixText(fmt::format("{}/{}", i, options.simulation_steps)));

		// Get (x,y, theta) of all vehicles
		auto vehicles_ids = Vehicle::getIDList();

		for (const auto& id : vehicles_ids) {
			auto& car = cars[id];

			const auto	 position = Vehicle::getPosition(id);
			// const auto geo = Simulation::convertGeo(position.x, position.y);
			// fmt::println("geo.x = {}, geo.y = {}", geo.x, geo.y);
			// const auto lat = Vehicle::getLatitude(id);
			// const double lat = geo.y;
			// const double lon = geo.x;
			// TODO: convert the lamps lon/lat into x/y once in the preprocessing step

			const double heading = Vehicle::getAngle(id);

			car.x = position.x;
			car.y = position.y;
			car.heading = heading;
			car.alive = true;
		}

		// TODO: preallocate some of the memory structures used in this block
		{ // Publish the data to clients
			json j;
			// Find all cars that are alive and put them into a json object
			for (const auto& item : cars) {
				const Car& car = item.second;
				if (! car.alive) {
					continue;
				}
				const std::string& vehicle_id = item.first;
				const auto		   car_as_json = car.to_json();
				j[vehicle_id] = car_as_json;
			}

			// Serialize to CBOR encoding format
			const std::vector<u8> v = json::to_cbor(j);

			// Convert to std::string
			const std::string payload(v.begin(), v.end());

			// Send the data to all clients
			sock.send(zmq::buffer(payload), zmq::send_flags::dontwait);
		}
	}

	indicators::show_console_cursor(true);

	Simulation::close();

	return 0;
}
