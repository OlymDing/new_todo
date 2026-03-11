#pragma once

#include <cstdint>
#include <optional>
#include <string>

struct SessionData
{
  int64_t user_id;
  std::string username;
  int64_t expires_at; // Unix timestamp
};

class SessionManager
{
public:
  // Session is valid for this many seconds after login (default: 8 hours).
  static constexpr int64_t TTL = 8 * 3600;

  // path: absolute path to the session file.
  explicit SessionManager(const std::string &path);

  // Load an existing, non-expired session.
  // Returns nullopt if no session file exists or it has expired.
  std::optional<SessionData> load() const;

  // Persist a new session.
  void save(int64_t user_id, const std::string &username) const;

  // Delete the session file (logout).
  void clear() const;

  const std::string &path() const { return path_; }

private:
  std::string path_;
};
