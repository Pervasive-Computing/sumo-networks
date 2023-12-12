// #include <algorithm>
#include <chrono>
#include <charconv>
#include <cmath>
using namespace std::chrono_literals;
#include <cmath>
#include <filesystem>
// #include <queue>
// #include <functional>
#include <iostream>
// #include <mutex>
// #include <numeric>
// #include <optional>
#include <string>
#include <string_view>
// #include <execution>
using namespace std::string_view_literals;
#include <thread>
#include <vector>

// #include <immintrin.h> // SIMD intrinsics

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
// #include <indicators/progress_bar.hpp>
#include <indicators/block_progress_bar.hpp>

#include <BS_thread_pool.hpp>
#include <toml.hpp>
#include <zmq.hpp>


#include <libsumo/libtraci.h>

#include "ansi-escape-codes.hpp"
// #include "debug-macro.hpp"
#include "humantime.hpp"
#include "pretty-printers.hpp"
#include "streetlamp.hpp"

using namespace libtraci; // TODO: remove

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


// static inline void show_console_cursor(bool const show) {
//   std::fputs(show ? "\033[?25h" : "\033[?25l", stdout);
// }

// static inline void erase_line() {
//   std::fputs("\r\033[K", stdout);
// }

class Timer {
  public:
	Timer() : t_start(std::chrono::high_resolution_clock::now()) { }

	auto elapsed_ns() const {
		return std::chrono::duration_cast<std::chrono::nanoseconds>(
			std::chrono::high_resolution_clock::now() - t_start);
	}

	auto elapsed_us() const -> std::uint64_t {
		const auto t_now = std::chrono::high_resolution_clock::now();
		const auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(t_now - t_start);
		return elapsed.count();
	}

	auto elapsed_ms() const -> std::uint64_t {
		const auto t_now = std::chrono::high_resolution_clock::now();
		const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(t_now - t_start);
		return elapsed.count();
	}

	auto elapsed_s() const -> std::uint64_t {
		const auto t_now = std::chrono::high_resolution_clock::now();
		const auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(t_now - t_start);
		return elapsed.count();
	}

	auto reset() -> void { t_start = std::chrono::high_resolution_clock::now(); }

  private:
	std::chrono::high_resolution_clock::time_point t_start;
};

struct SumoConfiguration {
	std::filesystem::path net_file;
	std::filesystem::path route_files;
	std::filesystem::path additional_files;
};

auto parse_sumocfg(const std::filesystem::path& sumocfg_path)
	-> tl::expected<SumoConfiguration, std::string> {
	pugi::xml_document sumocfg_xml_doc;
	spdlog::info("sumocfg_path: {}, cwd: {}", sumocfg_path.string(),
				 std::filesystem::current_path().string());
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

auto get_sumo_home_directory_path()
	-> tl::expected<std::filesystem::path, get_sumo_home_directory_path_error> {
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
	u16					  port;
	bool				  verbose = false;
	u16					  sumo_port;
	i32					  simulation_steps;
	std::filesystem::path sumocfg_path;
	std::filesystem::path osm_path;
	// bool	 gui = true;
	bool use_sumo_gui = false;
	bool spawn_sumo = false;
	i32	 streetlamp_distance_threshold;

	static auto print_toml_schema() -> void {
		fmt::print(R"(
verbose = false # <bool>
port = 10001 # <u16>

[sumo]
port = 10000 # <u16>
sumocfg-path = "katrinebjerg-lamp/katrinebjerg-lamp.sumocfg" # <string>
osm-path = "katrinebjerg-lamp/katrinebjerg-lamp.osm" # <string>
simulation-steps = 10000 # <unsigned integer>

[sumo.spawn]
enabled = true # <bool>
gui = false # <bool>

[sumo.streetlamps]
distance-threshold = 10 # <unsigned integer>
)");
	}
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
	fmt::println("{}{}.osm_path{} = {},", indent, markup::bold, reset, pformat(options.osm_path));
	// fmt::println("{}{}.gui{} = {},", indent, markup::bold, reset, pformat(options.gui));
	fmt::println("{}{}.verbose{} = {},", indent, markup::bold, reset, pformat(options.verbose));
	fmt::println("{}{}.use_sumo_gui{} = {},", indent, markup::bold, reset,
				 pformat(options.use_sumo_gui));
	fmt::println("{}{}.spawn_sumo{} = {},", indent, markup::bold, reset,
				 pformat(options.spawn_sumo));
	fmt::println("{}{}.streetlamp_distance_threshold{} = {},", indent, markup::bold, reset,
				 pformat(options.streetlamp_distance_threshold));
	fmt::println("}};");
}

[[nodiscard]] auto create_argv_parser() -> argparse::ArgumentParser {
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

	argv_parser.add_argument("--osm").required().help("OpenStreetMap file of the area");

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
		return tl::unexpected(fmt::format("OSM configuration file must have extension .osm, not {}",
										  osm_path.extension().string()));
	}

	return ProgramOptions {
		.port = static_cast<u16>(port),
		.verbose = argv_parser.get<bool>("verbose"),
		.sumo_port = static_cast<u16>(sumo_port),
		.simulation_steps = simulation_steps,
		.sumocfg_path = sumocfg_path,
		.osm_path = osm_path,
		// .gui = argv_parser.get<bool>("gui"),
	};
}

auto between(const int x, const int min, const int max) -> bool {
	return min <= x && x <= max;
}

// verbose = false
// port = 10001

// [sumo]
// port = 10000
// sumocfg-path = "katrinebjerg-lamp/katrinebjerg-lamp.sumocfg"
// osm-path = "katrinebjerg-lamp/katrinebjerg-lamp.osm"
// simulation-steps = 10000

// [sumo.spawn]
// enabled = true
// gui = false

// /**
//  * Checks if the current program is running in the Windows Subsystem for Linux (WSL) environment.
//  *
//  * @return true if running in WSL, false otherwise.
//  */
// auto in_wsl() -> bool {
// 	static std::optional<bool> in_wsl_cache = std::nullopt;
// 	if (!in_wsl_cache.has_value()) {
// 		const auto proc_version = std::filesystem::path("/proc/version");
// 		if (!std::filesystem::exists(proc_version)) {
// 			// /proc/version does not exist, so we are not in WSL
// 			in_wsl_cache = false;
// 		} else {
// 			// Open the file and search for the word "microsoft" case insensitively
// 			std::ifstream file(proc_version);
// 			std::string   line;
// 			while (std::getline(file, line)) {
// 				std::transform(line.begin(), line.end(), line.begin(), ::tolower);
// 				if (line.find("microsoft") != std::string::npos) {
// 					in_wsl_cache = true;
// 					break;
// 				}
// 			}
// 			in_wsl_cache = false;
// 		}
// 	}
// 	return *in_wsl_cache;
// }


// auto which(const std::string& program) -> std::vector<std::filesystem::path> {
// 	std::vector<std::filesystem::path> result;
// 	const std::string                   path_env = std::getenv("PATH");
// 	const std::string                   path_separator = in_wsl() ? ":" : ";";
// 	const std::vector<std::string>      path_dirs = split(path_env, path_separator);
// 	for (const auto& dir : path_dirs) {
// 		const std::filesystem::path path = dir;
// 		const std::filesystem::path program_path = path / program;
// 		if (std::filesystem::exists(program_path)) {
// 			result.push_back(program_path);
// 		}
// 	}
// 	return result;
// }

// auto start_sumo_process(const std::vector<std::string_view>& args, const bool gui) -> bool {
// 	#if defined(__linux__) || defined(__unix__)
// 		// Linux or Unix like operating system (e.g. MacOS)
// 		const std::string sumo_executable = gui ? "sumo-gui" : "sumo";

// 		if (gui) {
// 			// Use execfe to start the sumo-gui process
// 			execfe("sumo-gui", args);
// 		} else {
// 			// Use execfe to start the sumo process
// 			execfe("sumo", args);
// 		}
// 	#elif defined(_WIN32)
// 		// Windows
// 		STARTUPINFO si{};
// 		PROCESS_INFORMATION pi{};
// 		std::string command = "sumo";
// 		if (gui) {
// 			command += "-gui";
// 		}
// 		std::string commandLine;
// 		for (const auto& arg : args) {
// 			commandLine += arg;
// 			commandLine += " ";
// 		}
// 		if (CreateProcess(nullptr, commandLine.data(), nullptr, nullptr, FALSE, 0, nullptr, nullptr,
// &si, &pi)) { 			CloseHandle(pi.hThread); 			CloseHandle(pi.hProcess); 		} else {
// 			// Failed to start the sumo process
// 			return false;
// 		}
// 	#else
// 		// Unsupported operating system
// 		return false;
// 	#endif

// 	return true;
// }


// auto parse_configuration(const std::filesystem::path& configuration_file_path) -> ProgramOptions
// {
auto parse_configuration(const toml::parse_result& config) -> ProgramOptions {
	// const auto config = [&]() {
	// 	try {
	// 		const auto config = toml::parse("config.toml");
	// 		return config;
	// 	}
	// 	catch (const toml::parse_error& err) {
	// 		std::cerr << "Parsing error: " << err << "\n";
	// 		std::exit(1);
	// 		// Handle error...
	// 	}
	// }();


	// const auto config = toml::parse_file(configuration_file_path.string());
	const auto port = config["port"].value_or(10001);
	if (! between(port, 0, std::numeric_limits<u16>::max())) {
		spdlog::error("port must be between 0 and {}", std::numeric_limits<u16>::max());
		std::exit(1);
	}

	const auto sumo_port = config["sumo"]["port"].value_or(10000);
	if (! between(sumo_port, 0, std::numeric_limits<u16>::max())) {
		spdlog::error("sumo.port must be between 0 and {}", std::numeric_limits<u16>::max());
		std::exit(1);
	}

	const auto sumocfg_path = config["sumo"]["sumocfg-path"].value_or(""sv);
	if (sumocfg_path.empty()) {
		spdlog::error("sumo.sumocfg-path must be set");
		std::exit(1);
	}

	// Check that the sumocfg file exists
	if (! std::filesystem::exists(sumocfg_path)) {
		spdlog::error("SUMO configuration file not found: {}", sumocfg_path);
		std::exit(1);
	}

	const auto osm_path = config["sumo"]["osm-path"].value_or(""sv);
	if (osm_path.empty()) {
		spdlog::error("sumo.osm-path must be set");
		std::exit(1);
	}

	// Check that the osm file exists
	if (! std::filesystem::exists(osm_path)) {
		spdlog::error("{}:{} OSM file not found: {}", __FILE__, __LINE__, osm_path);
		std::exit(1);
	}

	const auto simulation_steps = config["sumo"]["simulation-steps"].value_or(10000);
	if (simulation_steps <= 0) {
		spdlog::error("sumo.simulation-steps must be positive");
		std::exit(1);
	}

	const bool use_sumo_gui = config["sumo"]["spawn"]["gui"].value_or(false);
	const bool spawn_sumo = config["sumo"]["spawn"]["enabled"].value_or(false);
	if (use_sumo_gui && ! spawn_sumo) {
		spdlog::error("sumo.spawn.gui is true, but sumo.spawn.enabled is false");
		std::exit(1);
	}

	const i32 streetlamp_distance_threshold =
		config["sumo"]["streetlamps"]["distance-threshold"].value_or(10);

	if (streetlamp_distance_threshold <= 0) {
		spdlog::error("sumo.streetlamps.distance-threshold must be positive");
		std::exit(1);
	}

	const bool verbose = config["verbose"].value_or(false);
	if (verbose) {
		std::cout << toml::json_formatter {config} << "\n";
	}

	return ProgramOptions {
		.port = static_cast<u16>(port),
		.verbose = verbose,
		.sumo_port = static_cast<u16>(sumo_port),
		.simulation_steps = simulation_steps,
		.sumocfg_path = std::filesystem::absolute(sumocfg_path),
		.osm_path = std::filesystem::absolute(osm_path),
		.use_sumo_gui = use_sumo_gui,
		.spawn_sumo = spawn_sumo,
		.streetlamp_distance_threshold = streetlamp_distance_threshold,
	};
}

namespace topics {
	static const auto cars = std::string("cars");
	static const auto streetlamps = std::string("streetlamps");
}; // namespace topics

struct Topic {
	std::string name;
	int			publish_rate = 0;
	bool		enabled = false;
};

auto pformat(const Topic& topic) -> std::string {
	return fmt::format("Topic {{ name: {}, publish_rate: {}, enabled: {} }}", topic.name,
					   topic.publish_rate, topic.enabled);
}

auto pprint(const Topic& topic) -> void {
	fmt::println("{}", pformat(topic));
}

auto main(int argc, char** argv) -> int {
	const auto configuration_file_path = std::filesystem::path("config.toml");
	if (! std::filesystem::exists(configuration_file_path)) {
		spdlog::error("Configuration file not found: {}", configuration_file_path.string());
		std::exit(1);
	}
	// const auto config = toml::parse_file("configuration.toml");
	// you can also iterate more 'traditionally' using a ranged-for
	// get key-value pairs
	// std::string_view library_name = config["library"]["name"].value_or(""sv);
	// std::string_view library_author = config["library"]["authors"][0].value_or(""sv);
	// int64_t depends_on_cpp_version = config["dependencies"]["cpp"].value_or(0);
	// fmt::print("library_name := {}\n", library_name);
	// fmt::print("library_author := {}\n", library_author);
	// fmt::print("depends_on_cpp_version := {}\n", depends_on_cpp_version);

	const auto argv_parser = create_argv_parser();
	const auto print_help = [&]() -> void {
		std::cerr << argv_parser;
	};

	const auto config = toml::parse_file(configuration_file_path.string());
	const auto options = parse_configuration(config);
	// const auto options = parse_args(argv_parser, argc, argv)
	// 						 .map_error([&](const auto& err) {
	// 							 spdlog::error("{}", err);
	// 							 print_help();
	// 							 std::exit(2);
	// 						 })
	//  .value();

	pprint(options);

	const auto topic_cars = Topic {
		.name = config["topics"]["cars"].value_or("cars"),
		.publish_rate = config["topics"]["cars"]["publish-rate"].value_or(0),
		.enabled = config["topics"]["cars"]["enabled"].value_or(false),
	};

	if (topic_cars.publish_rate <= 0) {
		spdlog::error("topics.cars.publish-rate must be positive");
		std::exit(1);
	}

	pprint(topic_cars);

	const auto topic_streetlamps = Topic {
		.name = config["topics"]["streetlamps"].value_or("streetlamps"),
		.publish_rate = config["topics"]["streetlamps"]["publish-rate"].value_or(0),
		.enabled = config["topics"]["streetlamps"]["enabled"].value_or(false),
	};

	if (topic_streetlamps.publish_rate <= 0) {
		spdlog::error("topics.streetlamps.publish-rate must be positive");
		std::exit(1);
	}

	pprint(topic_streetlamps);

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

	const auto cwd = std::filesystem::current_path();

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

	// phmap::flat_hash_map<int, Car> cars;
	phmap::parallel_flat_hash_map<int, Car> cars;

	zmq::context_t zmq_ctx;
	zmq::socket_t  sock(zmq_ctx, zmq::socket_type::pub);
	// FIXME: do not use tcp maybe ipc://
	const std::string addr = fmt::format("tcp://*:{}", options.port);
	sock.bind(addr);
	spdlog::info("Bound zmq PUB socket to {}", addr);

	const int num_retries_sumo_sim_connect = 100;
	Simulation::init(options.sumo_port, num_retries_sumo_sim_connect, "localhost");
	const double dt = Simulation::getDeltaT();

	auto streetlamps =
		extract_streetlamps_from_osm(options.osm_path)
			.map_error([](const auto& err) {
				if (err == extract_streetlamps_from_osm_error::file_not_found) {
					spdlog::error("{}:{} OSM file not found", __FILE__, __LINE__);
				} else if (err == extract_streetlamps_from_osm_error::xml_parse_error) {
					spdlog::error("Failed to parse OSM file");
				}
				std::exit(1);
			})
			.value();

	// Change each street lamp's lon/lat into x/y
	// We only need to do this once, as the street lamps are static
	// We need to do this since the OpenStreetMap file contains lon/lat coordinates of the street
	// lamps but the SUMO simulation uses x/y coordinates for the vehicles We do this here an not in
	// the parsing step because we need to have the simulation running to convert lon/lat to x/y
	for (auto& lamp : streetlamps) {
		const auto geo = Simulation::convertGeo(lamp.lon, lamp.lat, true);
		lamp.lon = geo.x;
		lamp.lat = geo.y;
	}

	spdlog::info("streetlamps.size(): {}", streetlamps.size());
	spdlog::info("dt: {}", dt);

	auto bar = indicators::BlockProgressBar {
		indicators::option::BarWidth {80},
		indicators::option::Start {"|"},
		indicators::option::End {"|"},
		indicators::option::ShowElapsedTime {true},
		indicators::option::ForegroundColor {indicators::Color::blue},
	};

	// Hide cursor
	indicators::show_console_cursor(false);


	// Preallocate memory for the streetlamp_ids_with_vehicles_nearby vector
	auto streetlamp_ids_with_vehicles_nearby = std::vector<std::int64_t>(streetlamps.size(), 0);

	const auto n_hardware_threads = std::thread::hardware_concurrency();
	spdlog::info("n_threads: {}", n_hardware_threads);
	const auto n_threads_in_pool = n_hardware_threads - 1;

	// Constructs a thread pool with as many threads as available in the hardware.
	BS::thread_pool pool(n_threads_in_pool);
	spdlog::info("Created thread pool with {} threads", pool.get_thread_count());

	const auto streetlamp_distance_threshold_doubled =
		std::pow(options.streetlamp_distance_threshold, 2);
	// TODO: detect signed overflow
	// deallocate-inactive-cars-every
	const auto do_deallocation_pass_every_n_steps =
		config["sumo"]["deallocate-inactive-cars-every"].value_or(1000);
	if (do_deallocation_pass_every_n_steps <= 0) {
		spdlog::error("sumo.deallocate-inactive-cars-every must be positive");
		std::exit(1);
	}

	int do_deallocation_pass_at_step = do_deallocation_pass_every_n_steps;

	// const auto t_sim_start = std::chrono::high_resolution_clock::now();
	const auto sim_timer = Timer {};

	for (int simulation_step = 0; simulation_step < options.simulation_steps; ++simulation_step) {
		// Keep track of the accumelated time of the simulation
		// const auto t_start = std::chrono::high_resolution_clock::now();
		const auto sim_step_timer = Timer {};
		Simulation::step();

		{ // Get (x,y, theta) of all vehicles
			const auto vehicles_ids = Vehicle::getIDList();

			for (const auto& id : vehicles_ids) {
				// TODO: use std::from_chars instead of std::stoi
				const int id_as_int = std::stoi(id);
				// const
				cars.try_emplace(id_as_int, Car {});
				auto& car = cars[id_as_int];

				const auto position = Vehicle::getPosition(id);

				car.x = position.x;
				car.y = position.y;
				car.heading = Vehicle::getAngle(id);
				car.alive = true;
			}
		}

		if (simulation_step == do_deallocation_pass_at_step) {
			// Go through all cars and deallocate the memory of the ones that are not alive
			if (options.verbose) {
				fmt::print("Deallocating memory of dead cars\r");
			}
			const auto num_cars_before = cars.size();
			for (auto it = cars.begin(); it != cars.end();) {
				if (! it->second.alive) {
					it = cars.erase(it);
				} else {
					++it;
				}
			}
			const auto num_cars_after = cars.size();
			if (options.verbose) {
				fmt::println("Deallocated {} - {} = {} cars", num_cars_before, num_cars_after,
							 num_cars_before - num_cars_after);
			}
			do_deallocation_pass_at_step += do_deallocation_pass_every_n_steps;
		}

		std::atomic<int> num_streetlamps_with_vehicles_nearby = 0;
		// Check if any cars are close to a street lamp
		const auto look_for_cars_close_to_streetlamps = [&](const auto start, const auto end) {
			for (auto idx = start; idx < end; ++idx) {
				const auto lamp = streetlamps[idx];
				for (const auto& [_, car] : cars) {
					// TODO: maybe not as x and y are quite large numbers
					// TODO PERF: use squared distance instead of distance to avoid the sqrt call
					const double distance = (car.x - lamp.lon) * (car.x - lamp.lon) +
											(car.y - lamp.lat) * (car.y - lamp.lat);
					// const double distance = std::hypot(car.x - lamp.lon, car.y - lamp.lat);

					// streetlamp_distance_threshold_doubled
					if (distance <= streetlamp_distance_threshold_doubled) {
						streetlamp_ids_with_vehicles_nearby[num_streetlamps_with_vehicles_nearby] =
							lamp.id;
						num_streetlamps_with_vehicles_nearby++;
					}
				}
			}
		};

		auto multi_future =
			pool.parallelize_loop(0, streetlamps.size(), look_for_cars_close_to_streetlamps);

		{ // Publish information about the position and heading of all active cars
			// TODO: preallocate some of the memory structures used in this block
			// TODO: rate limit the publish rate
			static auto last_publish_time = std::chrono::high_resolution_clock::now();
			const auto	now = std::chrono::high_resolution_clock::now();
			const auto	elapsed =
				std::chrono::duration_cast<std::chrono::microseconds>(now - last_publish_time)
					.count();
			const auto publish_freq = (1.0 / static_cast<double>(topic_cars.publish_rate)) *
									  1e6; // FIXME: calculated wrong
			if (elapsed >= publish_freq) {
				// goto skip_publish;
				// const auto sleep_for =
				// 	std::chrono::microseconds(static_cast<int>(publish_freq - elapsed));
				// // spdlog::info("Sleeping for {} μs", sleep_for.count());
				// std::this_thread::sleep_for(sleep_for);
				// std::this_thread::yield();
				// spdlog::info("Sleeping for {} μs", static_cast<int>(publish_freq -
				// elapsed.count()));

				json j; // { "1": { "x": 1, "y": 2, "heading": 3 }, "2": { "x": 1, "y": 2, "heading": 3 } }
				// Find all cars that are alive and put them into a json object
				for (auto& [vehicle_id, car] : cars) {
					if (car.alive) {
						car.alive = false; // Reset the alive flag to prevent staying alive forever
						j[std::to_string(vehicle_id)] = car.to_json();
					}
				}

				// Serialize to CBOR encoding format
				const std::vector<u8> v = json::to_cbor(j);
				const std::string	  payload(v.begin(), v.end());
				// Publish the data to all clients
				const auto flags = zmq::send_flags::none;
				const auto send_result = sock.send(zmq::buffer(topics::cars + payload), flags);
				if (! send_result.has_value()) {
					spdlog::error("{}:{} Failed to publish data on topic {}", __FILE__, __LINE__,
								topics::cars);
				}
			}

			last_publish_time = now;
			{
				static auto num_messages_published_last_second = 0;
				num_messages_published_last_second++;
				static auto last_report_publish_rate_time =
					std::chrono::high_resolution_clock::now();
				const auto now = std::chrono::high_resolution_clock::now();
				const auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
					now - last_report_publish_rate_time);
				if (elapsed.count() >= 1) {
					spdlog::info(
						"Published data {} times the last second on topic {} at a rate of {} Hz",
						num_messages_published_last_second, topics::cars, topic_cars.publish_rate);
					last_report_publish_rate_time = now;
					num_messages_published_last_second = 0;
				}
			}
		}
		// :skip_publish

		// Wait for all threads in the thread pool to finish
		// NOTE: We do this here after the code that generates the data of all alive cars, to have
		// the main thread do something while we wait for the other threads to finish This is better
		// than calling .wait() right after the call to pool.parallelize_loop()
		multi_future.wait();

		{ // Publish information about which street lamps that have vehicles nearby
			json array = json::array();
			for (int idx = 0; idx < num_streetlamps_with_vehicles_nearby; idx++) {
				array.push_back(streetlamp_ids_with_vehicles_nearby[idx]);
			}
			// Serialize to CBOR encoding format
			const std::vector<u8> v = json::to_cbor(array);
			const std::string	  payload(v.begin(), v.end());
			// Send the data to all clients
			const auto flags = zmq::send_flags::none;
			const auto send_result = sock.send(zmq::buffer(topics::streetlamps + payload), flags);
			if (! send_result.has_value()) {
				spdlog::error("{}:{} Failed to publish data on topic {}", __FILE__, __LINE__,
							  topics::streetlamps);
			}
		}

		{ // Update the progress bar
			const double percent_done =
				static_cast<double>(simulation_step) / options.simulation_steps * 100.0;
			bar.set_progress(percent_done);
			if (options.verbose) {
				// const auto t_end = std::chrono::high_resolution_clock::now();
				// const auto duration = std::chrono::duration_cast<std::chrono::microseconds>(t_end
				// - t_start);
				const auto duration = sim_step_timer.elapsed_us();
				// Estimate the remaining time of the simulation
				static long duration_avg = 0;
				duration_avg = (duration_avg + duration) / 2;
				const auto remaining_time_estimate = std::chrono::microseconds(
					duration_avg * (options.simulation_steps - simulation_step));
				const auto postfix =
					fmt::format("simulation-step: {}/{} (in percent: {:.2f}%) took: {} μs, "
								"estimated time to completion: {}",
								simulation_step, options.simulation_steps, percent_done, duration,
								humantime(remaining_time_estimate.count()));
				bar.set_option(indicators::option::PostfixText(postfix));
			}
		}
	}

	bar.set_progress(100.0);
	bar.mark_as_completed();
	indicators::show_console_cursor(true);

	Simulation::close();

	spdlog::info("Simulation took: {}", humantime(sim_timer.elapsed_us()));

	return 0;
}
