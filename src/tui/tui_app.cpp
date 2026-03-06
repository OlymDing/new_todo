#include "tui/tui_app.h"
#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>
#include <algorithm>
using namespace ftxui;

namespace tui {

TuiApp::TuiApp(Database& db, const AppConfig& cfg)
    : db_(db), cfg_(cfg), svc_(db_, cfg_) {}

void TuiApp::refresh_todos() {
    todos_ = svc_.listAll();
    if (todos_.empty()) {
        selected_ = 0;
    } else if (selected_ >= (int)todos_.size()) {
        selected_ = (int)todos_.size() - 1;
    }
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
            svc_.addTodo(add_input_);
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
            if (!todos_.empty() && selected_ > 0) --selected_;
            return true;
        }
        if (ev == Event::ArrowDown || ev == Event::Character('j')) {
            if (!todos_.empty() && selected_ < (int)todos_.size() - 1) ++selected_;
            return true;
        }
        if (ev == Event::Character('a')) {
            add_input_.clear();
            modal_ = Modal::AddTodo;
            tab_focus = 1;
            return true;
        }
        if (ev == Event::Character('d')) {
            if (!todos_.empty()) {
                delete_id_ = todos_[selected_].id;
                modal_ = Modal::ConfirmDelete;
                tab_focus = 2;
            }
            return true;
        }
        if (ev == Event::Character('s')) {
            if (!todos_.empty()) {
                const auto& t = todos_[selected_];
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

        if (todos_.empty()) {
            rows.push_back(text("  (no todos)") | dim);
        } else {
            for (int i = 0; i < (int)todos_.size(); ++i) {
                const auto& t = todos_[i];
                std::string id_str  = std::to_string(t.id);
                // pad / truncate title to 34 chars
                std::string title   = t.title.size() > 34
                                        ? t.title.substr(0, 33) + "\u2026"
                                        : t.title;
                std::string status  = t.status;

                auto row = hbox({
                    text(std::string(4 - id_str.size(), ' ') + id_str + "  "),
                    text(title + std::string(35 - title.size(), ' ')),
                    text(" "),
                    text(status),
                });

                if (i == selected_) {
                    row = row | inverted;
                }
                rows.push_back(row);
            }
        }

        auto list_view = vbox(rows) | border;
        auto title_line = text(" new_todo") | bold;
        auto status_bar = text(" a:add  d:delete  s:cycle-status  j/k:\u2191\u2193  q:quit") | dim;
        auto main_view = vbox({ title_line, list_view, status_bar });

        // Overlay modals
        if (modal_ == Modal::AddTodo) {
            auto modal_view = vbox({
                text(" Add Todo") | bold,
                separator(),
                hbox({ text(" Title: "), add_input->Render() }),
                separator(),
                hbox({ add_ok->Render(), text("  "), add_cancel->Render() }),
            }) | border | size(WIDTH, EQUAL, 40) | center;
            return dbox({ main_view, modal_view | center });
        }

        if (modal_ == Modal::ConfirmDelete) {
            std::string del_title = (delete_id_ > 0 && !todos_.empty())
                ? todos_[selected_].title
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
