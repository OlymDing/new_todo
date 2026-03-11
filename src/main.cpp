#include "config.h"
#include "db.h"
#include "cli/cli_app.h"
#include "tui/tui_app.h"

#include <unistd.h>
#include <limits.h>
#include <iostream>
#include <string>

static std::string GetExecutableDir()
{
  char result[PATH_MAX];
  ssize_t count = readlink("/proc/self/exe", result, PATH_MAX);
  std::string path(result, count > 0 ? count : 0);
  std::string dir = path.substr(0, path.find_last_of("\\/"));
  if (!dir.empty() && dir.back() != '/')
    dir += '/';
  return dir;
}

int main(int argc, char **argv)
{
  std::string exe_dir = GetExecutableDir();

  AppConfig cfg;
  try
  {
    cfg = ConfigLoader::load(exe_dir + "config.json");
  }
  catch (...)
  {
    cfg = ConfigLoader::defaults();
  }

  Database db(exe_dir + cfg.db_path);
  std::string session_path = exe_dir + "session";

  // Sub-command given: go straight to CLI (auth handled inside CliApp).
  if (argc >= 2)
  {
    cli::CliApp cli_app(db, cfg, session_path);
    int64_t user_id = cli_app.authenticate();
    if (user_id == 0) { std::cerr << "Authentication failed.\n"; return 1; }
    return cli_app.run(argc, argv, user_id);
  }

  // Interactive: ask which interface first, then let the chosen app handle auth.
  std::string choice;
  while (true)
  {
    std::cout << "Launch interface [tui/cli]: " << std::flush;
    if (!std::getline(std::cin, choice)) { std::cerr << "\n"; return 1; }

    if (choice == "tui")
    {
      tui::TuiApp tui_app(db, cfg, session_path);
      return tui_app.run();
    }
    if (choice == "cli")
    {
      cli::CliApp cli_app(db, cfg, session_path);
      int64_t user_id = cli_app.authenticate();
      if (user_id == 0) { std::cerr << "Authentication failed.\n"; return 1; }
      return cli_app.run(argc, argv, user_id);
    }

    std::cerr << "Please enter 'tui' or 'cli'.\n";
  }
}
