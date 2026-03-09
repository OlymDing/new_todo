#include "cli/cli_app.h"
#include "cli/commands.h"
#include "cli/formatter.h"
#include "tui/tui_app.h"

#include <iostream>
#include <stdexcept>
#include <unistd.h>
#include <readline/readline.h>
#include <readline/history.h>
#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>
using namespace ftxui;

namespace cli {

CliApp::CliApp(Database& db, const AppConfig& cfg)
    : db_(db), cfg_(cfg), svc_(db_, cfg_) {}

int CliApp::run(int argc, char** argv) {
    std::string prog = argv[0];

    if (argc >= 2) {
        std::string cmd  = argv[1];
        std::vector<std::string> args(argv + 2, argv + argc);
        try {
            return dispatch(cmd, prog, args);
        } catch (const std::invalid_argument& e) {
            std::cerr << "Error: " << e.what() << "\n";
            return 1;
        } catch (const std::exception& e) {
            std::cerr << "Fatal: " << e.what() << "\n";
            return 2;
        }
    } else if (isatty(STDIN_FILENO)) {
        int choice = show_selector();
        if (choice == 0) {
            tui::TuiApp tui_app(db_, cfg_);
            return tui_app.run();
        } else {
            return run_loop(prog);
        }
    } else {
        std::cerr << format_usage(prog);
        return 1;
    }
}

std::optional<std::string> CliApp::parse_flag(
        const std::vector<std::string>& args, const std::string& name) {
    for (size_t i = 0; i + 1 < args.size(); ++i) {
        if (args[i] == name) return args[i + 1];
    }
    return std::nullopt;
}

std::vector<std::string> CliApp::tokenize(const std::string& line) {
    std::vector<std::string> tokens;
    std::string current;
    bool in_single = false;
    bool in_double = false;

    for (size_t i = 0; i < line.size(); ++i) {
        char c = line[i];
        if (c == '\'' && !in_double) {
            in_single = !in_single;
        } else if (c == '"' && !in_single) {
            in_double = !in_double;
        } else if (c == ' ' && !in_single && !in_double) {
            if (!current.empty()) {
                tokens.push_back(current);
                current.clear();
            }
        } else {
            current += c;
        }
    }
    if (!current.empty()) {
        tokens.push_back(current);
    }
    return tokens;
}

int CliApp::run_loop(const std::string& prog) {
    std::cerr << "new_todo interactive mode. Type 'help' or 'quit' to exit.\n";

    while (true) {
        char* raw = readline("todo> ");
        if (raw == nullptr) {
            // EOF (Ctrl-D)
            std::cerr << "\n";
            break;
        }

        std::string line(raw);
        free(raw);

        // trim leading/trailing whitespace
        size_t start = line.find_first_not_of(" \t");
        if (start == std::string::npos) continue;
        size_t end = line.find_last_not_of(" \t");
        line = line.substr(start, end - start + 1);

        if (line.empty()) continue;

        add_history(line.c_str());

        std::vector<std::string> tokens = tokenize(line);
        if (tokens.empty()) continue;

        std::string cmd = tokens[0];

        if (cmd == "quit" || cmd == "exit") {
            break;
        }
        if (cmd == "help") {
            std::cerr << format_usage(prog);
            continue;
        }

        std::vector<std::string> args(tokens.begin() + 1, tokens.end());
        try {
            dispatch(cmd, prog, args);
        } catch (const std::invalid_argument& e) {
            std::cerr << "Error: " << e.what() << "\n";
        } catch (const std::exception& e) {
            std::cerr << "Fatal: " << e.what() << "\n";
        }
    }

    return 0;
}

int CliApp::dispatch(const std::string& cmd, const std::string& prog,
                     const std::vector<std::string>& args) {
    if (cmd == "add") {
        if (args.empty()) {
            std::cerr << "add requires <title>\n";
            return 1;
        }
        return cmd_add(svc_, args[0],
                       parse_flag(args, "--status"),
                       parse_flag(args, "--note"));

    } else if (cmd == "add-child") {
        if (args.size() < 2) {
            std::cerr << "add-child requires <parent_id> <title>\n";
            return 1;
        }
        return cmd_add_child(svc_, std::stoll(args[0]), args[1],
                             parse_flag(args, "--status"),
                             parse_flag(args, "--note"));

    } else if (cmd == "list") {
        return cmd_list(svc_, parse_flag(args, "--status"));

    } else if (cmd == "show-tree") {
        return cmd_show_tree(svc_);

    } else if (cmd == "update") {
        if (args.empty()) {
            std::cerr << "update requires <id>\n";
            return 1;
        }
        return cmd_update(svc_, std::stoll(args[0]),
                          parse_flag(args, "--title"),
                          parse_flag(args, "--status"),
                          parse_flag(args, "--note"));

    } else if (cmd == "delete") {
        if (args.empty()) {
            std::cerr << "delete requires <id>\n";
            return 1;
        }
        return cmd_delete(svc_, std::stoll(args[0]));

    } else if (cmd == "change-parent") {
        if (args.size() < 2) {
            std::cerr << "change-parent requires <id> <new_parent_id>  (use 0 for root)\n";
            return 1;
        }
        return cmd_change_parent(svc_, std::stoll(args[0]), std::stoll(args[1]));

    } else if (cmd == "show") {
        if (args.empty()) {
            std::cerr << "show requires <id>\n";
            return 1;
        }
        return cmd_show(svc_, std::stoll(args[0]));

    } else {
        std::cerr << "Unknown command: " << cmd << "\n";
        std::cerr << format_usage(prog);
        return 1;
    }
}

int CliApp::show_selector() {
    std::vector<std::string> entries = { "TUI", "CLI (interactive)" };
    int selected = 0;
    bool confirmed = false;

    auto screen = ScreenInteractive::TerminalOutput();
    auto menu = Menu(&entries, &selected);
    auto evented = CatchEvent(menu, [&](Event ev) -> bool {
        if (ev == Event::Return) {
            confirmed = true;
            screen.ExitLoopClosure()();
            return true;
        }
        if (ev == Event::Escape) {
            screen.ExitLoopClosure()();
            return true;
        }
        return false;
    });
    auto renderer = Renderer(evented, [&, evented] {
        return vbox({
            text(""),
            text("   new_todo") | bold,
            text(""),
            text("  Select interface:"),
            evented->Render(),
            text(""),
            text("  \u2191\u2193 navigate   Enter confirm   Esc cancel") | dim,
        }) | border | size(WIDTH, EQUAL, 35) | center;
    });
    screen.Loop(renderer);

    // Clear the selector UI from the terminal
    std::cout << "\033[2J\033[H" << std::flush;

    if (!confirmed) return 1;  // Esc -> fall back to CLI
    return selected;            // 0=TUI, 1=CLI
}

} // namespace cli
