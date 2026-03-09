#include "db.h"

#include <sqlite3.h>
#include <functional>
#include <stdexcept>
#include <unordered_map>

// ── helpers
// ───────────────────────────────────────────────────────────────────

static void check(int rc, const std::string &msg)
{
  if (rc != SQLITE_OK && rc != SQLITE_DONE && rc != SQLITE_ROW)
  {
    throw std::runtime_error(msg + ": " + std::to_string(rc));
  }
}

// ── construction / destruction
// ────────────────────────────────────────────────

Database::Database(const std::string &path)
{
  int rc = sqlite3_open(path.c_str(), &db_);
  if (rc != SQLITE_OK)
  {
    std::string err = sqlite3_errmsg(db_);
    sqlite3_close(db_);
    throw std::runtime_error("Cannot open database '" + path + "': " + err);
  }
  // Enable foreign keys
  sqlite3_exec(db_, "PRAGMA foreign_keys = ON;", nullptr, nullptr, nullptr);
  initSchema();
}

Database::~Database()
{
  if (db_)
  {
    sqlite3_close(db_);
    db_ = nullptr;
  }
}

void Database::initSchema()
{
  const char *sql = R"sql(
        CREATE TABLE IF NOT EXISTS todos (
            id          INTEGER PRIMARY KEY AUTOINCREMENT,
            parent_id   INTEGER REFERENCES todos(id),
            title       TEXT    NOT NULL,
            status      TEXT    NOT NULL DEFAULT 'todo',
            ext_info    TEXT    NOT NULL DEFAULT '',
            create_time INTEGER NOT NULL,
            update_time INTEGER NOT NULL,
            due_time    INTEGER NOT NULL DEFAULT 0
        );
        CREATE INDEX IF NOT EXISTS idx_todos_parent_id ON todos(parent_id);

        -- FTS5 virtual table: content= keeps the index in sync with todos.
        -- The indexed document is "title\next_info" so both fields are
        -- searchable in a single query.
        CREATE VIRTUAL TABLE IF NOT EXISTS todos_fts USING fts5(
            content,
            content='todos',
            content_rowid='id',
            tokenize='unicode61'
        );

        -- Keep the FTS index in sync with the todos table.
        CREATE TRIGGER IF NOT EXISTS todos_fts_insert
            AFTER INSERT ON todos BEGIN
                INSERT INTO todos_fts(rowid, content)
                VALUES (new.id, new.title || char(10) || new.ext_info);
            END;

        CREATE TRIGGER IF NOT EXISTS todos_fts_update
            AFTER UPDATE ON todos BEGIN
                INSERT INTO todos_fts(todos_fts, rowid, content)
                VALUES ('delete', old.id, old.title || char(10) || old.ext_info);
                INSERT INTO todos_fts(rowid, content)
                VALUES (new.id, new.title || char(10) || new.ext_info);
            END;

        CREATE TRIGGER IF NOT EXISTS todos_fts_delete
            AFTER DELETE ON todos BEGIN
                INSERT INTO todos_fts(todos_fts, rowid, content)
                VALUES ('delete', old.id, old.title || char(10) || old.ext_info);
            END;
    )sql";
  char *errmsg = nullptr;
  int rc = sqlite3_exec(db_, sql, nullptr, nullptr, &errmsg);
  if (rc != SQLITE_OK)
  {
    std::string e = errmsg ? errmsg : "unknown";
    sqlite3_free(errmsg);
    throw std::runtime_error("Schema init failed: " + e);
  }
  // Migrate existing databases that lack the due_time column (ignore error if
  // the column already exists — SQLite does not support ADD COLUMN IF NOT
  // EXISTS before version 3.37).
  sqlite3_exec(
      db_, "ALTER TABLE todos ADD COLUMN due_time INTEGER NOT NULL DEFAULT 0;",
      nullptr, nullptr, nullptr
  );
  // Migrate: add user_id column for multi-user support (0 = anonymous/legacy).
  sqlite3_exec(
      db_, "ALTER TABLE todos ADD COLUMN user_id INTEGER NOT NULL DEFAULT 0;",
      nullptr, nullptr, nullptr
  );
  // Rebuild the FTS index so that rows inserted before the triggers existed
  // (e.g. in an older database file) are also indexed.
  sqlite3_exec(
      db_,
      "INSERT INTO todos_fts(todos_fts) VALUES ('rebuild');",
      nullptr, nullptr, nullptr
  );
}

// ── row deserialisation
// ───────────────────────────────────────────────────────

Todo Database::rowToTodo(sqlite3_stmt *stmt) const
{
  Todo t;
  t.id = sqlite3_column_int64(stmt, 0);
  // parent_id is NULL for root-level nodes; map NULL → 0
  if (sqlite3_column_type(stmt, 1) == SQLITE_NULL)
    t.parent_id = 0;
  else
    t.parent_id = sqlite3_column_int64(stmt, 1);
  t.user_id = sqlite3_column_int64(stmt, 2);
  auto text = [&](int col) -> std::string
  {
    const unsigned char *p = sqlite3_column_text(stmt, col);
    return p ? reinterpret_cast<const char *>(p) : "";
  };
  t.title       = text(3);
  t.status      = text(4);
  t.ext_info    = text(5);
  t.create_time = sqlite3_column_int64(stmt, 6);
  t.update_time = sqlite3_column_int64(stmt, 7);
  t.due_time    = sqlite3_column_int64(stmt, 8);
  return t;
}

// ── CRUD
// ──────────────────────────────────────────────────────────────────────

int64_t Database::insertTodo(const Todo &todo)
{
  const char *sql =
      "INSERT INTO todos (parent_id, user_id, title, status, ext_info, "
      "create_time, update_time, due_time) "
      "VALUES (?, ?, ?, ?, ?, ?, ?, ?)";
  sqlite3_stmt *stmt = nullptr;
  check(sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr), "prepare insert");

  if (todo.parent_id == 0)
    sqlite3_bind_null(stmt, 1);
  else
    sqlite3_bind_int64(stmt, 1, todo.parent_id);
  sqlite3_bind_int64(stmt, 2, todo.user_id);
  sqlite3_bind_text(stmt, 3, todo.title.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 4, todo.status.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 5, todo.ext_info.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_int64(stmt, 6, todo.create_time);
  sqlite3_bind_int64(stmt, 7, todo.update_time);
  sqlite3_bind_int64(stmt, 8, todo.due_time);

  int rc = sqlite3_step(stmt);
  sqlite3_finalize(stmt);
  if (rc != SQLITE_DONE)
    throw std::runtime_error("insertTodo failed: " + std::to_string(rc));
  return sqlite3_last_insert_rowid(db_);
}

std::optional<Todo> Database::getTodo(int64_t id) const
{
  const char *sql =
      "SELECT id,parent_id,user_id,title,status,ext_info,"
      "create_time,update_time,due_time FROM todos WHERE id=?";
  sqlite3_stmt *stmt = nullptr;
  check(sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr), "prepare getTodo");
  sqlite3_bind_int64(stmt, 1, id);
  std::optional<Todo> result;
  if (sqlite3_step(stmt) == SQLITE_ROW)
    result = rowToTodo(stmt);
  sqlite3_finalize(stmt);
  return result;
}

std::vector<Todo> Database::getChildren(int64_t parent_id,
                                        int64_t user_id) const
{
  std::string sql =
      "SELECT id,parent_id,user_id,title,status,ext_info,"
      "create_time,update_time,due_time FROM todos WHERE ";
  sqlite3_stmt *stmt = nullptr;
  std::string pid_clause =
      (parent_id == 0) ? "parent_id IS NULL" : "parent_id=?";
  std::string uid_clause = (user_id != 0) ? " AND user_id=?" : "";
  sql += pid_clause + uid_clause;

  check(
      sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr),
      "prepare getChildren"
  );
  int bind = 1;
  if (parent_id != 0)
    sqlite3_bind_int64(stmt, bind++, parent_id);
  if (user_id != 0)
    sqlite3_bind_int64(stmt, bind++, user_id);

  std::vector<Todo> result;
  while (sqlite3_step(stmt) == SQLITE_ROW)
    result.push_back(rowToTodo(stmt));
  sqlite3_finalize(stmt);
  return result;
}

std::vector<Todo> Database::getAllTodos(int64_t user_id) const
{
  std::string sql =
      "SELECT id,parent_id,user_id,title,status,ext_info,"
      "create_time,update_time,due_time FROM todos";
  if (user_id != 0)
    sql += " WHERE user_id=?";
  sqlite3_stmt *stmt = nullptr;
  check(
      sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr),
      "prepare getAllTodos"
  );
  if (user_id != 0)
    sqlite3_bind_int64(stmt, 1, user_id);
  std::vector<Todo> result;
  while (sqlite3_step(stmt) == SQLITE_ROW)
    result.push_back(rowToTodo(stmt));
  sqlite3_finalize(stmt);
  return result;
}

bool Database::updateTodo(const Todo &todo)
{
  const char *sql = "UPDATE todos SET parent_id=?, user_id=?, title=?, "
                    "status=?, ext_info=?, update_time=?, due_time=? "
                    "WHERE id=?";
  sqlite3_stmt *stmt = nullptr;
  check(sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr), "prepare updateTodo");

  if (todo.parent_id == 0)
    sqlite3_bind_null(stmt, 1);
  else
    sqlite3_bind_int64(stmt, 1, todo.parent_id);
  sqlite3_bind_int64(stmt, 2, todo.user_id);
  sqlite3_bind_text(stmt, 3, todo.title.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 4, todo.status.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 5, todo.ext_info.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_int64(stmt, 6, todo.update_time);
  sqlite3_bind_int64(stmt, 7, todo.due_time);
  sqlite3_bind_int64(stmt, 8, todo.id);

  int rc = sqlite3_step(stmt);
  sqlite3_finalize(stmt);
  if (rc != SQLITE_DONE)
    throw std::runtime_error("updateTodo failed: " + std::to_string(rc));
  return sqlite3_changes(db_) > 0;
}

// Cascade delete: delete all descendants first, then the node itself.
// We do a BFS/DFS in application code because SQLite FK cascade requires
// "ON DELETE CASCADE" which we avoid to keep the schema simple.
int Database::deleteTodo(int64_t id)
{
  // Collect all ids to delete (BFS)
  std::vector<int64_t> to_delete;
  std::vector<int64_t> queue = {id};
  while (!queue.empty())
  {
    int64_t cur = queue.back();
    queue.pop_back();
    to_delete.push_back(cur);
    auto children = getChildren(cur);
    for (auto &c : children)
      queue.push_back(c.id);
  }

  int count = 0;
  // Delete leaves first (reverse order gives bottom-up within a path,
  // but we just delete all at once since FK is already handled by ordering)
  for (int i = (int)to_delete.size() - 1; i >= 0; --i)
  {
    const char *sql = "DELETE FROM todos WHERE id=?";
    sqlite3_stmt *stmt = nullptr;
    check(
        sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr), "prepare deleteTodo"
    );
    sqlite3_bind_int64(stmt, 1, to_delete[i]);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    count += sqlite3_changes(db_);
  }
  return count;
}

// ── tree operations
// ───────────────────────────────────────────────────────────

std::vector<TodoNode> Database::buildTree(int64_t root_parent_id,
                                          int64_t user_id) const
{
  std::vector<Todo> all = getAllTodos(user_id);

  // Build adjacency map
  std::unordered_map<int64_t, std::vector<Todo *>> by_parent;
  for (auto &t : all)
  {
    by_parent[t.parent_id].push_back(&t);
  }

  // Recursive lambda to build sub-trees
  std::function<std::vector<TodoNode>(int64_t)> build =
      [&](int64_t pid) -> std::vector<TodoNode>
  {
    std::vector<TodoNode> nodes;
    auto it = by_parent.find(pid);
    if (it == by_parent.end())
      return nodes;
    for (auto *tp : it->second)
    {
      TodoNode n;
      n.todo = *tp;
      n.children = build(tp->id);
      nodes.push_back(std::move(n));
    }
    return nodes;
  };

  return build(root_parent_id);
}

std::vector<Todo> Database::getAncestors(int64_t id) const
{
  std::vector<Todo> ancestors;
  std::optional<Todo> cur = getTodo(id);
  if (!cur)
    return ancestors;

  // Walk up, collecting parent chain
  std::vector<Todo> chain;
  int64_t pid = cur->parent_id;
  while (pid != 0)
  {
    auto p = getTodo(pid);
    if (!p)
      break;
    chain.push_back(*p);
    pid = p->parent_id;
  }
  // Reverse so result is root-down order
  ancestors.assign(chain.rbegin(), chain.rend());
  return ancestors;
}

std::vector<Todo> Database::searchTodos(const std::string &query,
                                        int64_t user_id) const
{
  std::string sql =
      "SELECT t.id, t.parent_id, t.user_id, t.title, t.status, t.ext_info, "
      "       t.create_time, t.update_time, t.due_time "
      "FROM todos t "
      "JOIN todos_fts f ON f.rowid = t.id "
      "WHERE todos_fts MATCH ?";
  if (user_id != 0)
    sql += " AND t.user_id=?";
  sql += " ORDER BY rank";

  sqlite3_stmt *stmt = nullptr;
  check(
      sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr),
      "prepare searchTodos"
  );
  sqlite3_bind_text(stmt, 1, query.c_str(), -1, SQLITE_TRANSIENT);
  if (user_id != 0)
    sqlite3_bind_int64(stmt, 2, user_id);

  std::vector<Todo> result;
  while (sqlite3_step(stmt) == SQLITE_ROW)
    result.push_back(rowToTodo(stmt));
  sqlite3_finalize(stmt);
  return result;
}
