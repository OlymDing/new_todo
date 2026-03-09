#pragma once

#include "db.h"

#include <optional>
#include <string>

struct User
{
  int64_t id = 0;
  std::string username;
};

// Handles user registration, login, and password hashing.
// Passwords are stored as: hex(SHA-256(salt + password)), with a random salt.
class AuthService
{
public:
  explicit AuthService(Database &db);

  // Returns the new user's id, throws if username already exists.
  int64_t registerUser(const std::string &username, const std::string &password);

  // Returns the authenticated User, or nullopt on bad credentials.
  std::optional<User> login(const std::string &username,
                            const std::string &password);

private:
  Database &db_;

  static std::string randomSalt();
  static std::string hashPassword(const std::string &salt,
                                  const std::string &password);
};
