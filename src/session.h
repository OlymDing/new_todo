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

  // Load an existing, non-expired session for this terminal.
  // Returns nullopt if no session file exists or it has expired.
  std::optional<SessionData> load() const;

  // Persist a new session for this terminal.
  void save(int64_t user_id, const std::string &username) const;

  // Delete the session file (logout).
  void clear() const;

private:
  // Path of the session file, keyed to the current TTY.
  // Returns empty string when stdin is not a TTY.
  std::string sessionPath() const;
};
