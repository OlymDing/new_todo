# new_todo

船新的todo记事本，通过CLI + TUI交互，实乃居家办公 + 外出旅行必备！

未来计划是一个去中心化快速团队TODO对齐工具，告别那些慢的要死的团队写作工具，拥抱高效人生

下面都是AI写的废话，可看可不看

have fun

---

## Features

- Hierarchical todos (parent / child tree, unlimited depth)
- Multi-user accounts with SHA-256 hashed passwords
- TTY-scoped session files — log in once per terminal (8-hour TTL)
- Full-text search powered by SQLite FTS5
- Full-screen TUI (FTXUI) with resizable split panels and modal overlays
- Interactive CLI with GNU Readline history
- Due dates, notes, configurable status values
- All state stored in a single SQLite database file

---

## Build

Requires: `xmake`, `clang++` / `g++` (C++17), and the following system
libraries:

| Library | Package (Arch / Debian) |
|---|---|
| SQLite3 + FTS5 | `sqlite` / `libsqlite3-dev` |
| OpenSSL | `openssl` / `libssl-dev` |
| GNU Readline | `readline` / `libreadline-dev` |
| FTXUI | build from source or AUR `ftxui` |
| GoogleTest | `gtest` / `libgtest-dev` |

```
xmake build new_todo
xmake run  new_todo
```

Run all tests:

```
xmake build test_todo test_db test_service test_config test_auth
xmake run test_todo && xmake run test_db && xmake run test_service \
  && xmake run test_config && xmake run test_auth
```

---

## Configuration

`config.json` (looked up in the working directory at startup):

```json
{
  "statuses":        ["todo", "in_progress", "done"],
  "default_status":  "todo",
  "db_path":         "todo.db"
}
```

If the file is missing, built-in defaults are used.

---

## Usage

### Interface selection

When launched interactively without arguments, a menu lets you choose between
the TUI and the interactive CLI.

### TUI key bindings

| Key | Action |
|---|---|
| `j` / `k` or `↑` / `↓` | Navigate list |
| `a` | Add root todo |
| `c` | Add child under selected |
| `d` | Delete selected (with confirmation) |
| `u` | Open edit panel (title, status, due date, notes) |
| `p` | Change parent of selected todo |
| `/` | Full-text search |
| `q` | Quit (session kept) |
| `Esc` | Logout confirmation dialog |

Inside the edit panel, `Tab` moves between fields; the Status field is a
dropdown; `Save` / `Cancel` buttons or `Esc` close the panel.

### CLI one-shot commands

```
new_todo add <title> [--status <s>] [--note <text>]
new_todo add-child <parent_id> <title> [--status <s>] [--note <text>]
new_todo list [--status <s>]
new_todo show-tree
new_todo show <id>
new_todo update <id> [--title <t>] [--status <s>] [--note <text>]
new_todo delete <id>
new_todo change-parent <id> <new_parent_id>   # 0 = make root
new_todo search <query>
new_todo logout
```

### Non-interactive / scripting

Set `TODO_USER` and `TODO_PASS` environment variables to bypass the login
screen:

```
TODO_USER=alice TODO_PASS=secret new_todo list
```

---

## Architecture

```
┌─────────────────────────────────────────────────────────────────┐
│                           new_todo                              │
│                          (main.cpp)                             │
│          ConfigLoader::load()  ──►  AppConfig                   │
│          Database(path)                                         │
│          CliApp(db, cfg).run()                                  │
└───────────────────────────┬─────────────────────────────────────┘
                            │
          ┌─────────────────▼──────────────────┐
          │            CLI Layer               │
          │         src/cli/cli_app            │
          │                                    │
          │  authenticate()                    │
          │    ├─ SessionManager.load()        │
          │    ├─ env: TODO_USER / TODO_PASS   │
          │    └─ FTXUI login / register form  │
          │                                    │
          │  show_selector()  ──► TUI or CLI   │
          │  run_loop()  (readline REPL)       │
          │  dispatch()  ──► commands.cpp      │
          │                  formatter.cpp     │
          └──────┬──────────────────┬──────────┘
                 │                  │
    ┌────────────▼──────┐  ┌────────▼────────────────────────────┐
    │    TUI Layer      │  │          Domain Layer               │
    │  src/tui/tui_app  │  │                                     │
    │                   │  │  TodoService  (user_id scoped)      │
    │  ScreenInteractive│  │    addTodo / addChild               │
    │  ResizableSplit   │  │    updateTodo / deleteTodo          │
    │  Left panel       │  │    changeParent  (cycle detect)     │
    │    todo tree list │  │    listAll / getTree                │
    │  Right panel      │  │    search()                         │
    │    Detail / Edit  │  │                                     │
    │  Modal overlays   │  │  AuthService                        │
    │    AddTodo        │  │    registerUser()                   │
    │    ConfirmDelete  │  │    login()                          │
    │    EditDetail     │  │    hashPassword()  SHA-256          │
    │    ChangeParent   │  │    randomSalt()    RAND_bytes       │
    │    Search         │  │                                     │
    │    ConfirmLogout  │  │  SessionManager                     │
    │  Dropdown(status) │  │    /tmp/new_todo_session_<tty>      │
    └────────┬──────────┘  │    load() / save() / clear()        │
             │             │    TTL = 8 hours                    │
             └──────┐      │                                     │
                    │      └─────────────────────────────────────┘
                    │                      │
          ┌─────────▼──────────────────────▼──────────┐
          │               Data Layer                  │
          │            src/db.h / db.cpp              │
          │                                           │
          │  Database  (SQLite3 wrapper)              │
          │                                           │
          │  todos table                              │
          │    id, parent_id, user_id                 │
          │    title, status, ext_info                │
          │    create_time, update_time, due_time     │
          │                                           │
          │  todos_fts  (FTS5 virtual table)          │
          │    content = todos  (title + ext_info)    │
          │    sync triggers: insert / update /delete │
          │                                           │
          │  users table                              │
          │    id, username, password_hash, salt      │
          │                                           │
          │  insertTodo / updateTodo / deleteTodo     │
          │  buildTree / getChildren / getAncestors   │
          │  searchTodos  (FTS5 MATCH query)          │
          │  rawHandle()  →  AuthService schema init  │
          └───────────────────┬───────────────────────┘
                              │
          ┌───────────────────▼───────────────────────┐
          │           External Libraries              │
          │                                           │
          │  SQLite3 + FTS5   persistence & search    │
          │  OpenSSL          SHA-256, RAND_bytes      │
          │  FTXUI            TUI rendering & events  │
          │  GNU Readline     CLI history & editing   │
          └───────────────────────────────────────────┘


  Build targets (xmake.lua)
  ─────────────────────────
  todo_lib   static  todo + db + config + auth + session
  cli_lib    static  cli_app + commands + formatter
  tui_lib    static  tui_app
  new_todo   binary  main  ←  cli_lib + tui_lib + todo_lib

  test_todo     unit tests: Todo struct & timestamps
  test_db       unit tests: Database CRUD + 7 FTS tests
  test_service  unit tests: TodoService operations
  test_config   unit tests: ConfigLoader
  test_auth     unit tests: AuthService (10 tests)
```

---

## Project layout

```
new_todo/
├── config.json          runtime configuration
├── todo.db              SQLite database (created on first run)
├── xmake.lua            build definition
├── src/
│   ├── main.cpp
│   ├── todo.h / todo.cpp          Todo, TodoNode, Timestamp
│   ├── db.h / db.cpp              Database (SQLite3 wrapper)
│   ├── config.h / config.cpp      AppConfig, ConfigLoader
│   ├── todo_service.h / .cpp      TodoService (domain logic)
│   ├── auth.h / auth.cpp          AuthService (register / login)
│   ├── session.h / session.cpp    SessionManager (TTY sessions)
│   ├── cli/
│   │   ├── cli_app.h / .cpp       CliApp, auth UI, REPL, dispatch
│   │   ├── commands.h / .cpp      cmd_* functions
│   │   └── formatter.h / .cpp     output formatting helpers
│   └── tui/
│       └── tui_app.h / .cpp       TuiApp (FTXUI full-screen UI)
└── tests/
    ├── test_todo.cpp
    ├── test_db.cpp
    ├── test_service.cpp
    ├── test_config.cpp
    └── test_auth.cpp
```

---

## Data model

```
users
  id            INTEGER PK
  username      TEXT UNIQUE
  password_hash TEXT          SHA-256(salt + password) hex
  salt          TEXT          16-byte random hex

todos
  id            INTEGER PK
  parent_id     INTEGER       0 = root level
  user_id       INTEGER       FK → users.id
  title         TEXT
  status        TEXT          configurable (default: todo/in_progress/done)
  ext_info      TEXT          free-form notes
  create_time   INTEGER       Unix timestamp
  update_time   INTEGER       Unix timestamp
  due_time      INTEGER       Unix timestamp, 0 = none

todos_fts  (FTS5 virtual table, content=todos)
  title
  ext_info
  — kept in sync by INSERT / UPDATE / DELETE triggers
```

---

## License

MIT
