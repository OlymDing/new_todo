#include <gtest/gtest.h>
#include "../src/db.h"
#include "../src/todo.h"

#include <algorithm>

// ── fixture ───────────────────────────────────────────────────────────────────

class DbTest : public ::testing::Test {
protected:
    Database db{":memory:"};

    Todo make(const std::string& title, int64_t parent_id = 0,
              const std::string& status = "todo") {
        Todo t;
        t.title       = title;
        t.parent_id   = parent_id;
        t.status      = status;
        t.create_time = 1000;
        t.update_time = 1000;
        return t;
    }
};

// ── tests ─────────────────────────────────────────────────────────────────────

TEST_F(DbTest, InsertAndGet) {
    Todo t = make("Buy milk");
    int64_t id = db.insertTodo(t);
    EXPECT_GT(id, 0);

    auto got = db.getTodo(id);
    ASSERT_TRUE(got.has_value());
    EXPECT_EQ(got->title, "Buy milk");
    EXPECT_EQ(got->parent_id, 0);
    EXPECT_EQ(got->status, "todo");
}

TEST_F(DbTest, GetMissingReturnsNullopt) {
    auto got = db.getTodo(9999);
    EXPECT_FALSE(got.has_value());
}

TEST_F(DbTest, GetChildren) {
    int64_t parent = db.insertTodo(make("Parent"));
    db.insertTodo(make("Child 1", parent));
    db.insertTodo(make("Child 2", parent));
    db.insertTodo(make("Root level"));  // should not appear

    auto children = db.getChildren(parent);
    EXPECT_EQ(children.size(), 2u);
}

TEST_F(DbTest, GetChildrenOfRoot) {
    db.insertTodo(make("Root A"));
    db.insertTodo(make("Root B"));
    int64_t parent = db.insertTodo(make("Root C"));
    db.insertTodo(make("Child", parent));

    auto roots = db.getChildren(0);
    EXPECT_EQ(roots.size(), 3u);
}

TEST_F(DbTest, UpdateTodo) {
    int64_t id = db.insertTodo(make("Original"));
    auto t = db.getTodo(id);
    ASSERT_TRUE(t.has_value());
    t->title       = "Updated";
    t->status      = "done";
    t->update_time = 2000;
    bool ok = db.updateTodo(*t);
    EXPECT_TRUE(ok);

    auto got = db.getTodo(id);
    ASSERT_TRUE(got.has_value());
    EXPECT_EQ(got->title, "Updated");
    EXPECT_EQ(got->status, "done");
}

TEST_F(DbTest, UpdateNonExistentReturnsFalse) {
    Todo t = make("Ghost");
    t.id = 9999;
    bool ok = db.updateTodo(t);
    EXPECT_FALSE(ok);
}

TEST_F(DbTest, CascadeDelete) {
    int64_t parent = db.insertTodo(make("Parent"));
    int64_t child  = db.insertTodo(make("Child", parent));
    db.insertTodo(make("Grandchild", child));

    int deleted = db.deleteTodo(parent);
    EXPECT_EQ(deleted, 3);
    EXPECT_FALSE(db.getTodo(parent).has_value());
    EXPECT_FALSE(db.getTodo(child).has_value());
}

TEST_F(DbTest, DeleteNonExistentReturns0) {
    int deleted = db.deleteTodo(9999);
    EXPECT_EQ(deleted, 0);
}

TEST_F(DbTest, BuildTree) {
    int64_t a  = db.insertTodo(make("A"));
    int64_t b  = db.insertTodo(make("B"));
    int64_t a1 = db.insertTodo(make("A1", a));
    db.insertTodo(make("A2", a));
    db.insertTodo(make("A1a", a1));

    auto tree = db.buildTree(0);
    EXPECT_EQ(tree.size(), 2u);  // A and B at root

    // Find node A
    auto it = std::find_if(tree.begin(), tree.end(),
                           [&](const TodoNode& n){ return n.todo.id == a; });
    ASSERT_NE(it, tree.end());
    EXPECT_EQ(it->children.size(), 2u);  // A1 and A2

    auto it2 = std::find_if(it->children.begin(), it->children.end(),
                             [&](const TodoNode& n){ return n.todo.id == a1; });
    ASSERT_NE(it2, it->children.end());
    EXPECT_EQ(it2->children.size(), 1u);  // A1a
}

TEST_F(DbTest, GetAncestors) {
    int64_t a   = db.insertTodo(make("A"));
    int64_t a1  = db.insertTodo(make("A1", a));
    int64_t a1a = db.insertTodo(make("A1a", a1));

    auto ancestors = db.getAncestors(a1a);
    ASSERT_EQ(ancestors.size(), 2u);
    EXPECT_EQ(ancestors[0].id, a);
    EXPECT_EQ(ancestors[1].id, a1);
}

TEST_F(DbTest, GetAncestorsForRoot) {
    int64_t id = db.insertTodo(make("Root"));
    auto ancestors = db.getAncestors(id);
    EXPECT_TRUE(ancestors.empty());
}
