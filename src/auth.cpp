#include "auth.h"

#include <openssl/evp.h>
#include <openssl/rand.h>
#include <sqlite3.h>
#include <stdexcept>
#include <sstream>
#include <iomanip>

// ── helpers ──────────────────────────────────────────────────────────────────

static void check(int rc, const std::string &msg)
{
  if (rc != SQLITE_OK && rc != SQLITE_DONE && rc != SQLITE_ROW)
    throw std::runtime_error(msg + ": " + std::to_string(rc));
}

// ── AuthService ───────────────────────────────────────────────────────────────

AuthService::AuthService(Database &db) : db_(db)
{
  const char *sql = R"sql(
    CREATE TABLE IF NOT EXISTS users (
        id       INTEGER PRIMARY KEY AUTOINCREMENT,
        username TEXT    NOT NULL UNIQUE,
        salt     TEXT    NOT NULL,
        pw_hash  TEXT    NOT NULL
    );
  )sql";
  char *errmsg = nullptr;
  sqlite3 *raw = db_.rawHandle();
  int rc = sqlite3_exec(raw, sql, nullptr, nullptr, &errmsg);
  if (rc != SQLITE_OK)
  {
    std::string e = errmsg ? errmsg : "unknown";
    sqlite3_free(errmsg);
    throw std::runtime_error("AuthService schema init failed: " + e);
  }
}

int64_t AuthService::registerUser(const std::string &username,
                                  const std::string &password)
{
  if (username.empty())
    throw std::invalid_argument("Username must not be empty");
  if (password.empty())
    throw std::invalid_argument("Password must not be empty");

  std::string salt = randomSalt();
  std::string hash = hashPassword(salt, password);

  const char *sql =
      "INSERT INTO users (username, salt, pw_hash) VALUES (?, ?, ?)";
  sqlite3_stmt *stmt = nullptr;
  sqlite3 *raw = db_.rawHandle();
  check(sqlite3_prepare_v2(raw, sql, -1, &stmt, nullptr),
        "prepare registerUser");
  sqlite3_bind_text(stmt, 1, username.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 2, salt.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 3, hash.c_str(), -1, SQLITE_TRANSIENT);

  int rc = sqlite3_step(stmt);
  sqlite3_finalize(stmt);
  if (rc == SQLITE_CONSTRAINT)
    throw std::invalid_argument("Username already exists: " + username);
  if (rc != SQLITE_DONE)
    throw std::runtime_error("registerUser failed: " + std::to_string(rc));

  return sqlite3_last_insert_rowid(raw);
}

std::optional<User> AuthService::login(const std::string &username,
                                       const std::string &password)
{
  const char *sql =
      "SELECT id, salt, pw_hash FROM users WHERE username = ?";
  sqlite3_stmt *stmt = nullptr;
  sqlite3 *raw = db_.rawHandle();
  check(sqlite3_prepare_v2(raw, sql, -1, &stmt, nullptr), "prepare login");
  sqlite3_bind_text(stmt, 1, username.c_str(), -1, SQLITE_TRANSIENT);

  std::optional<User> result;
  if (sqlite3_step(stmt) == SQLITE_ROW)
  {
    int64_t id = sqlite3_column_int64(stmt, 0);
    auto col = [&](int c) -> std::string
    {
      const unsigned char *p = sqlite3_column_text(stmt, c);
      return p ? reinterpret_cast<const char *>(p) : "";
    };
    std::string salt    = col(1);
    std::string pw_hash = col(2);
    if (hashPassword(salt, password) == pw_hash)
    {
      User u;
      u.id       = id;
      u.username = username;
      result     = u;
    }
  }
  sqlite3_finalize(stmt);
  return result;
}

// ── private helpers ───────────────────────────────────────────────────────────

std::string AuthService::randomSalt()
{
  unsigned char buf[16];
  if (RAND_bytes(buf, sizeof(buf)) != 1)
    throw std::runtime_error("RAND_bytes failed");
  std::ostringstream oss;
  for (unsigned char b : buf)
    oss << std::hex << std::setw(2) << std::setfill('0') << (int)b;
  return oss.str();
}

std::string AuthService::hashPassword(const std::string &salt,
                                      const std::string &password)
{
  std::string input = salt + password;
  unsigned char digest[EVP_MAX_MD_SIZE];
  unsigned int digest_len = 0;

  EVP_MD_CTX *ctx = EVP_MD_CTX_new();
  EVP_DigestInit_ex(ctx, EVP_sha256(), nullptr);
  EVP_DigestUpdate(ctx, input.data(), input.size());
  EVP_DigestFinal_ex(ctx, digest, &digest_len);
  EVP_MD_CTX_free(ctx);

  std::ostringstream oss;
  for (unsigned int i = 0; i < digest_len; ++i)
    oss << std::hex << std::setw(2) << std::setfill('0') << (int)digest[i];
  return oss.str();
}
