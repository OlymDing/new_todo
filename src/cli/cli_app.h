#pragma once

#include "auth.h"
#include "config.h"
#include "db.h"
#include "session.h"
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
  AuthService auth_;
  SessionManager session_;
  // svc_ is constructed after login; use optional to allow deferred init.
  std::optional<TodoService> svc_;

  static std::optional<std::string>
  parse_flag(const std::vector<std::string> &args, const std::string &name);

  static std::vector<std::string> tokenize(const std::string &line);

  int dispatch(
      const std::string &cmd, const std::string &prog,
      const std::vector<std::string> &args
  );

  int run_loop(const std::string &prog); // interactive REPL loop
  int show_selector();                   // FTXUI selector: 0=TUI, 1=CLI

  // Returns the authenticated user_id, or 0 on failure.
  // Tries TODO_USER / TODO_PASS env vars first, then shows FTXUI login screen.
  int64_t authenticate();
  int64_t show_login_screen(); // FTXUI login/register; returns user_id or 0
};

} // namespace cli
