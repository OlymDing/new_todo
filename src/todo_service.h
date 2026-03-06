#pragma once

#include "config.h"
#include "db.h"
#include "todo.h"

#include <optional>
#include <string>
#include <vector>

class TodoService {
public:
    TodoService(Database& db, const AppConfig& config);

    int64_t addTodo(const std::string& title,
                    const std::string& status   = "",
                    const std::string& ext_info = "");

    int64_t addChild(int64_t            parent_id,
                     const std::string& title,
                     const std::string& status   = "",
                     const std::string& ext_info = "");

    void updateTodo(int64_t                       id,
                    std::optional<std::string>    title,
                    std::optional<std::string>    status,
                    std::optional<std::string>    ext_info);

    void deleteTodo(int64_t id);

    std::vector<Todo>     listAll()                             const;
    std::vector<Todo>     listByStatus(const std::string& s)   const;
    std::vector<TodoNode> getTree()                            const;
    std::optional<Todo>   findById(int64_t id)                 const;

private:
    Database&        db_;
    const AppConfig& config_;

    std::string resolveStatus(const std::string& s) const;
};
