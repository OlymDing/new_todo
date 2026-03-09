#include "session.h"

#include <ctime>
#include <fstream>
#include <sstream>
#include <unistd.h>

// ── helpers ───────────────────────────────────────────────────────────────────

static std::string sanitizeTty(const std::string &tty)
{
  std::string s = tty;
  for (char &c : s)
    if (c == '/' || c == ' ')
      c = '_';
  return s;
}

// ── SessionManager ────────────────────────────────────────────────────────────

std::string SessionManager::sessionPath() const
{
  // Use the TTY name so each terminal window gets its own session.
  const char *tty = ttyname(STDIN_FILENO);
  if (!tty)
    return "";
  return "/tmp/new_todo_session_" + sanitizeTty(tty);
}

std::optional<SessionData> SessionManager::load() const
{
  std::string path = sessionPath();
  if (path.empty())
    return std::nullopt;

  std::ifstream f(path);
  if (!f.is_open())
    return std::nullopt;

  SessionData s;
  std::string username;
  int64_t expires_at = 0;
  if (!(f >> s.user_id >> expires_at))
    return std::nullopt;
  f.ignore(1); // skip newline after expires_at
  if (!std::getline(f, s.username))
    return std::nullopt;

  int64_t now = static_cast<int64_t>(std::time(nullptr));
  if (now >= expires_at)
  {
    // Expired — clean up the stale file.
    std::remove(path.c_str());
    return std::nullopt;
  }

  s.expires_at = expires_at;
  return s;
}

void SessionManager::save(int64_t user_id, const std::string &username) const
{
  std::string path = sessionPath();
  if (path.empty())
    return;

  int64_t expires_at =
      static_cast<int64_t>(std::time(nullptr)) + TTL;

  std::ofstream f(path, std::ios::trunc);
  if (!f.is_open())
    return;
  f << user_id << "\n" << expires_at << "\n" << username << "\n";
}

void SessionManager::clear() const
{
  std::string path = sessionPath();
  if (!path.empty())
    std::remove(path.c_str());
}
