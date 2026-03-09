#pragma once

#include "todo.h"

#include <optional>
#include <string>
#include <vector>

// Forward-declare SQLite types to avoid including sqlite3.h in the header
struct sqlite3;
struct sqlite3_stmt;

class Database
{
public:
  explicit Database(const std::string &path);
  ~Database();

  // Disable copy; allow move
  Database(const Database &) = delete;
  Database &operator=(const Database &) = delete;

  int64_t insertTodo(const Todo &todo);
  // id lookup ignores user_id (needed for parent validation across users)
  std::optional<Todo> getTodo(int64_t id) const;
  std::vector<Todo> getChildren(int64_t parent_id, int64_t user_id = 0) const;
  std::vector<Todo> getAllTodos(int64_t user_id = 0) const;
  bool updateTodo(const Todo &todo);
  int deleteTodo(int64_t id); // cascades to descendants

  // Build full tree rooted at parent_id (0 = virtual root)
  std::vector<TodoNode> buildTree(int64_t root_parent_id = 0,
                                  int64_t user_id = 0) const;

  // Return ancestors from root down to (but not including) id
  std::vector<Todo> getAncestors(int64_t id) const;

  // Full-text search across title and ext_info, scoped to user_id
  std::vector<Todo> searchTodos(const std::string &query,
                                int64_t user_id = 0) const;

  // Expose raw handle for use by AuthService (schema extensions).
  sqlite3 *rawHandle() const { return db_; }

private:
  sqlite3 *db_ = nullptr;

  void initSchema();
  Todo rowToTodo(sqlite3_stmt *stmt) const;
};
