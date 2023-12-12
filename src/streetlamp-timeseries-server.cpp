#include <charconv>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

#include <fmt/core.h>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>
#include <sqlite3.h>
#include <tl/expected.hpp>
#include <toml.hpp>
#include <zmq.hpp>
// for convenience
using json = nlohmann::json;
using namespace nlohmann::literals; // for ""_json



// {"jsonrpc": "2.0", "method": "lightlevel", "params": {"streetlamp": "123456", "reducer":
		// "mean|median", "per": "quarter|hour|day|week", "from": "2020-01-01T00:00:00Z", "to":
		// "2020-01-01T01:00:00Z"}, "id": 1}

struct RequestParams {
    int streetlamp;
    std::string reducer;
    std::string per;
    int from; // unix timestamp
    int to; // unix timestamp
};

auto pformat(const RequestParams& params) -> std::string {
    return fmt::format("{{\"streetlamp\": \"{}\", \"reducer\": \"{}\", \"per\": \"{}\", \"from\": \"{}\", \"to\": \"{}\"}}",
                       params.streetlamp, params.reducer, params.per, params.from, params.to);
}

struct Request {
    std::string jsonrpc;
    std::string method;
    RequestParams params;
    int id;
};

auto pformat(const Request& req) -> std::string {
    return fmt::format("{{\"jsonrpc\": \"{}\", \"method\": \"{}\", \"params\": {}, \"id\": {}}}",
                       req.jsonrpc, req.method, pformat(req.params), req.id);
}

enum class ParseRequestError {
    invalid_jsonrpc_version,
    unknown_method,
    invalid_params,
};

auto parse_request(std::string_view req) -> tl::expected<Request, ParseRequestError> {
    // Convert to a json object
    auto j = json::parse(req);
    // Validate json-rpc 2.0 request
    if (j["jsonrpc"] != "2.0") {
        return tl::make_unexpected(ParseRequestError::invalid_jsonrpc_version);
    }
    if (j["method"] != "lightlevel") {
        return tl::make_unexpected(ParseRequestError::unknown_method);
    }
    if (! j["params"].is_object()) {
        return tl::make_unexpected(ParseRequestError::invalid_params);
    }

    // Convert to a Request object
    Request r;
    r.jsonrpc = j["jsonrpc"];
    r.method = j["method"];
    r.id = j["id"];
    r.params.streetlamp = j["params"]["streetlamp"];
    r.params.reducer = j["params"]["reducer"];
    r.params.per = j["params"]["per"];
    r.params.from = j["params"]["from"];
    r.params.to = j["params"]["to"];

    return r;
}

struct Reply {
    std::string jsonrpc;
    std::vector<float> result;
    int id;
};

auto main(int argc, char** argv) -> int {
	const auto config_file_path = std::filesystem::path("config.toml");
	if (! std::filesystem::exists(config_file_path)) {
		spdlog::error("config.toml file not found in: {}",
					  std::filesystem::current_path().string());
		return 1;
	}

	toml::table config;
	try {
		config = toml::parse_file(config_file_path.string());
	} catch (const toml::parse_error& err) {
		spdlog::error("Parsing failed:\n{}", err.what());
		return 1;
	}

    const auto sqlite3_file_path = std::filesystem::path("streetlamps.sqlite3");
    if (! std::filesystem::exists(sqlite3_file_path)) {
        spdlog::error("sqlite3 database file not found in: {}",
                      std::filesystem::current_path().string());
        return 1;
    }

	sqlite3*   db;
	const auto rc = sqlite3_open(sqlite3_file_path.c_str(), &db);
	if (rc) {
		spdlog::error("Can't open database: {}", sqlite3_errmsg(db));
		sqlite3_close(db);
		return 1;
	}

	const auto num_streetlamps = [&]() -> int {
		constexpr auto sql_check_if_streetlamps_table_is_empty = R"(
        SELECT COUNT(*) FROM streetlamps;
    )";
		char*		   err_msg = nullptr;
		int			   n_rows = 0;
		const auto	   rc = sqlite3_exec(
			db, sql_check_if_streetlamps_table_is_empty,
			[](void* n_rows, int argc, char** argv, char** azColName) -> int {
				// TODO: use std::from_chars
				*static_cast<int*>(n_rows) = std::stoi(argv[0]);
				return 0;
			},
			&n_rows, &err_msg);

		if (rc != SQLITE_OK) {
			spdlog::error("SQL error: {}", err_msg);
			sqlite3_free(err_msg);
			std::exit(1);
		}

		return n_rows;
	}();

    if (num_streetlamps == 0) {
        spdlog::info("No streetlamps found in database");
        return 0;
    }

    spdlog::info("Found {} streetlamps in database", num_streetlamps);

	zmq::context_t	  zmq_ctx;
	zmq::socket_t	  sock(zmq_ctx, zmq::socket_type::rep);
	const auto	  port = config["server"]["streetlamps"]["port"].value_or(13000);
	const std::string addr = fmt::format("tcp://*:{}", port);
	sock.bind(addr);
	spdlog::info("Bound zmq PUB socket to {}", addr);

	const auto send = [&](std::string_view msg) {
		zmq::message_t reply(msg.size());
		memcpy(reply.data(), msg.data(), msg.size());
		sock.send(reply, zmq::send_flags::none);
	};

    Reply reply;

    sqlite3_stmt *get_light_levels_between_stmt;
    const auto sql_get_light_levels_between = R"(
        SELECT light_level FROM measurements
        WHERE streetlamp_id = ? AND timestamp BETWEEN ? AND ?
    )";

    if (sqlite3_prepare_v2(db, sql_get_light_levels_between, -1, &get_light_levels_between_stmt, nullptr) != SQLITE_OK) {
        spdlog::error("Failed to prepare statement: {}", sqlite3_errmsg(db));
        goto cleanup;
    }



	while (true) {
		zmq::message_t request;
		// Expect a json-rpc 2.0 request
		// {"jsonrpc": "2.0", "method": "lightlevel", "params": {"streetlamp": "123456", "reducer":
		// "mean|median", "per": "quarter|hour|day|week", "from": "1702396387", "to":
		// "1701791732"}, "id": 1}
		if (const auto res = sock.recv(request, zmq::recv_flags::none); ! res) {
			spdlog::error("{}:{} Error receiving message", __FILE__, __LINE__);
			return 1;
		}

		auto msg = std::string_view(static_cast<char*>(request.data()), request.size());

		const auto req = parse_request(msg);
		if (! req) {
			const auto err_msg = fmt::format("Invalid request: {}", msg);
			send(err_msg);
			spdlog::error(err_msg);
			continue;
		}

		spdlog::info("Received request: {}", pformat(*req));

        // query database for light levels of the given streetlamp
        sqlite3_bind_int(get_light_levels_between_stmt, 1, req->params.streetlamp);
        sqlite3_bind_int(get_light_levels_between_stmt, 2, req->params.from);
        sqlite3_bind_int(get_light_levels_between_stmt, 3, req->params.to);
        std::vector<float> light_levels;
        while (sqlite3_step(get_light_levels_between_stmt) == SQLITE_ROW) {
            light_levels.push_back(sqlite3_column_double(get_light_levels_between_stmt, 0));
        }
        sqlite3_reset(get_light_levels_between_stmt);

        spdlog::info("Found {} light levels for streetlamp {}", light_levels.size(), req->params.streetlamp);

        reply.jsonrpc = req->jsonrpc;
        reply.id = req->id;
        reply.result = {0.25, 0.56, 1.0, 0.0};

		// Send reply back to client
		// {"jsonrpc": "2.0", "result": [0.25, 0.56, 1.0, 0.0, ...], "id": 1}
        // Serialize reply to json
        const auto reply_json = json{
            {"jsonrpc", reply.jsonrpc},
            {"result", reply.result},
            {"id", reply.id},
        }.dump();

		zmq::message_t reply(reply_json.size());
		memcpy(reply.data(), reply_json.data(), reply_json.size());
		sock.send(reply, zmq::send_flags::none);
	}

	cleanup:
    sqlite3_finalize(get_light_levels_between_stmt);
    sqlite3_close(db);

	return 0;
}
