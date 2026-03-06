#pragma once

#include <string>
#include <vector>

struct AppConfig {
    std::vector<std::string> statuses;
    std::string              default_status;
    std::string              db_path;

    bool isValidStatus(const std::string& s) const;
};

struct ConfigLoader {
    static AppConfig load(const std::string& filepath);
    static AppConfig defaults();
};
