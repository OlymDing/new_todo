add_rules("mode.debug", "mode.release")
set_languages("c++17")

-- ftxui packages (kept but UI not implemented yet)
add_requires("ftxui-dom",       {system = true})
add_requires("ftxui-component", {system = true})
add_requires("ftxui-screen",    {system = true})
-- new dependencies
add_requires("sqlite3",    {system = true})
add_requires("gtest",      {system = true})
add_requires("gtest_main", {system = true})

-- main binary
target("new_todo")
    set_kind("binary")
    add_files("src/*.cpp")
    add_packages("ftxui-dom", "ftxui-component", "ftxui-screen", "sqlite3")
    add_includedirs("/usr/local/include")

-- testable static library (excludes main.cpp)
target("todo_lib")
    set_kind("static")
    add_files("src/todo.cpp", "src/db.cpp", "src/config.cpp", "src/todo_service.cpp")
    add_packages("sqlite3")
    add_includedirs("/usr/local/include")

-- test targets
target("test_todo")
    set_kind("binary")
    add_files("tests/test_todo.cpp")
    add_deps("todo_lib")
    add_packages("gtest", "gtest_main", "sqlite3")
    add_includedirs("/usr/local/include")

target("test_db")
    set_kind("binary")
    add_files("tests/test_db.cpp")
    add_deps("todo_lib")
    add_packages("gtest", "gtest_main", "sqlite3")
    add_includedirs("/usr/local/include")

target("test_config")
    set_kind("binary")
    add_files("tests/test_config.cpp")
    add_deps("todo_lib")
    add_packages("gtest", "gtest_main")
    add_includedirs("/usr/local/include")

target("test_service")
    set_kind("binary")
    add_files("tests/test_service.cpp")
    add_deps("todo_lib")
    add_packages("gtest", "gtest_main", "sqlite3")
    add_includedirs("/usr/local/include")
