#pragma once

#include "todo.h"

#include <string>
#include <vector>

namespace cli {

std::string format_todo(const Todo& t, const std::string& indent = "");
std::string format_tree(const std::vector<TodoNode>& nodes, int depth = 0);
std::string format_empty();
std::string format_created(int64_t id);
std::string format_created_child(int64_t id);
std::string format_updated(int64_t id);
std::string format_deleted(int64_t id);
std::string format_usage(const std::string& prog);

} // namespace cli
