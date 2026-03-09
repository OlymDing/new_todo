#pragma once

#include "config.h"
#include "db.h"
#include "todo.h"

#include <optional>
#include <string>
#include <vector>

class TodoService
{
public:
  // user_id = 0 → legacy/anonymous mode (no per-user filtering)
  TodoService(Database &db, const AppConfig &config, int64_t user_id = 0);

  int64_t addTodo(
      const std::string &title, const std::string &status = "",
      const std::string &ext_info = ""
  );

  int64_t addChild(
      int64_t parent_id, const std::string &title,
      const std::string &status = "", const std::string &ext_info = ""
  );

  void updateTodo(
      int64_t id, std::optional<std::string> title,
      std::optional<std::string> status, std::optional<std::string> ext_info,
      std::optional<Timestamp> due_time = std::nullopt
  );

  void deleteTodo(int64_t id);

  // Move todo to a new parent (0 = make root). Detects cycles.
  void changeParent(int64_t id, int64_t new_parent_id);

  std::vector<Todo> listAll() const;
  std::vector<Todo> listByStatus(const std::string &s) const;
  std::vector<TodoNode> getTree() const;
  std::optional<Todo> findById(int64_t id) const;
  std::vector<Todo> search(const std::string &query) const;

private:
  Database &db_;
  const AppConfig &config_;
  int64_t user_id_; // 0 = anonymous / no filtering

  std::string resolveStatus(const std::string &s) const;
};
