#pragma once

#include <cstdint>
#include <string>
#include <vector>

using Timestamp = int64_t;

struct Todo
{
  int64_t id = 0;
  int64_t parent_id = 0; // 0 = child of virtual root
  int64_t user_id = 0;   // 0 = unowned (legacy / anonymous)
  std::string title;
  std::string status;
  std::string ext_info;
  Timestamp create_time = 0;
  Timestamp update_time = 0;
  Timestamp due_time = 0; // 0 = no due date

  bool isRootLevel() const { return parent_id == 0; }
};

struct TodoNode
{
  Todo todo;
  std::vector<TodoNode> children;
};

Timestamp now_timestamp();
