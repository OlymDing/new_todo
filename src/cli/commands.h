#pragma once

#include "todo_service.h"

#include <cstdint>
#include <optional>
#include <string>

namespace cli {

int cmd_add(TodoService& svc, const std::string& title,
            const std::optional<std::string>& status,
            const std::optional<std::string>& note);

int cmd_add_child(TodoService& svc, int64_t parent_id,
                  const std::string& title,
                  const std::optional<std::string>& status,
                  const std::optional<std::string>& note);

int cmd_list(TodoService& svc, const std::optional<std::string>& status);

int cmd_show_tree(TodoService& svc);

int cmd_update(TodoService& svc, int64_t id,
               const std::optional<std::string>& title,
               const std::optional<std::string>& status,
               const std::optional<std::string>& note);

int cmd_delete(TodoService& svc, int64_t id);

int cmd_show(TodoService& svc, int64_t id);

} // namespace cli
