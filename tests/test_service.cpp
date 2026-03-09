#include <gtest/gtest.h>
#include "../src/db.h"
#include "../src/config.h"
#include "../src/todo_service.h"

// ── fixture
// ───────────────────────────────────────────────────────────────────

class ServiceTest : public ::testing::Test
{
protected:
  Database db{":memory:"};
  AppConfig cfg = ConfigLoader::defaults();
  TodoService svc{db, cfg};
};

// ── tests
// ─────────────────────────────────────────────────────────────────────

TEST_F(ServiceTest, AddTodoUsesDefaultStatus)
{
  int64_t id = svc.addTodo("Buy eggs");
  auto t = svc.findById(id);
  ASSERT_TRUE(t.has_value());
  EXPECT_EQ(t->status, cfg.default_status);
  EXPECT_EQ(t->title, "Buy eggs");
}

TEST_F(ServiceTest, AddTodoWithExplicitStatus)
{
  int64_t id = svc.addTodo("Finish report", "in_progress");
  auto t = svc.findById(id);
  ASSERT_TRUE(t.has_value());
  EXPECT_EQ(t->status, "in_progress");
}

TEST_F(ServiceTest, AddTodoWithInvalidStatusThrows)
{
  EXPECT_THROW(svc.addTodo("Foo", "invalid_status"), std::invalid_argument);
}

TEST_F(ServiceTest, AddTodoWithEmptyTitleThrows)
{
  EXPECT_THROW(svc.addTodo(""), std::invalid_argument);
}

TEST_F(ServiceTest, AddChildSuccess)
{
  int64_t parent = svc.addTodo("Parent");
  int64_t child = svc.addChild(parent, "Child");
  auto t = svc.findById(child);
  ASSERT_TRUE(t.has_value());
  EXPECT_EQ(t->parent_id, parent);
}

TEST_F(ServiceTest, AddChildWithInvalidParentThrows)
{
  EXPECT_THROW(svc.addChild(9999, "Child"), std::invalid_argument);
}

TEST_F(ServiceTest, PartialUpdateTitle)
{
  int64_t id = svc.addTodo("Old title");
  svc.updateTodo(id, "New title", std::nullopt, std::nullopt);
  auto t = svc.findById(id);
  ASSERT_TRUE(t.has_value());
  EXPECT_EQ(t->title, "New title");
  EXPECT_EQ(t->status, cfg.default_status); // unchanged
}

TEST_F(ServiceTest, PartialUpdateStatus)
{
  int64_t id = svc.addTodo("Task");
  svc.updateTodo(id, std::nullopt, "done", std::nullopt);
  auto t = svc.findById(id);
  ASSERT_TRUE(t.has_value());
  EXPECT_EQ(t->status, "done");
  EXPECT_EQ(t->title, "Task"); // unchanged
}

TEST_F(ServiceTest, UpdateWithInvalidStatusThrows)
{
  int64_t id = svc.addTodo("Task");
  EXPECT_THROW(
      svc.updateTodo(id, std::nullopt, "bad_status", std::nullopt),
      std::invalid_argument
  );
}

TEST_F(ServiceTest, UpdateNonExistentThrows)
{
  EXPECT_THROW(
      svc.updateTodo(9999, "Title", std::nullopt, std::nullopt),
      std::invalid_argument
  );
}

TEST_F(ServiceTest, ListByStatus)
{
  svc.addTodo("A", "todo");
  svc.addTodo("B", "done");
  svc.addTodo("C", "todo");

  auto todos = svc.listByStatus("todo");
  EXPECT_EQ(todos.size(), 2u);

  auto done = svc.listByStatus("done");
  EXPECT_EQ(done.size(), 1u);
}

TEST_F(ServiceTest, ListByInvalidStatusThrows)
{
  EXPECT_THROW(svc.listByStatus("nope"), std::invalid_argument);
}

TEST_F(ServiceTest, DeleteTodoRemovesIt)
{
  int64_t id = svc.addTodo("Delete me");
  svc.deleteTodo(id);
  EXPECT_FALSE(svc.findById(id).has_value());
}

TEST_F(ServiceTest, DeleteNonExistentThrows)
{
  EXPECT_THROW(svc.deleteTodo(9999), std::invalid_argument);
}

TEST_F(ServiceTest, GetTree)
{
  int64_t a = svc.addTodo("A");
  svc.addChild(a, "A1");
  svc.addTodo("B");

  auto tree = svc.getTree();
  EXPECT_EQ(tree.size(), 2u);
}

TEST_F(ServiceTest, ExtInfoStoredAndRetrieved)
{
  int64_t id = svc.addTodo("Task", "", "some note");
  auto t = svc.findById(id);
  ASSERT_TRUE(t.has_value());
  EXPECT_EQ(t->ext_info, "some note");
}

TEST_F(ServiceTest, DueTimeStoredAndRetrieved)
{
  int64_t id = svc.addTodo("Task with due");
  svc.updateTodo(
      id, std::nullopt, std::nullopt, std::nullopt,
      std::optional<Timestamp>(1234567890)
  );
  auto t = svc.findById(id);
  ASSERT_TRUE(t.has_value());
  EXPECT_EQ(t->due_time, 1234567890);
}

TEST_F(ServiceTest, DueTimeClearedWithZero)
{
  int64_t id = svc.addTodo("Task");
  svc.updateTodo(
      id, std::nullopt, std::nullopt, std::nullopt,
      std::optional<Timestamp>(9999)
  );
  svc.updateTodo(
      id, std::nullopt, std::nullopt, std::nullopt, std::optional<Timestamp>(0)
  );
  auto t = svc.findById(id);
  ASSERT_TRUE(t.has_value());
  EXPECT_EQ(t->due_time, 0);
}
