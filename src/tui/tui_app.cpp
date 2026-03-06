#include "tui/tui_app.h"
#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>
#include <algorithm>
#include <functional>
using namespace ftxui;

namespace tui {

TuiApp::TuiApp(Database& db, const AppConfig& cfg)
    : db_(db), cfg_(cfg), svc_(db_, cfg_) {}

void TuiApp::refresh_todos() {
    items_.clear();
    std::function<void(const std::vector<TodoNode>&, int)> dfs =
        [&](const std::vector<TodoNode>& nodes, int depth) {
            for (size_t i = 0; i < nodes.size(); ++i) {
                bool last = (i == nodes.size() - 1);
                items_.push_back({ nodes[i].todo, depth, last });
                dfs(nodes[i].children, depth + 1);
            }
        };
    dfs(svc_.getTree(), 0);

    if (items_.empty()) selected_ = 0;
    else if (selected_ >= (int)items_.size())
        selected_ = (int)items_.size() - 1;
}

std::string TuiApp::next_status(const std::string& current) const {
    const auto& sv = cfg_.statuses;
    if (sv.empty()) return current;
    auto it = std::find(sv.begin(), sv.end(), current);
    if (it == sv.end() || std::next(it) == sv.end()) return sv.front();
    return *std::next(it);
}

int TuiApp::run() {
    refresh_todos();

    auto screen = ScreenInteractive::TerminalOutput();

    // ---- Add modal ----
    auto add_input = Input(&add_input_, "Title...");
    int add_focus = 0;  // 0=input, 1=OK, 2=Cancel
    auto add_ok = Button("  OK  ", [&] {
        if (!add_input_.empty()) {
            if (add_parent_id_ == 0)
                svc_.addTodo(add_input_);
            else
                svc_.addChild(add_parent_id_, add_input_);
            add_input_.clear();
            refresh_todos();
        }
        modal_ = Modal::None;
    });
    auto add_cancel = Button("Cancel", [&] {
        add_input_.clear();
        modal_ = Modal::None;
    });
    auto add_buttons = Container::Horizontal({ add_ok, add_cancel });
    auto add_comp = Container::Vertical({ add_input, add_buttons });

    // ---- Delete modal ----
    auto del_yes = Button("  Yes  ", [&] {
        if (delete_id_ != 0) {
            svc_.deleteTodo(delete_id_);
            delete_id_ = 0;
            refresh_todos();
        }
        modal_ = Modal::None;
    });
    auto del_no = Button("  No   ", [&] {
        delete_id_ = 0;
        modal_ = Modal::None;
    });
    auto del_comp = Container::Horizontal({ del_yes, del_no });

    // ---- Tab container (0=main, 1=add, 2=delete) ----
    int tab_focus = 0;
    auto dummy = Renderer([] { return text(""); });
    auto tab_container = Container::Tab({ dummy, add_comp, del_comp }, &tab_focus);

    // ---- Global event handler ----
    auto main_comp = CatchEvent(tab_container, [&](Event ev) -> bool {
        // Modal-specific events
        if (modal_ != Modal::None) {
            if (ev == Event::Escape) {
                add_input_.clear();
                delete_id_ = 0;
                modal_ = Modal::None;
                tab_focus = 0;
                return true;
            }
            return false;  // let modal component handle it
        }

        // Main list navigation
        if (ev == Event::ArrowUp || ev == Event::Character('k')) {
            if (!items_.empty() && selected_ > 0) --selected_;
            return true;
        }
        if (ev == Event::ArrowDown || ev == Event::Character('j')) {
            if (!items_.empty() && selected_ < (int)items_.size() - 1) ++selected_;
            return true;
        }
        if (ev == Event::Character('a')) {
            add_parent_id_ = 0;
            add_input_.clear();
            modal_ = Modal::AddTodo;
            tab_focus = 1;
            return true;
        }
        if (ev == Event::Character('c')) {
            if (!items_.empty()) {
                add_parent_id_ = items_[selected_].todo.id;
                add_input_.clear();
                modal_ = Modal::AddTodo;
                tab_focus = 1;
            }
            return true;
        }
        if (ev == Event::Character('d')) {
            if (!items_.empty()) {
                delete_id_ = items_[selected_].todo.id;
                modal_ = Modal::ConfirmDelete;
                tab_focus = 2;
            }
            return true;
        }
        if (ev == Event::Character('s')) {
            if (!items_.empty()) {
                const auto& t = items_[selected_].todo;
                svc_.updateTodo(t.id, std::nullopt, next_status(t.status), std::nullopt);
                refresh_todos();
            }
            return true;
        }
        if (ev == Event::Character('q')) {
            screen.ExitLoopClosure()();
            return true;
        }
        return false;
    });

    // ---- Renderer ----
    auto final_renderer = Renderer(main_comp, [&] {
        // Build list rows
        Elements rows;
        // Header
        rows.push_back(
            hbox({
                text("  ID  ") | bold,
                text(" "),
                text("Title                              ") | bold,
                text(" "),
                text("Status      ") | bold,
            })
        );
        rows.push_back(separator());

        if (items_.empty()) {
            rows.push_back(text("  (no todos)") | dim);
        } else {
            for (int i = 0; i < (int)items_.size(); ++i) {
                const auto& item = items_[i];
                const auto& t    = item.todo;

                std::string prefix;
                if (item.depth == 0) {
                    prefix = "  ";
                } else {
                    prefix = std::string((item.depth - 1) * 2, ' ')
                           + (item.last_child ? "\u2514\u2500 " : "\u251c\u2500 ");
                }

                std::string id_str = std::to_string(t.id);
                int title_budget = 33 - (int)prefix.size();
                if (title_budget < 5) title_budget = 5;
                std::string title = t.title.size() > (size_t)title_budget
                                      ? t.title.substr(0, title_budget - 1) + "\u2026"
                                      : t.title;

                Element status_el = text(t.status);
                if      (t.status == "todo")        status_el = status_el | color(Color::Blue);
                else if (t.status == "in_progress") status_el = status_el | color(Color::Yellow);
                else if (t.status == "done")        status_el = status_el | color(Color::Green);

                auto row = hbox({
                    text(std::string(4 - id_str.size(), ' ') + id_str + " "),
                    text(prefix) | (item.depth > 0 ? dim : nothing),
                    text(title + std::string(title_budget - (int)title.size() + 1, ' ')),
                    status_el,
                });

                if (i == selected_) row = row | inverted;
                rows.push_back(row);
            }
        }

        auto list_view = vbox(rows) | border;
        auto title_line = text(" new_todo") | bold;
        auto status_bar = text(" a:add-root  c:add-child  d:delete  s:cycle  j/k:\u2191\u2193  q:quit") | dim;
        auto main_view = vbox({ title_line, list_view, status_bar });

        // Overlay modals
        if (modal_ == Modal::AddTodo) {
            std::string modal_title = (add_parent_id_ == 0)
                ? " Add Root Todo"
                : " Add Child Todo";
            auto modal_view = vbox({
                text(modal_title) | bold,
                separator(),
                hbox({ text(" Title: "), add_input->Render() }),
                separator(),
                hbox({ add_ok->Render(), text("  "), add_cancel->Render() }),
            }) | border | size(WIDTH, EQUAL, 40) | center;
            return dbox({ main_view, modal_view | center });
        }

        if (modal_ == Modal::ConfirmDelete) {
            std::string del_title = (delete_id_ > 0 && !items_.empty())
                ? items_[selected_].todo.title
                : "";
            auto modal_view = vbox({
                text(" Delete Todo") | bold,
                separator(),
                text(" Delete: \"" + del_title + "\"?"),
                separator(),
                hbox({ del_yes->Render(), text("  "), del_no->Render() }),
            }) | border | size(WIDTH, EQUAL, 40) | center;
            return dbox({ main_view, modal_view | center });
        }

        return main_view;
    });

    screen.Loop(final_renderer);
    return 0;
}

} // namespace tui
