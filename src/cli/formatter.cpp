#include "cli/formatter.h"
#include "todo.h"

#include <sstream>

namespace cli {

std::string format_todo(const Todo& t, const std::string& indent) {
    std::ostringstream oss;
    oss << indent
        << "[" << t.id << "] "
        << t.title
        << "  (" << t.status << ")";
    if (!t.ext_info.empty()) oss << "  note: " << t.ext_info;
    oss << "\n";
    return oss.str();
}

std::string format_tree(const std::vector<TodoNode>& nodes, int depth) {
    std::string indent(depth * 2, ' ');
    std::string result;
    for (const auto& n : nodes) {
        result += format_todo(n.todo, indent);
        result += format_tree(n.children, depth + 1);
    }
    return result;
}

std::string format_empty() {
    return "(no todos)\n";
}

std::string format_created(int64_t id) {
    return "Created todo #" + std::to_string(id) + "\n";
}

std::string format_created_child(int64_t id) {
    return "Created child todo #" + std::to_string(id) + "\n";
}

std::string format_updated(int64_t id) {
    return "Updated todo #" + std::to_string(id) + "\n";
}

std::string format_deleted(int64_t id) {
    return "Deleted todo #" + std::to_string(id) + " (and its children)\n";
}

std::string format_usage(const std::string& prog) {
    return
        "Usage:\n"
        "  " + prog + " add <title> [--status <s>] [--note <text>]\n"
        "  " + prog + " add-child <parent_id> <title> [--status <s>] [--note <text>]\n"
        "  " + prog + " list [--status <s>]\n"
        "  " + prog + " show-tree\n"
        "  " + prog + " update <id> [--title <t>] [--status <s>] [--note <text>]\n"
        "  " + prog + " delete <id>\n"
        "  " + prog + " change-parent <id> <new_parent_id>  (0 = make root)\n"
        "  " + prog + " show <id>\n";
}

} // namespace cli
