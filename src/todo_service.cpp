#include "todo_service.h"

#include <algorithm>
#include <stdexcept>

TodoService::TodoService(Database& db, const AppConfig& config)
    : db_(db), config_(config) {}

std::string TodoService::resolveStatus(const std::string& s) const {
    if (s.empty()) return config_.default_status;
    if (!config_.isValidStatus(s)) {
        throw std::invalid_argument("Invalid status: '" + s + "'");
    }
    return s;
}

int64_t TodoService::addTodo(const std::string& title,
                              const std::string& status,
                              const std::string& ext_info) {
    if (title.empty()) throw std::invalid_argument("Title must not be empty");
    Todo t;
    t.title       = title;
    t.status      = resolveStatus(status);
    t.ext_info    = ext_info;
    t.parent_id   = 0;
    t.create_time = now_timestamp();
    t.update_time = t.create_time;
    return db_.insertTodo(t);
}

int64_t TodoService::addChild(int64_t parent_id, const std::string& title,
                               const std::string& status,
                               const std::string& ext_info) {
    if (title.empty()) throw std::invalid_argument("Title must not be empty");
    if (!db_.getTodo(parent_id).has_value()) {
        throw std::invalid_argument("Parent todo not found: " + std::to_string(parent_id));
    }
    Todo t;
    t.title       = title;
    t.status      = resolveStatus(status);
    t.ext_info    = ext_info;
    t.parent_id   = parent_id;
    t.create_time = now_timestamp();
    t.update_time = t.create_time;
    return db_.insertTodo(t);
}

void TodoService::updateTodo(int64_t id,
                              std::optional<std::string> title,
                              std::optional<std::string> status,
                              std::optional<std::string> ext_info,
                              std::optional<Timestamp>   due_time) {
    auto existing = db_.getTodo(id);
    if (!existing.has_value()) {
        throw std::invalid_argument("Todo not found: " + std::to_string(id));
    }
    Todo t = *existing;
    if (title.has_value()) {
        if (title->empty()) throw std::invalid_argument("Title must not be empty");
        t.title = *title;
    }
    if (status.has_value()) {
        if (!config_.isValidStatus(*status)) {
            throw std::invalid_argument("Invalid status: '" + *status + "'");
        }
        t.status = *status;
    }
    if (ext_info.has_value()) {
        t.ext_info = *ext_info;
    }
    if (due_time.has_value()) {
        t.due_time = *due_time;
    }
    t.update_time = now_timestamp();
    db_.updateTodo(t);
}

void TodoService::deleteTodo(int64_t id) {
    if (!db_.getTodo(id).has_value()) {
        throw std::invalid_argument("Todo not found: " + std::to_string(id));
    }
    db_.deleteTodo(id);
}

std::vector<Todo> TodoService::listAll() const {
    return db_.getAllTodos();
}

std::vector<Todo> TodoService::listByStatus(const std::string& s) const {
    if (!config_.isValidStatus(s)) {
        throw std::invalid_argument("Invalid status: '" + s + "'");
    }
    auto all = db_.getAllTodos();
    std::vector<Todo> result;
    for (auto& t : all) {
        if (t.status == s) result.push_back(t);
    }
    return result;
}

std::vector<TodoNode> TodoService::getTree() const {
    return db_.buildTree(0);
}

std::optional<Todo> TodoService::findById(int64_t id) const {
    return db_.getTodo(id);
}
