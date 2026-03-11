#pragma once

#include "auth.h"
#include "config.h"
#include "db.h"
#include "session.h"
#include "todo_service.h"
#include "todo.h"

#include <string>
#include <vector>
#include <ctime>

namespace tui
{

enum class Modal
{
  None,
  AddTodo,
  ConfirmDelete,
  EditDetail,
  ChangeParent,
  Search,
  ConfirmLogout
};

struct FlatItem
{
  Todo todo;
  int depth = 0;
  bool last_child = false;
};

class TuiApp
{
public:
  TuiApp(Database &db, const AppConfig &cfg, const std::string &session_path);
  int run();

private:
  Database &db_;
  const AppConfig &cfg_;
  AuthService auth_;
  SessionManager session_;
  // svc_ is constructed after login.
  std::optional<TodoService> svc_;

  std::vector<FlatItem> items_;
  int selected_ = 0;
  Modal modal_ = Modal::None;
  std::string add_input_;
  std::string add_note_;
  std::string add_due_;
  int64_t delete_id_ = 0;
  int64_t add_parent_id_ = 0;
  std::string cp_input_;

  std::string        search_query_;
  std::vector<Todo>  search_results_;
  int                search_selected_ = 0;

  std::string edit_title_;
  std::string edit_ext_info_;
  std::string edit_due_;
  int edit_status_idx_ = 0;

  int focus_panel_ = 0;
  int tab_focus_ = 0;
  int left_size_ = 48;

  // Returns user_id (0 = cancelled / failed).
  int64_t show_login_screen();

  int run_main();

  int run_main(int64_t user_id);

  void refresh_todos();
  void begin_edit();
  void commit_edit();
  void cancel_edit();
  std::string format_timestamp(int64_t ts) const;
  std::string format_date(int64_t ts) const;
  int64_t parse_due_date(const std::string &s) const;
};

} // namespace tui
