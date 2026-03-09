#include "cli/commands.h"
#include "cli/formatter.h"

#include <iostream>
#include <stdexcept>

namespace cli
{

int cmd_add(
    TodoService &svc, const std::string &title,
    const std::optional<std::string> &status,
    const std::optional<std::string> &note
)
{
  int64_t id = svc.addTodo(title, status ? *status : "", note ? *note : "");
  std::cout << format_created(id);
  return 0;
}

int cmd_add_child(
    TodoService &svc, int64_t parent_id, const std::string &title,
    const std::optional<std::string> &status,
    const std::optional<std::string> &note
)
{
  int64_t id =
      svc.addChild(parent_id, title, status ? *status : "", note ? *note : "");
  std::cout << format_created_child(id);
  return 0;
}

int cmd_list(TodoService &svc, const std::optional<std::string> &status)
{
  std::vector<Todo> todos;
  if (status)
  {
    todos = svc.listByStatus(*status);
  }
  else
  {
    todos = svc.listAll();
  }
  if (todos.empty())
  {
    std::cout << format_empty();
  }
  else
  {
    for (const auto &t : todos)
    {
      std::cout << format_todo(t);
    }
  }
  return 0;
}

int cmd_show_tree(TodoService &svc)
{
  auto tree = svc.getTree();
  if (tree.empty())
  {
    std::cout << format_empty();
  }
  else
  {
    std::cout << format_tree(tree);
  }
  return 0;
}

int cmd_update(
    TodoService &svc, int64_t id, const std::optional<std::string> &title,
    const std::optional<std::string> &status,
    const std::optional<std::string> &note
)
{
  svc.updateTodo(id, title, status, note);
  std::cout << format_updated(id);
  return 0;
}

int cmd_delete(TodoService &svc, int64_t id)
{
  svc.deleteTodo(id);
  std::cout << format_deleted(id);
  return 0;
}

int cmd_change_parent(TodoService &svc, int64_t id, int64_t new_parent_id)
{
  svc.changeParent(id, new_parent_id);
  std::cout << "Moved todo #" << id << " under "
            << (new_parent_id == 0 ? "root"
                                   : "todo #" + std::to_string(new_parent_id))
            << "\n";
  return 0;
}

int cmd_search(TodoService &svc, const std::string &query)
{
  auto results = svc.search(query);
  if (results.empty())
  {
    std::cout << "(no results)\n";
  }
  else
  {
    for (const auto &t : results)
      std::cout << format_todo(t);
  }
  return 0;
}

int cmd_show(TodoService &svc, int64_t id)
{
  auto t = svc.findById(id);
  if (!t)
  {
    std::cerr << "Todo #" << id << " not found\n";
    return 1;
  }
  std::cout << format_todo(*t);
  return 0;
}

} // namespace cli
