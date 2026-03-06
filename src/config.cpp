#include "config.h"

#include <algorithm>
#include <fstream>
#include <stdexcept>

#include <nlohmann/json.hpp>

bool AppConfig::isValidStatus(const std::string& s) const {
    return std::find(statuses.begin(), statuses.end(), s) != statuses.end();
}

AppConfig ConfigLoader::defaults() {
    return AppConfig{
        {"todo", "in_progress", "done"},
        "todo",
        "todo.db"
    };
}

AppConfig ConfigLoader::load(const std::string& filepath) {
    std::ifstream f(filepath);
    if (!f.is_open()) {
        throw std::runtime_error("Cannot open config file: " + filepath);
    }

    nlohmann::json j;
    try {
        f >> j;
    } catch (const nlohmann::json::parse_error& e) {
        throw std::runtime_error(std::string("JSON parse error: ") + e.what());
    }

    AppConfig cfg;
    cfg.statuses       = j.at("statuses").get<std::vector<std::string>>();
    cfg.default_status = j.at("default_status").get<std::string>();
    cfg.db_path        = j.at("db_path").get<std::string>();
    return cfg;
}
