#include <gtest/gtest.h>
#include "../src/config.h"

#include <cstdio>
#include <fstream>

// ── helpers ──────────────────────────────────────────────────────────────────

static std::string write_tmp(const std::string& content) {
    char path[] = "/tmp/test_config_XXXXXX.json";
    // mkstemp variant for .json suffix – just write a fixed name
    static int counter = 0;
    std::string p = "/tmp/test_config_" + std::to_string(counter++) + ".json";
    std::ofstream f(p);
    f << content;
    return p;
}

// ── tests ─────────────────────────────────────────────────────────────────────

TEST(ConfigLoader, LoadsValidFile) {
    std::string path = write_tmp(R"({
        "statuses": ["todo","in_progress","done"],
        "default_status": "todo",
        "db_path": "mydb.db"
    })");
    AppConfig cfg = ConfigLoader::load(path);
    EXPECT_EQ(cfg.statuses.size(), 3u);
    EXPECT_EQ(cfg.default_status, "todo");
    EXPECT_EQ(cfg.db_path, "mydb.db");
    std::remove(path.c_str());
}

TEST(ConfigLoader, DefaultsAreReasonable) {
    AppConfig cfg = ConfigLoader::defaults();
    EXPECT_FALSE(cfg.statuses.empty());
    EXPECT_FALSE(cfg.default_status.empty());
    EXPECT_FALSE(cfg.db_path.empty());
    EXPECT_TRUE(cfg.isValidStatus(cfg.default_status));
}

TEST(ConfigLoader, IsValidStatus) {
    AppConfig cfg = ConfigLoader::defaults();
    EXPECT_TRUE(cfg.isValidStatus("todo"));
    EXPECT_FALSE(cfg.isValidStatus("nonexistent"));
    EXPECT_FALSE(cfg.isValidStatus(""));
}

TEST(ConfigLoader, ThrowsOnMissingFile) {
    EXPECT_THROW(ConfigLoader::load("/nonexistent/path/config.json"), std::runtime_error);
}

TEST(ConfigLoader, ThrowsOnBadJson) {
    std::string path = write_tmp("not valid json {{{{");
    EXPECT_THROW(ConfigLoader::load(path), std::runtime_error);
    std::remove(path.c_str());
}
