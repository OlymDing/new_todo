add_rules("mode.debug", "mode.release")
set_languages("c++17")

-- ftxui packages (kept for future tui frontend)
add_requires("ftxui-dom",       {system = true})
add_requires("ftxui-component", {system = true})
add_requires("ftxui-screen",    {system = true})
-- dependencies
add_requires("sqlite3",    {system = true})
add_requires("readline",   {system = true})
add_requires("gtest",      {system = true})
add_requires("gtest_main", {system = true})
add_requires("openssl",    {system = true})

-- domain library (unchanged)
target("todo_lib")
    set_kind("static")
    add_files("src/todo.cpp", "src/db.cpp", "src/config.cpp",
              "src/todo_service.cpp", "src/auth.cpp", "src/session.cpp")
    add_packages("sqlite3", "openssl")
    add_includedirs("src", "/usr/local/include")

-- CLI layer library
target("cli_lib")
    set_kind("static")
    add_files("src/cli/*.cpp")
    add_deps("todo_lib")
    add_packages("readline", "ftxui-component", "ftxui-dom", "ftxui-screen")
    add_includedirs("src", "/usr/local/include")

-- TUI layer library
target("tui_lib")
    set_kind("static")
    add_files("src/tui/*.cpp")
    add_deps("todo_lib")
    add_packages("ftxui-component", "ftxui-dom", "ftxui-screen")
    add_includedirs("src", "/usr/local/include")

-- main binary
target("new_todo")
    set_kind("binary")
    add_files("src/main.cpp")
    add_deps("cli_lib", "tui_lib", "todo_lib")
    add_packages("sqlite3", "readline", "ftxui-component", "ftxui-dom",
                 "ftxui-screen", "openssl")
    add_includedirs("src", "/usr/local/include")

-- test targets (unchanged)
target("test_todo")
    set_kind("binary")
    add_files("tests/test_todo.cpp")
    add_deps("todo_lib")
    add_packages("gtest", "gtest_main", "sqlite3", "openssl")
    add_includedirs("src", "/usr/local/include")

target("test_db")
    set_kind("binary")
    add_files("tests/test_db.cpp")
    add_deps("todo_lib")
    add_packages("gtest", "gtest_main", "sqlite3", "openssl")
    add_includedirs("src", "/usr/local/include")

target("test_config")
    set_kind("binary")
    add_files("tests/test_config.cpp")
    add_deps("todo_lib")
    add_packages("gtest", "gtest_main", "openssl")
    add_includedirs("src", "/usr/local/include")

target("test_service")
    set_kind("binary")
    add_files("tests/test_service.cpp")
    add_deps("todo_lib")
    add_packages("gtest", "gtest_main", "sqlite3", "openssl")
    add_includedirs("src", "/usr/local/include")

target("test_auth")
    set_kind("binary")
    add_files("tests/test_auth.cpp")
    add_deps("todo_lib")
    add_packages("gtest", "gtest_main", "sqlite3", "openssl")
    add_includedirs("src", "/usr/local/include")
