#include <iostream>
#include <string>
#include <vector>
#include <optional>
#include <stdexcept>
#include <cstdlib>

#include "config.h"
#include "db.h"
#include "todo.h"
#include "todo_service.h"

// ── helpers ───────────────────────────────────────────────────────────────────

static void print_todo(const Todo& t, const std::string& indent = "") {
    std::cout << indent
              << "[" << t.id << "] "
              << t.title
              << "  (" << t.status << ")";
    if (!t.ext_info.empty()) std::cout << "  note: " << t.ext_info;
    std::cout << "\n";
}

static void print_tree(const std::vector<TodoNode>& nodes, int depth = 0) {
    std::string indent(depth * 2, ' ');
    for (auto& n : nodes) {
        print_todo(n.todo, indent);
        print_tree(n.children, depth + 1);
    }
}

static void usage(const char* prog) {
    std::cerr <<
        "Usage:\n"
        "  " << prog << " add <title> [--status <s>] [--note <text>]\n"
        "  " << prog << " add-child <parent_id> <title> [--status <s>] [--note <text>]\n"
        "  " << prog << " list [--status <s>]\n"
        "  " << prog << " show-tree\n"
        "  " << prog << " update <id> [--title <t>] [--status <s>] [--note <text>]\n"
        "  " << prog << " delete <id>\n"
        "  " << prog << " show <id>\n";
}

// Simple flag parser: find value after a given flag name, or return empty optional.
static std::optional<std::string> flag(const std::vector<std::string>& args,
                                        const std::string& name) {
    for (size_t i = 0; i + 1 < args.size(); ++i) {
        if (args[i] == name) return args[i + 1];
    }
    return std::nullopt;
}

// ── main ──────────────────────────────────────────────────────────────────────

int main(int argc, char** argv) {
    if (argc < 2) { usage(argv[0]); return 1; }

    // Load config (optional – fall back to defaults if not found)
    AppConfig cfg;
    try {
        cfg = ConfigLoader::load("config.json");
    } catch (...) {
        cfg = ConfigLoader::defaults();
    }

    Database    db(cfg.db_path);
    TodoService svc(db, cfg);

    std::string cmd = argv[1];
    std::vector<std::string> args(argv + 2, argv + argc);

    try {
        if (cmd == "add") {
            if (args.empty()) { std::cerr << "add requires <title>\n"; return 1; }
            std::string title  = args[0];
            auto status   = flag(args, "--status");
            auto note     = flag(args, "--note");
            int64_t id = svc.addTodo(title,
                                     status  ? *status  : "",
                                     note    ? *note    : "");
            std::cout << "Created todo #" << id << "\n";

        } else if (cmd == "add-child") {
            if (args.size() < 2) {
                std::cerr << "add-child requires <parent_id> <title>\n"; return 1;
            }
            int64_t parent_id = std::stoll(args[0]);
            std::string title = args[1];
            auto status = flag(args, "--status");
            auto note   = flag(args, "--note");
            int64_t id = svc.addChild(parent_id, title,
                                      status ? *status : "",
                                      note   ? *note   : "");
            std::cout << "Created child todo #" << id << "\n";

        } else if (cmd == "list") {
            auto status = flag(args, "--status");
            std::vector<Todo> todos;
            if (status) {
                todos = svc.listByStatus(*status);
            } else {
                todos = svc.listAll();
            }
            if (todos.empty()) {
                std::cout << "(no todos)\n";
            } else {
                for (auto& t : todos) print_todo(t);
            }

        } else if (cmd == "show-tree") {
            auto tree = svc.getTree();
            if (tree.empty()) {
                std::cout << "(no todos)\n";
            } else {
                print_tree(tree);
            }

        } else if (cmd == "update") {
            if (args.empty()) { std::cerr << "update requires <id>\n"; return 1; }
            int64_t id = std::stoll(args[0]);
            auto title  = flag(args, "--title");
            auto status = flag(args, "--status");
            auto note   = flag(args, "--note");
            svc.updateTodo(id, title, status, note);
            std::cout << "Updated todo #" << id << "\n";

        } else if (cmd == "delete") {
            if (args.empty()) { std::cerr << "delete requires <id>\n"; return 1; }
            int64_t id = std::stoll(args[0]);
            svc.deleteTodo(id);
            std::cout << "Deleted todo #" << id << " (and its children)\n";

        } else if (cmd == "show") {
            if (args.empty()) { std::cerr << "show requires <id>\n"; return 1; }
            int64_t id = std::stoll(args[0]);
            auto t = svc.findById(id);
            if (!t) { std::cerr << "Todo #" << id << " not found\n"; return 1; }
            print_todo(*t);

        } else {
            std::cerr << "Unknown command: " << cmd << "\n";
            usage(argv[0]);
            return 1;
        }
    } catch (const std::invalid_argument& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    } catch (const std::exception& e) {
        std::cerr << "Fatal: " << e.what() << "\n";
        return 2;
    }

    return 0;
}
