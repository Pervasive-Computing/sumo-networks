#include <filesystem>
#include <string>
#include <string_view>
#include <vector>
#include <charconv>


#include <tl/expected.hpp>
#include <sqlite3.h>
#include <spdlog/spdlog.h>
#include <zmq.hpp>
#include <toml.hpp>
#include <fmt/core.h>
#include <nlohmann/json.hpp>
// for convenience
using json = nlohmann::json;
using namespace nlohmann::literals; // for ""_json



class Database {
    public:
    Database(const std::filesystem::path& db_path) {
        const auto rc = sqlite3_open(db_path.c_str(), &db);
        if (rc) {
            spdlog::error("Can't open database: {}", sqlite3_errmsg(db));
            sqlite3_close(db);
            return;
        }
    }

    ~Database() {
        sqlite3_close(db);
    }

    private:
        sqlite3 *db;
};

enum class RequestType {
    weekly,
    daily,
};

struct Request {
    int streetlamp_id;
    RequestType type;
};

auto pformat(const Request& req) -> std::string {
    const auto type = req.type == RequestType::weekly ? "weekly" : "daily";
    return fmt::format("Request: id = {}, type = {}", req.streetlamp_id, type);
}

enum class ParseRequestError {
    invalid_delimiter,
    id_not_an_integer,
    request_type_not_recognized,
};

auto parse_request(std::string_view req) -> tl::expected<Request, ParseRequestError> {
    const auto delimiter_index = req.find("/");
    if (delimiter_index == std::string_view::npos) {
        return tl::make_unexpected(ParseRequestError::invalid_delimiter);
    }

    const auto id_substr = req.substr(0, delimiter_index);
    const auto type_substr = req.substr(delimiter_index + 1);
    int id;
    if (const auto res = std::from_chars(id_substr.data(), id_substr.data() + id_substr.size(), id); res.ec != std::errc()) {
        return tl::make_unexpected(ParseRequestError::id_not_an_integer);
    }

    if (type_substr == "weekly") {
        return Request{id, RequestType::weekly};
    } else if (type_substr == "daily") {
        return Request{id, RequestType::daily};
    } else {
        return tl::make_unexpected(ParseRequestError::request_type_not_recognized);
    }
}

auto main(int argc, char **argv) -> int {

    sqlite3 *db;
    const auto rc = sqlite3_open("streetlamps.db", &db);
    if (rc) {
        spdlog::error("Can't open database: {}", sqlite3_errmsg(db));
        sqlite3_close(db);
        return 1;
    }

    const auto create_table_sql = R"(
        CREATE TABLE IF NOT EXISTS streetlamps (
            id INTEGER PRIMARY KEY,
            lat REAL,
            lon REAL
        );
    )";

    constexpr std::string_view sql_create_streetlamps_table = R"(
        CREATE TABLE IF NOT EXISTS streetlamps (
            id INTEGER PRIMARY KEY,
            lat REAL,
            lon REAL
        );
    )";


    constexpr std::string_view sql_create_measurements_table = R"(
        CREATE TABLE IF NOT EXISTS measurements (
            id INTEGER PRIMARY KEY,
            streetlamp_id INTEGER,
            timestamp INTEGER,
            value REAL,
            FOREIGN KEY (streetlamp_id) REFERENCES streetlamps(id)
        );
    )";


    char *err_msg = nullptr;
    const auto create_table_rc = sqlite3_exec(db, create_table_sql, nullptr, nullptr, &err_msg);
    if (create_table_rc != SQLITE_OK) {
        spdlog::error("SQL error: {}", err_msg);
        sqlite3_free(err_msg);
        sqlite3_close(db);
        return 1;
    }

    // Insert some records

    zmq::context_t zmq_ctx;
	zmq::socket_t  sock(zmq_ctx, zmq::socket_type::rep);
	constexpr auto port = 13000;
    const std::string addr = fmt::format("tcp://*:{}", port);
	sock.bind(addr);
	spdlog::info("Bound zmq PUB socket to {}", addr);

    const auto send = [&](std::string_view msg) {
        zmq::message_t reply(msg.size());
        memcpy(reply.data(), msg.data(), msg.size());
        sock.send(reply, zmq::send_flags::none);
    };


    while (true) {
        zmq::message_t request;
        if (const auto res = sock.recv(request, zmq::recv_flags::none); !res) {
            spdlog::error("{}:{} Error receiving message", __FILE__, __LINE__);
            return 1;
        }

        auto msg = std::string_view(static_cast<char*>(request.data()), request.size());
        const auto req = parse_request(msg);
        if (!req) {
            const auto err_msg = fmt::format("Invalid request: {}", msg);
            send(err_msg);
            spdlog::error(err_msg);
            continue;
        }

        spdlog::info("Received request: {}", pformat(*req));

        // Send reply back to client
        std::string reply_str = "World";
        zmq::message_t reply(reply_str.size());
        memcpy(reply.data(), reply_str.data(), reply_str.size());
        sock.send(reply, zmq::send_flags::none);
    }

    return 0;
}
