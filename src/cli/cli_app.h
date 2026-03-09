#pragma once

#include "config.h"
#include "db.h"
#include "todo_service.h"

#include <optional>
#include <string>
#include <vector>

namespace cli
{

class CliApp
{
public:
  CliApp(Database &db, const AppConfig &cfg);
  int run(int argc, char **argv);

private:
  Database &db_;
  const AppConfig &cfg_;
  TodoService svc_;

  static std::optional<std::string>
  parse_flag(const std::vector<std::string> &args, const std::string &name);

  static std::vector<std::string> tokenize(const std::string &line);

  int dispatch(
      const std::string &cmd, const std::string &prog,
      const std::vector<std::string> &args
  );

  int run_loop(const std::string &prog); // interactive REPL loop
  int show_selector();                   // FTXUI selector: 0=TUI, 1=CLI
};

} // namespace cli
