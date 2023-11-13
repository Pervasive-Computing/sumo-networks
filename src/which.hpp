#pragma once

#include <string>
#include <sstream>
#include <vector>
#include <filesystem>

std::vector<std::string> which(const std::string& cmd) {
    std::string path = std::getenv("PATH");
    std::stringstream ss(path);
    std::string item;

    std::vector<std::string> paths = {};

    namespace fs = std::filesystem;

    while (std::getline(ss, item, ':')) {
        fs::path exec_path = fs::path(item) / cmd;
        if (!fs::exists(exec_path)) {
            continue;
        }

        const auto permissions = fs::status(exec_path).permissions();
        const auto executable = ((permissions & fs::perms::owner_exec) == fs::perms::owner_exec);
        if (executable) {
            paths.push_back(exec_path.string());
        }
    }

    return paths;
}
