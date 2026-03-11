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
  CliApp(Database &db, const AppConfig &cfg, const std::string &session_path);

  // Checks session → env vars → FTXUI login screen. Returns user_id or 0.
  int64_t authenticate();

  // Run a single command (argc/argv) or drop into REPL if no command given.
  // Requires a valid user_id from authenticate().
  int run(int argc, char **argv, int64_t user_id);

private:
  Database &db_;
  const AppConfig &cfg_;
  AuthService auth_;
  SessionManager session_;
  std::optional<TodoService> svc_;

  static std::optional<std::string>
  parse_flag(const std::vector<std::string> &args, const std::string &name);

  static std::vector<std::string> tokenize(const std::string &line);

  int dispatch(
      const std::string &cmd, const std::string &prog,
      const std::vector<std::string> &args
  );

  int run_loop(const std::string &prog);
  int64_t show_login_screen();
};

} // namespace cli
