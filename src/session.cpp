#include "session.h"

#include <ctime>
#include <fstream>

// ── SessionManager ────────────────────────────────────────────────────────────

SessionManager::SessionManager(const std::string &path) : path_(path)
{
}

std::optional<SessionData> SessionManager::load() const
{
  std::ifstream f(path_);
  if (!f.is_open())
    return std::nullopt;

  SessionData s;
  int64_t expires_at = 0;
  if (!(f >> s.user_id >> expires_at))
    return std::nullopt;
  f.ignore(1); // skip newline after expires_at
  if (!std::getline(f, s.username))
    return std::nullopt;

  int64_t now = static_cast<int64_t>(std::time(nullptr));
  if (now >= expires_at)
  {
    std::remove(path_.c_str());
    return std::nullopt;
  }

  s.expires_at = expires_at;
  return s;
}

void SessionManager::save(int64_t user_id, const std::string &username) const
{
  int64_t expires_at = static_cast<int64_t>(std::time(nullptr)) + TTL;

  std::ofstream f(path_, std::ios::trunc);
  if (!f.is_open())
    return;
  f << user_id << "\n" << expires_at << "\n" << username << "\n";
}

void SessionManager::clear() const { std::remove(path_.c_str()); }
