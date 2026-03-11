#include "cli/cli_app.h"
#include "cli/commands.h"
#include "cli/formatter.h"

#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <termios.h>
#include <unistd.h>
#include <readline/readline.h>
#include <readline/history.h>

namespace cli
{

CliApp::CliApp(Database &db, const AppConfig &cfg, const std::string &session_path)
    : db_(db), cfg_(cfg), auth_(db_), session_(session_path)
{
}

int64_t CliApp::authenticate()
{
  // 1. Reuse an active session.
  if (auto s = session_.load())
    return s->user_id;

  // 2. Non-interactive: honour TODO_USER / TODO_PASS environment variables.
  const char *env_user = std::getenv("TODO_USER");
  const char *env_pass = std::getenv("TODO_PASS");
  if (env_user && env_pass)
  {
    auto u = auth_.login(env_user, env_pass);
    if (u)
    {
      session_.save(u->id, u->username);
      return u->id;
    }
    std::cerr << "Authentication failed for user '" << env_user << "'.\n";
    return 0;
  }

  // 3. Interactive: show FTXUI login screen.
  return show_login_screen();
}

int CliApp::run(int argc, char **argv, int64_t user_id)
{
  std::string prog = argv[0];

  svc_.emplace(db_, cfg_, user_id);

  if (argc >= 2)
  {
    std::string cmd = argv[1];
    std::vector<std::string> args(argv + 2, argv + argc);
    try
    {
      return dispatch(cmd, prog, args);
    }
    catch (const std::invalid_argument &e)
    {
      std::cerr << "Error: " << e.what() << "\n";
      return 1;
    }
    catch (const std::exception &e)
    {
      std::cerr << "Fatal: " << e.what() << "\n";
      return 2;
    }
  }

  return run_loop(prog);
}

int64_t CliApp::show_login_screen()
{
  // Helper: read a password without echoing characters.
  auto read_password = [](const std::string &prompt) -> std::string
  {
    std::cout << prompt << std::flush;
    struct termios old_t, new_t;
    tcgetattr(STDIN_FILENO, &old_t);
    new_t = old_t;
    new_t.c_lflag &= ~(tcflag_t)ECHO;
    tcsetattr(STDIN_FILENO, TCSANOW, &new_t);
    std::string pw;
    std::getline(std::cin, pw);
    tcsetattr(STDIN_FILENO, TCSANOW, &old_t);
    std::cout << "\n";
    return pw;
  };

  while (true)
  {
    std::cout << "\n  new_todo\n"
              << "  [1] Login\n"
              << "  [2] Register\n"
              << "  [q] Quit\n"
              << "> " << std::flush;

    std::string choice;
    if (!std::getline(std::cin, choice) || choice == "q")
      return 0;

    if (choice == "1")
    {
      std::cout << "Username: " << std::flush;
      std::string username;
      if (!std::getline(std::cin, username)) return 0;
      std::string password = read_password("Password: ");

      try
      {
        auto u = auth_.login(username, password);
        if (u)
        {
          session_.save(u->id, u->username);
          return u->id;
        }
        std::cerr << "  Invalid username or password.\n";
      }
      catch (const std::exception &e) { std::cerr << "  Error: " << e.what() << "\n"; }
    }
    else if (choice == "2")
    {
      std::cout << "Username: " << std::flush;
      std::string username;
      if (!std::getline(std::cin, username)) return 0;
      std::string password        = read_password("Password: ");
      std::string password_confirm = read_password("Confirm password: ");

      if (password != password_confirm) { std::cerr << "  Passwords do not match.\n"; continue; }

      try
      {
        int64_t id = auth_.registerUser(username, password);
        session_.save(id, username);
        return id;
      }
      catch (const std::exception &e) { std::cerr << "  Error: " << e.what() << "\n"; }
    }
    else
    {
      std::cerr << "  Please enter 1, 2, or q.\n";
    }
  }
}

std::optional<std::string> CliApp::parse_flag(
    const std::vector<std::string> &args, const std::string &name
)
{
  for (size_t i = 0; i + 1 < args.size(); ++i)
  {
    if (args[i] == name)
      return args[i + 1];
  }
  return std::nullopt;
}

std::vector<std::string> CliApp::tokenize(const std::string &line)
{
  std::vector<std::string> tokens;
  std::string current;
  bool in_single = false;
  bool in_double = false;

  for (size_t i = 0; i < line.size(); ++i)
  {
    char c = line[i];
    if (c == '\'' && !in_double)
      in_single = !in_single;
    else if (c == '"' && !in_single)
      in_double = !in_double;
    else if (c == ' ' && !in_single && !in_double)
    {
      if (!current.empty()) { tokens.push_back(current); current.clear(); }
    }
    else
      current += c;
  }
  if (!current.empty())
    tokens.push_back(current);
  return tokens;
}

int CliApp::run_loop(const std::string &prog)
{
  std::cerr << "new_todo interactive mode. Type 'help' or 'quit' to exit.\n";

  while (true)
  {
    char *raw = readline("todo> ");
    if (raw == nullptr) { std::cerr << "\n"; break; }

    std::string line(raw);
    free(raw);

    size_t start = line.find_first_not_of(" \t");
    if (start == std::string::npos) continue;
    size_t end = line.find_last_not_of(" \t");
    line = line.substr(start, end - start + 1);
    if (line.empty()) continue;

    add_history(line.c_str());

    std::vector<std::string> tokens = tokenize(line);
    if (tokens.empty()) continue;

    std::string cmd = tokens[0];
    if (cmd == "quit" || cmd == "exit") break;
    if (cmd == "help") { std::cerr << format_usage(prog); continue; }

    std::vector<std::string> args(tokens.begin() + 1, tokens.end());
    try { dispatch(cmd, prog, args); }
    catch (const std::invalid_argument &e) { std::cerr << "Error: " << e.what() << "\n"; }
    catch (const std::exception &e)        { std::cerr << "Fatal: " << e.what() << "\n"; }
  }

  return 0;
}

int CliApp::dispatch(
    const std::string &cmd, const std::string &prog,
    const std::vector<std::string> &args
)
{
  if (cmd == "add")
  {
    if (args.empty()) { std::cerr << "add requires <title>\n"; return 1; }
    return cmd_add(*svc_, args[0], parse_flag(args, "--status"), parse_flag(args, "--note"));
  }
  else if (cmd == "add-child")
  {
    if (args.size() < 2) { std::cerr << "add-child requires <parent_id> <title>\n"; return 1; }
    return cmd_add_child(*svc_, std::stoll(args[0]), args[1],
                         parse_flag(args, "--status"), parse_flag(args, "--note"));
  }
  else if (cmd == "list")
    return cmd_list(*svc_, parse_flag(args, "--status"));
  else if (cmd == "show-tree")
    return cmd_show_tree(*svc_);
  else if (cmd == "update")
  {
    if (args.empty()) { std::cerr << "update requires <id>\n"; return 1; }
    return cmd_update(*svc_, std::stoll(args[0]), parse_flag(args, "--title"),
                      parse_flag(args, "--status"), parse_flag(args, "--note"));
  }
  else if (cmd == "delete")
  {
    if (args.empty()) { std::cerr << "delete requires <id>\n"; return 1; }
    return cmd_delete(*svc_, std::stoll(args[0]));
  }
  else if (cmd == "change-parent")
  {
    if (args.size() < 2)
    {
      std::cerr << "change-parent requires <id> <new_parent_id>  (use 0 for root)\n";
      return 1;
    }
    return cmd_change_parent(*svc_, std::stoll(args[0]), std::stoll(args[1]));
  }
  else if (cmd == "search")
  {
    if (args.empty()) { std::cerr << "search requires <query>\n"; return 1; }
    return cmd_search(*svc_, args[0]);
  }
  else if (cmd == "logout")
  {
    session_.clear();
    std::cout << "Logged out.\n";
    return 0;
  }
  else if (cmd == "show")
  {
    if (args.empty()) { std::cerr << "show requires <id>\n"; return 1; }
    return cmd_show(*svc_, std::stoll(args[0]));
  }
  else
  {
    std::cerr << "Unknown command: " << cmd << "\n";
    std::cerr << format_usage(prog);
    return 1;
  }
}

} // namespace cli
