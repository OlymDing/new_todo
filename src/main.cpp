#include "config.h"
#include "db.h"
#include "cli/cli_app.h"
#include <unistd.h>
#include <limits.h>
#include <string>
#include <iostream>

std::string GetExecutablePath() {
    char result[PATH_MAX];
    ssize_t count = readlink("/proc/self/exe", result, PATH_MAX);
    std::string path = std::string(result, (count > 0) ? count : 0);
    return path.substr(0, path.find_last_of("\\/"));
}

int main(int argc, char **argv)
{
  AppConfig cfg;
  try
  {
    cfg = ConfigLoader::load("config.json");
  }
  catch (...)
  {
    cfg = ConfigLoader::defaults();
  }

  std::cout << GetExecutablePath();
  Database db(cfg.db_path);
  cli::CliApp app(db, cfg, "session");
  return app.run(argc, argv);
}
