#include "tui/tui_app.h"
#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>
#include <algorithm>
#include <functional>
using namespace ftxui;

namespace tui
{

TuiApp::TuiApp(Database &db, const AppConfig &cfg, int64_t user_id)
    : db_(db), cfg_(cfg), svc_(db_, cfg_, user_id)
{
}

void TuiApp::refresh_todos()
{
  items_.clear();
  std::function<void(const std::vector<TodoNode> &, int)> dfs =
      [&](const std::vector<TodoNode> &nodes, int depth)
  {
    for (size_t i = 0; i < nodes.size(); ++i)
    {
      bool last = (i == nodes.size() - 1);
      items_.push_back({nodes[i].todo, depth, last});
      dfs(nodes[i].children, depth + 1);
    }
  };
  dfs(svc_.getTree(), 0);

  if (items_.empty())
    selected_ = 0;
  else if (selected_ >= (int)items_.size())
    selected_ = (int)items_.size() - 1;
}

std::string TuiApp::next_status(const std::string &current) const
{
  const auto &sv = cfg_.statuses;
  if (sv.empty())
    return current;
  auto it = std::find(sv.begin(), sv.end(), current);
  if (it == sv.end() || std::next(it) == sv.end())
    return sv.front();
  return *std::next(it);
}

void TuiApp::begin_edit()
{
  if (items_.empty())
    return;
  const auto &t = items_[selected_].todo;
  edit_title_ = t.title;
  edit_ext_info_ = t.ext_info;
  edit_due_ = format_date(t.due_time);
  const auto &sv = cfg_.statuses;
  auto it = std::find(sv.begin(), sv.end(), t.status);
  edit_status_idx_ = (it != sv.end()) ? (int)(it - sv.begin()) : 0;
  modal_ = Modal::EditDetail;
  tab_focus_ = 3;
  focus_panel_ = 1;
}

void TuiApp::commit_edit()
{
  if (!items_.empty())
  {
    const auto &t = items_[selected_].todo;
    std::string new_status =
        cfg_.statuses.empty() ? t.status : cfg_.statuses[edit_status_idx_];
    int64_t new_due = parse_due_date(edit_due_);
    svc_.updateTodo(
        t.id,
        edit_title_.empty() ? std::nullopt
                            : std::optional<std::string>(edit_title_),
        new_status, edit_ext_info_, std::optional<Timestamp>(new_due)
    );
    refresh_todos();
  }
  cancel_edit();
}

void TuiApp::cancel_edit()
{
  edit_title_.clear();
  edit_ext_info_.clear();
  edit_due_.clear();
  edit_status_idx_ = 0;
  modal_ = Modal::None;
  tab_focus_ = 0;
  focus_panel_ = 0;
}

std::string TuiApp::format_timestamp(int64_t ts) const
{
  if (ts == 0)
    return "(none)";
  std::time_t t = (std::time_t)ts;
  char buf[32];
  std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M", std::localtime(&t));
  return buf;
}

std::string TuiApp::format_date(int64_t ts) const
{
  if (ts == 0)
    return "";
  std::time_t t = (std::time_t)ts;
  char buf[16];
  std::strftime(buf, sizeof(buf), "%Y-%m-%d", std::localtime(&t));
  return buf;
}

int64_t TuiApp::parse_due_date(const std::string &s) const
{
  if (s.empty())
    return 0;
  std::tm tm = {};
  // Accept "YYYY-MM-DD"
  if (std::sscanf(
          s.c_str(), "%d-%d-%d", &tm.tm_year, &tm.tm_mon, &tm.tm_mday
      ) != 3)
    return 0;
  tm.tm_year -= 1900;
  tm.tm_mon -= 1;
  tm.tm_hour = 0;
  tm.tm_min = 0;
  tm.tm_sec = 0;
  tm.tm_isdst = -1;
  std::time_t ts = std::mktime(&tm);
  return (ts == (std::time_t)-1) ? 0 : (int64_t)ts;
}

int TuiApp::run()
{
  refresh_todos();

  auto screen = ScreenInteractive::TerminalOutput();

  // ---- Add modal ----
  auto add_input = Input(&add_input_, "Title...");
  InputOption add_note_opt;
  add_note_opt.multiline = true;
  auto add_note_input = Input(&add_note_, add_note_opt);
  InputOption add_due_opt;
  add_due_opt.multiline = false;
  auto add_due_input = Input(&add_due_, add_due_opt);
  int add_focus = 0; // unused, kept for reference
  auto add_ok = Button(
      "  OK  ",
      [&]
      {
        if (!add_input_.empty())
        {
          int64_t due = parse_due_date(add_due_);
          if (add_parent_id_ == 0)
          {
            int64_t id = svc_.addTodo(add_input_, "", add_note_);
            if (due != 0)
              svc_.updateTodo(
                  id, std::nullopt, std::nullopt, std::nullopt,
                  std::optional<Timestamp>(due)
              );
          }
          else
          {
            int64_t id =
                svc_.addChild(add_parent_id_, add_input_, "", add_note_);
            if (due != 0)
              svc_.updateTodo(
                  id, std::nullopt, std::nullopt, std::nullopt,
                  std::optional<Timestamp>(due)
              );
          }
          add_input_.clear();
          add_note_.clear();
          add_due_.clear();
          refresh_todos();
        }
        modal_ = Modal::None;
        tab_focus_ = 0;
      }
  );
  auto add_cancel = Button(
      "Cancel",
      [&]
      {
        add_input_.clear();
        add_note_.clear();
        add_due_.clear();
        modal_ = Modal::None;
        tab_focus_ = 0;
      }
  );
  auto add_buttons = Container::Horizontal({add_ok, add_cancel});
  auto add_comp = Container::Vertical(
      {add_input, add_due_input, add_note_input, add_buttons}
  );

  // ---- Delete modal ----
  auto del_yes = Button(
      "  Yes  ",
      [&]
      {
        if (delete_id_ != 0)
        {
          svc_.deleteTodo(delete_id_);
          delete_id_ = 0;
          refresh_todos();
        }
        modal_ = Modal::None;
        tab_focus_ = 0;
      }
  );
  auto del_no = Button(
      "  No   ",
      [&]
      {
        delete_id_ = 0;
        modal_ = Modal::None;
        tab_focus_ = 0;
      }
  );
  auto del_comp = Container::Horizontal({del_yes, del_no});

  // ---- Change-parent modal ----
  auto cp_input_comp = Input(&cp_input_, "New parent ID (0 = root)");
  auto cp_ok = Button(
      "  OK  ",
      [&]
      {
        if (!items_.empty() && !cp_input_.empty())
        {
          try
          {
            int64_t new_pid = std::stoll(cp_input_);
            svc_.changeParent(items_[selected_].todo.id, new_pid);
            refresh_todos();
          }
          catch (...)
          {
          }
        }
        cp_input_.clear();
        modal_ = Modal::None;
        tab_focus_ = 0;
      }
  );
  auto cp_cancel = Button(
      "Cancel",
      [&]
      {
        cp_input_.clear();
        modal_ = Modal::None;
        tab_focus_ = 0;
      }
  );
  auto cp_buttons = Container::Horizontal({cp_ok, cp_cancel});
  auto cp_comp = Container::Vertical({cp_input_comp, cp_buttons});

  // ---- Edit detail components ----
  InputOption title_opt;
  title_opt.multiline = false;
  title_opt.on_enter = [&] { commit_edit(); };
  auto edit_title_input = Input(&edit_title_, title_opt);

  InputOption note_opt;
  note_opt.multiline = true;
  auto edit_note_input = Input(&edit_ext_info_, note_opt);

  InputOption due_opt;
  due_opt.multiline = false;
  auto edit_due_input = Input(&edit_due_, due_opt);

  auto edit_save_btn = Button("  Save  ", [&] { commit_edit(); });
  auto edit_cancel_btn = Button(" Cancel ", [&] { cancel_edit(); });
  auto edit_btns = Container::Horizontal({edit_save_btn, edit_cancel_btn});
  auto edit_inputs_comp = Container::Vertical(
      {edit_title_input, edit_due_input, edit_note_input, edit_btns}
  );

  // ---- Logout confirmation modal ----
  auto logout_yes = Button(
      "  Yes  ",
      [&]
      {
        session_.clear();
        screen.ExitLoopClosure()();
      }
  );
  auto logout_no = Button(
      "  No   ",
      [&]
      {
        modal_ = Modal::None;
        tab_focus_ = 0;
      }
  );
  auto logout_comp = Container::Horizontal({logout_yes, logout_no});

  // ---- Tab container (0=main, 1=add, 2=delete, 3=edit, 4=change-parent, 5=search, 6=logout) ----
  // Search input component — onChange triggers a live FTS query.
  InputOption search_opt;
  search_opt.multiline = false;
  search_opt.on_change = [&]
  {
    search_selected_ = 0;
    search_results_ = search_query_.empty()
                          ? std::vector<Todo>{}
                          : svc_.search(search_query_);
  };
  auto search_input_comp = Input(&search_query_, "Search...", search_opt);
  auto search_comp       = Container::Vertical({search_input_comp});

  auto dummy = Renderer([] { return text(""); });
  auto tab_container = Container::Tab(
      {dummy, add_comp, del_comp, edit_inputs_comp, cp_comp, search_comp,
       logout_comp},
      &tab_focus_
  );

  // ---- Left panel component ----
  auto left_comp = Renderer(tab_container, [&]
  {
    Elements rows;
    rows.push_back(hbox({
        text("  ID") | bold,
        text("  "),
        text("Title                    ") | bold,
        text("  "),
        text("Status") | bold,
    }));
    rows.push_back(separator());

    if (items_.empty())
    {
      rows.push_back(text("  (no todos)") | dim);
    }
    else
    {
      for (int i = 0; i < (int)items_.size(); ++i)
      {
        const auto &item = items_[i];
        const auto &t    = item.todo;

        std::string prefix;
        if (item.depth == 0)
          prefix = "  ";
        else
          prefix = std::string((item.depth - 1) * 2, ' ') +
                   (item.last_child ? "\u2514\u2500 " : "\u251c\u2500 ");

        std::string id_str     = std::to_string(t.id);
        int         title_budget = 22 - (int)prefix.size();
        if (title_budget < 5) title_budget = 5;
        std::string title =
            t.title.size() > (size_t)title_budget
                ? t.title.substr(0, title_budget - 1) + "\u2026"
                : t.title;

        Element status_el = text(t.status);
        if (t.status == "todo")
          status_el = status_el | color(Color::Blue);
        else if (t.status == "in_progress")
          status_el = status_el | color(Color::Yellow);
        else if (t.status == "done")
          status_el = status_el | color(Color::Green);

        auto row = hbox({
            text(std::string(4 - id_str.size(), ' ') + id_str + " "),
            text(prefix) | (item.depth > 0 ? dim : nothing),
            text(title + std::string(title_budget - (int)title.size() + 1, ' ')),
            status_el,
        });

        if (i == selected_)
          row = (focus_panel_ == 0) ? row | inverted : row | inverted | dim;
        rows.push_back(row);
      }
    }

    return vbox({
               text(" List") | bold,
               separator(),
               vbox(rows) | yframe,
           }) |
           border;
  });

  // ---- Right panel component ----
  auto right_comp = Renderer(tab_container, [&]
  {
    if (modal_ == Modal::EditDetail && !items_.empty())
    {
      const auto &t       = items_[selected_].todo;
      std::string cur_status = cfg_.statuses.empty()
                                   ? t.status
                                   : cfg_.statuses[edit_status_idx_];
      Element status_el = text(cur_status);
      if (cur_status == "todo")
        status_el = status_el | color(Color::Blue);
      else if (cur_status == "in_progress")
        status_el = status_el | color(Color::Yellow);
      else if (cur_status == "done")
        status_el = status_el | color(Color::Green);

      return vbox({
                 text(" Edit") | bold | color(Color::Yellow),
                 separator(),
                 hbox({text(" Title:   ") | bold, edit_title_input->Render()}),
                 hbox({text(" Status:  ") | bold, status_el}),
                 hbox({text(" Due:     ") | bold, edit_due_input->Render(),
                       text("  YYYY-MM-DD") | dim}),
                 separator(),
                 text(" Notes:") | bold,
                 edit_note_input->Render() | size(HEIGHT, GREATER_THAN, 4),
                 filler(),
                 separator(),
                 hbox({edit_save_btn->Render(), text("  "),
                       edit_cancel_btn->Render()}),
                 separator(),
                 text("  Tab:next  Enter:save  Esc:cancel") | dim,
             }) |
             border | flex | color(Color::Yellow);
    }
    if (!items_.empty())
    {
      const auto &t = items_[selected_].todo;
      Element status_el = text(t.status);
      if (t.status == "todo")
        status_el = status_el | color(Color::Blue);
      else if (t.status == "in_progress")
        status_el = status_el | color(Color::Yellow);
      else if (t.status == "done")
        status_el = status_el | color(Color::Green);

      return vbox({
                 text(" Detail") | bold,
                 separator(),
                 hbox({text(" Title:   ") | bold, text(t.title)}),
                 hbox({text(" Status:  ") | bold, status_el}),
                 hbox({text(" Due:     ") | bold,
                       text(t.due_time == 0 ? "(none)" : format_date(t.due_time))}),
                 separator(),
                 text(" Notes:") | bold,
                 paragraph(t.ext_info.empty() ? "(none)" : t.ext_info) | dim,
                 filler(),
                 separator(),
                 hbox({text(" Created: ") | dim,
                       text(format_timestamp(t.create_time)) | dim}),
                 hbox({text(" Updated: ") | dim,
                       text(format_timestamp(t.update_time)) | dim}),
             }) |
             border | flex;
    }
    return vbox({
               text(" Detail") | bold,
               separator(),
               text("  (no selection)") | dim,
               filler(),
           }) |
           border | flex;
  });

  // ---- Resizable split ----
  auto split = ResizableSplitLeft(left_comp, right_comp, &left_size_);

  // ---- Global event handler ----
  auto main_comp = CatchEvent(
      split,
      [&](Event ev) -> bool
      {
        // Escape: close all modals
        if (ev == Event::Escape)
        {
          if (modal_ == Modal::AddTodo || modal_ == Modal::ConfirmDelete)
          {
            add_input_.clear();
            delete_id_ = 0;
            modal_ = Modal::None;
            tab_focus_ = 0;
            return true;
          }
          if (modal_ == Modal::EditDetail)
          {
            cancel_edit();
            return true;
          }
          if (modal_ == Modal::ChangeParent)
          {
            cp_input_.clear();
            modal_ = Modal::None;
            tab_focus_ = 0;
            return true;
          }
          if (modal_ == Modal::Search)
          {
            search_query_.clear();
            search_results_.clear();
            search_selected_ = 0;
            modal_ = Modal::None;
            tab_focus_ = 0;
            return true;
          }
          if (modal_ == Modal::ConfirmLogout)
          {
            modal_ = Modal::None;
            tab_focus_ = 0;
            return true;
          }
          // No modal open: Esc shows logout confirmation.
          modal_ = Modal::ConfirmLogout;
          tab_focus_ = 6;
          return true;
        }

        // Search modal: handle navigation and confirm inside the modal.
        if (modal_ == Modal::Search)
        {
          if (ev == Event::ArrowUp)
          {
            if (search_selected_ > 0)
              --search_selected_;
            return true;
          }
          if (ev == Event::ArrowDown)
          {
            if (search_selected_ < (int)search_results_.size() - 1)
              ++search_selected_;
            return true;
          }
          if (ev == Event::Return && !search_results_.empty())
          {
            // Jump to the selected result in the main list.
            int64_t target = search_results_[search_selected_].id;
            for (int i = 0; i < (int)items_.size(); ++i)
            {
              if (items_[i].todo.id == target)
              {
                selected_ = i;
                break;
              }
            }
            search_query_.clear();
            search_results_.clear();
            search_selected_ = 0;
            modal_ = Modal::None;
            tab_focus_ = 0;
            return true;
          }
          // All other keys (typing) pass through to search_input_comp.
          return false;
        }

        // AddTodo / ConfirmDelete / ChangeParent: pass through to modal
        // component
        if (modal_ == Modal::AddTodo || modal_ == Modal::ConfirmDelete ||
            modal_ == Modal::ChangeParent)
          return false;

        // EditDetail mode: only Escape is handled above; all other keys pass
        // through to edit_inputs_comp so that characters (including 's') type
        // into the fields.
        if (modal_ == Modal::EditDetail)
          return false;

        // Normal mode (left panel)
        if (ev == Event::ArrowUp || ev == Event::Character('k'))
        {
          if (!items_.empty() && selected_ > 0)
            --selected_;
          return true;
        }
        if (ev == Event::ArrowDown || ev == Event::Character('j'))
        {
          if (!items_.empty() && selected_ < (int)items_.size() - 1)
            ++selected_;
          return true;
        }
        if (ev == Event::Character('a'))
        {
          add_parent_id_ = 0;
          add_input_.clear();
          modal_ = Modal::AddTodo;
          tab_focus_ = 1;
          return true;
        }
        if (ev == Event::Character('c'))
        {
          if (!items_.empty())
          {
            add_parent_id_ = items_[selected_].todo.id;
            add_input_.clear();
            modal_ = Modal::AddTodo;
            tab_focus_ = 1;
          }
          return true;
        }
        if (ev == Event::Character('d'))
        {
          if (!items_.empty())
          {
            delete_id_ = items_[selected_].todo.id;
            modal_ = Modal::ConfirmDelete;
            tab_focus_ = 2;
          }
          return true;
        }
        if (ev == Event::Character('s'))
        {
          if (!items_.empty())
          {
            const auto &t = items_[selected_].todo;
            svc_.updateTodo(
                t.id, std::nullopt, next_status(t.status), std::nullopt
            );
            refresh_todos();
          }
          return true;
        }
        if (ev == Event::Character('u'))
        {
          begin_edit();
          return true;
        }
        if (ev == Event::Character('p'))
        {
          if (!items_.empty())
          {
            cp_input_.clear();
            modal_ = Modal::ChangeParent;
            tab_focus_ = 4;
          }
          return true;
        }
        if (ev == Event::Character('/'))
        {
          search_query_.clear();
          search_results_.clear();
          search_selected_ = 0;
          modal_ = Modal::Search;
          tab_focus_ = 5;
          return true;
        }
        if (ev == Event::Character('q'))
        {
          screen.ExitLoopClosure()();
          return true;
        }
        return false;
      }
  );

  // ---- Renderer ----
  auto final_renderer = Renderer(
      main_comp,
      [&]
      {
        auto title_line = text(" new_todo") | bold;
        auto status_bar = text(
                              " a:add-root  c:add-child  d:del  s:cycle  "
                              "u:edit  p:parent  /:search  j/k:\u2191\u2193  q:quit  Esc:logout"
                          ) |
                          dim;
        auto main_view = vbox({title_line, main_comp->Render() | flex, status_bar});

        // Overlay modals
        if (modal_ == Modal::AddTodo)
        {
          std::string modal_title =
              (add_parent_id_ == 0) ? " Add Root Todo" : " Add Child Todo";
          auto modal_view =
              vbox({
                  text(modal_title) | bold,
                  separator(),
                  hbox({text(" Title: ") | bold, add_input->Render()}),
                  hbox(
                      {text(" Due:   ") | bold, add_due_input->Render(),
                       text("  YYYY-MM-DD") | dim}
                  ),
                  separator(),
                  text(" Notes:") | bold,
                  add_note_input->Render() | size(HEIGHT, GREATER_THAN, 3),
                  separator(),
                  hbox({add_ok->Render(), text("  "), add_cancel->Render()}),
              }) |
              border | size(WIDTH, EQUAL, 50) | center;
          return dbox({main_view, clear_under(modal_view | center)});
        }

        if (modal_ == Modal::ConfirmDelete)
        {
          std::string del_title = (delete_id_ > 0 && !items_.empty())
                                      ? items_[selected_].todo.title
                                      : "";
          auto modal_view =
              vbox({
                  text(" Delete Todo") | bold,
                  separator(),
                  text(" Delete: \"" + del_title + "\"?"),
                  separator(),
                  hbox({del_yes->Render(), text("  "), del_no->Render()}),
              }) |
              border | size(WIDTH, EQUAL, 40) | center;
          return dbox({main_view, clear_under(modal_view | center)});
        }

        if (modal_ == Modal::ChangeParent && !items_.empty())
        {
          const auto &t = items_[selected_].todo;
          std::string cur_parent =
              t.parent_id == 0 ? "root" : "#" + std::to_string(t.parent_id);
          auto modal_view =
              vbox({
                  text(" Change Parent") | bold,
                  separator(),
                  text(" Todo: \"" + t.title + "\""),
                  text(" Current parent: " + cur_parent) | dim,
                  separator(),
                  hbox(
                      {text(" New parent ID: ") | bold, cp_input_comp->Render()}
                  ),
                  text(" (enter 0 to make root)") | dim,
                  separator(),
                  hbox({cp_ok->Render(), text("  "), cp_cancel->Render()}),
              }) |
              border | size(WIDTH, EQUAL, 50) | center;
          return dbox({main_view, clear_under(modal_view | center)});
        }

        if (modal_ == Modal::Search)
        {
          Elements result_rows;
          if (search_results_.empty())
          {
            result_rows.push_back(
                text(search_query_.empty() ? "  Type to search…"
                                           : "  (no results)") |
                dim
            );
          }
          else
          {
            for (int i = 0; i < (int)search_results_.size(); ++i)
            {
              const auto &t = search_results_[i];
              Element status_el = text(t.status);
              if (t.status == "todo")
                status_el = status_el | color(Color::Blue);
              else if (t.status == "in_progress")
                status_el = status_el | color(Color::Yellow);
              else if (t.status == "done")
                status_el = status_el | color(Color::Green);

              auto row = hbox({
                  text(" #" + std::to_string(t.id) + " ") | dim,
                  text(t.title) | flex,
                  text("  "),
                  status_el,
                  text(" "),
              });
              if (i == search_selected_)
                row = row | inverted;
              result_rows.push_back(row);
            }
          }

          auto modal_view =
              vbox({
                  text(" Search") | bold,
                  separator(),
                  hbox({text(" / ") | bold, search_input_comp->Render()}),
                  separator(),
                  vbox(result_rows) | size(HEIGHT, LESS_THAN, 12),
                  separator(),
                  text(
                      "  \u2191\u2193 navigate   Enter:jump   Esc:close"
                  ) | dim,
              }) |
              border | size(WIDTH, EQUAL, 55) | center;
          return dbox({main_view, clear_under(modal_view | center)});
        }

        if (modal_ == Modal::ConfirmLogout)
        {
          auto modal_view =
              vbox({
                  text(" Logout") | bold,
                  separator(),
                  text(" Are you sure you want to log out?"),
                  separator(),
                  hbox({logout_yes->Render(), text("  "), logout_no->Render()}),
              }) |
              border | size(WIDTH, EQUAL, 42) | center;
          return dbox({main_view, clear_under(modal_view | center)});
        }

        return main_view;
      }
  );

  screen.Loop(final_renderer);
  return 0;
}

} // namespace tui
