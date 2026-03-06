#pragma once
#include "config.h"
#include "db.h"
#include "todo_service.h"
#include "todo.h"
#include <string>
#include <vector>

namespace tui {

enum class Modal { None, AddTodo, ConfirmDelete };

struct FlatItem {
    Todo  todo;
    int   depth      = 0;
    bool  last_child = false;
};

class TuiApp {
public:
    TuiApp(Database& db, const AppConfig& cfg);
    int run();

private:
    Database&        db_;
    const AppConfig& cfg_;
    TodoService      svc_;

    std::vector<FlatItem> items_;
    int                   selected_       = 0;
    Modal                 modal_          = Modal::None;
    std::string           add_input_;
    int64_t               delete_id_      = 0;
    int64_t               add_parent_id_  = 0;

    std::string next_status(const std::string& current) const;
    void        refresh_todos();
};

} // namespace tui
