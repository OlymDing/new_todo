#include "todo_service.h"

#include <algorithm>
#include <functional>
#include <stdexcept>

TodoService::TodoService(Database &db, const AppConfig &config, int64_t user_id)
    : db_(db), config_(config), user_id_(user_id)
{
}

std::string TodoService::resolveStatus(const std::string &s) const
{
  if (s.empty())
    return config_.default_status;
  if (!config_.isValidStatus(s))
  {
    throw std::invalid_argument("Invalid status: '" + s + "'");
  }
  return s;
}

int64_t TodoService::addTodo(
    const std::string &title, const std::string &status,
    const std::string &ext_info
)
{
  if (title.empty())
    throw std::invalid_argument("Title must not be empty");
  Todo t;
  t.title       = title;
  t.status      = resolveStatus(status);
  t.ext_info    = ext_info;
  t.parent_id   = 0;
  t.user_id     = user_id_;
  t.create_time = now_timestamp();
  t.update_time = t.create_time;
  return db_.insertTodo(t);
}

int64_t TodoService::addChild(
    int64_t parent_id, const std::string &title, const std::string &status,
    const std::string &ext_info
)
{
  if (title.empty())
    throw std::invalid_argument("Title must not be empty");
  if (!db_.getTodo(parent_id).has_value())
  {
    throw std::invalid_argument(
        "Parent todo not found: " + std::to_string(parent_id)
    );
  }
  Todo t;
  t.title       = title;
  t.status      = resolveStatus(status);
  t.ext_info    = ext_info;
  t.parent_id   = parent_id;
  t.user_id     = user_id_;
  t.create_time = now_timestamp();
  t.update_time = t.create_time;
  return db_.insertTodo(t);
}

void TodoService::updateTodo(
    int64_t id, std::optional<std::string> title,
    std::optional<std::string> status, std::optional<std::string> ext_info,
    std::optional<Timestamp> due_time
)
{
  auto existing = db_.getTodo(id);
  if (!existing.has_value())
  {
    throw std::invalid_argument("Todo not found: " + std::to_string(id));
  }
  Todo t = *existing;
  if (title.has_value())
  {
    if (title->empty())
      throw std::invalid_argument("Title must not be empty");
    t.title = *title;
  }
  if (status.has_value())
  {
    if (!config_.isValidStatus(*status))
    {
      throw std::invalid_argument("Invalid status: '" + *status + "'");
    }
    t.status = *status;
  }
  if (ext_info.has_value())
  {
    t.ext_info = *ext_info;
  }
  if (due_time.has_value())
  {
    t.due_time = *due_time;
  }
  t.update_time = now_timestamp();
  db_.updateTodo(t);
}

void TodoService::deleteTodo(int64_t id)
{
  if (!db_.getTodo(id).has_value())
  {
    throw std::invalid_argument("Todo not found: " + std::to_string(id));
  }
  db_.deleteTodo(id);
}

void TodoService::changeParent(int64_t id, int64_t new_parent_id)
{
  if (!db_.getTodo(id).has_value())
    throw std::invalid_argument("Todo not found: " + std::to_string(id));
  if (id == new_parent_id)
    throw std::invalid_argument("A todo cannot be its own parent");
  if (new_parent_id != 0)
  {
    if (!db_.getTodo(new_parent_id).has_value())
      throw std::invalid_argument(
          "New parent not found: " + std::to_string(new_parent_id)
      );
    // Cycle check: new_parent_id must not be a descendant of id
    auto subtree = db_.buildTree(id);
    std::function<bool(const std::vector<TodoNode> &)> contains =
        [&](const std::vector<TodoNode> &nodes) -> bool
    {
      for (const auto &n : nodes)
      {
        if (n.todo.id == new_parent_id)
          return true;
        if (contains(n.children))
          return true;
      }
      return false;
    };
    if (contains(subtree))
      throw std::invalid_argument("Cannot move todo under its own descendant");
  }
  auto t = db_.getTodo(id);
  t->parent_id = new_parent_id;
  t->update_time = now_timestamp();
  db_.updateTodo(*t);
}

std::vector<Todo> TodoService::listAll() const { return db_.getAllTodos(user_id_); }

std::vector<Todo> TodoService::listByStatus(const std::string &s) const
{
  if (!config_.isValidStatus(s))
  {
    throw std::invalid_argument("Invalid status: '" + s + "'");
  }
  auto all = db_.getAllTodos(user_id_);
  std::vector<Todo> result;
  for (auto &t : all)
  {
    if (t.status == s)
      result.push_back(t);
  }
  return result;
}

std::vector<TodoNode> TodoService::getTree() const
{
  return db_.buildTree(0, user_id_);
}

std::optional<Todo> TodoService::findById(int64_t id) const
{
  return db_.getTodo(id);
}

std::vector<Todo> TodoService::search(const std::string &query) const
{
  if (query.empty())
    return {};
  return db_.searchTodos(query, user_id_);
}
