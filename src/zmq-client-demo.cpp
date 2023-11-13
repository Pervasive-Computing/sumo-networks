#include <chrono>
using namespace std::chrono_literals;
#include <thread>

#include <argparse/argparse.hpp>
#include <fmt/core.h>
#include <nlohmann/json.hpp>
// for convenience
using json = nlohmann::json;
using namespace nlohmann::literals; // for ""_json
#include <spdlog/spdlog.h>
#include <zmq.hpp>


auto main(int argc, char** argv) -> int {
	auto argv_parser = argparse::ArgumentParser(__FILE__, "0.1.0");
	argv_parser.add_argument("-p", "--port")
		.default_value(11111)
		.scan<'i', int>()
		.help(fmt::format("Port used for the zeromq PUB server, constraints: 0 < port <= {}",
						  std::pow(2, 16) - 1));

	try {
		argv_parser.parse_args(argc, argv);
	} catch (const std::exception& err) {
		fmt::println("{}", err.what());
		return 2;
	}

	const auto port = argv_parser.get<int>("port");

	auto	   zmq_ctx = zmq::context_t();
	auto	   subscriber = zmq::socket_t(zmq_ctx, zmq::socket_type::sub);
	const auto addr = fmt::format("tcp://*:{}", port);
	subscriber.connect(addr);
	const auto topic = std::string("/cars");
	subscriber.set(zmq::sockopt::subscribe, topic);
	spdlog::info("Created zeromq SUB socket connected to addr: {} listening on topic: {}", addr,
				 topic);

	while (true) {
		zmq::message_t message;
		subscriber.recv(&message, ZMQ_RCVMORE);
		std::string text(static_cast<char*>(message.data()), message.size());
		fmt::println("Received {}", text);
		std::this_thread::sleep_for(10ms);
	}

	return 0;
}
