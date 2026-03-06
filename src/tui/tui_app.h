#pragma once
#include "config.h"
#include "db.h"
#include "todo_service.h"
#include "todo.h"
#include <string>
#include <vector>
#include <ctime>

namespace tui {

enum class Modal { None, AddTodo, ConfirmDelete, EditDetail };

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
    std::string           add_note_;
    std::string           add_due_;
    int64_t               delete_id_      = 0;
    int64_t               add_parent_id_  = 0;

    // edit buffers
    std::string  edit_title_;
    std::string  edit_ext_info_;
    std::string  edit_due_;          // "YYYY-MM-DD" or "" for no due date
    int          edit_status_idx_ = 0;

    // panel/tab focus
    int          focus_panel_ = 0;
    int          tab_focus_   = 0;

    std::string next_status(const std::string& current) const;
    void        refresh_todos();

    void        begin_edit();
    void        commit_edit();
    void        cancel_edit();
    std::string format_timestamp(int64_t ts) const;
    std::string format_date(int64_t ts) const;      // "YYYY-MM-DD" or ""
    int64_t     parse_due_date(const std::string& s) const; // 0 on empty/invalid
};

} // namespace tui
