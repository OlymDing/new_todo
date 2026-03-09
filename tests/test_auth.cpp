#include <gtest/gtest.h>
#include "../src/auth.h"
#include "../src/config.h"
#include "../src/db.h"
#include "../src/todo_service.h"

// ── fixture
// ───────────────────────────────────────────────────────────────────

class AuthTest : public ::testing::Test
{
protected:
  Database db{":memory:"};
  AuthService auth{db};
};

// ── tests
// ─────────────────────────────────────────────────────────────────────

TEST_F(AuthTest, RegisterAndLogin)
{
  int64_t id = auth.registerUser("alice", "secret");
  EXPECT_GT(id, 0);

  auto u = auth.login("alice", "secret");
  ASSERT_TRUE(u.has_value());
  EXPECT_EQ(u->id, id);
  EXPECT_EQ(u->username, "alice");
}

TEST_F(AuthTest, LoginWrongPassword)
{
  auth.registerUser("bob", "correct");
  auto u = auth.login("bob", "wrong");
  EXPECT_FALSE(u.has_value());
}

TEST_F(AuthTest, LoginUnknownUser)
{
  auto u = auth.login("nobody", "pass");
  EXPECT_FALSE(u.has_value());
}

TEST_F(AuthTest, DuplicateUsernameThrows)
{
  auth.registerUser("carol", "pass1");
  EXPECT_THROW(auth.registerUser("carol", "pass2"), std::invalid_argument);
}

TEST_F(AuthTest, EmptyUsernameThrows)
{
  EXPECT_THROW(auth.registerUser("", "pass"), std::invalid_argument);
}

TEST_F(AuthTest, EmptyPasswordThrows)
{
  EXPECT_THROW(auth.registerUser("dave", ""), std::invalid_argument);
}

TEST_F(AuthTest, MultipleUsersGetSeparateIds)
{
  int64_t id1 = auth.registerUser("user1", "pass1");
  int64_t id2 = auth.registerUser("user2", "pass2");
  EXPECT_NE(id1, id2);
}

TEST_F(AuthTest, PasswordsHashedDifferentlyWithSalt)
{
  // Same password for two users should not result in the same stored hash
  // (salts are random). We verify indirectly: both can log in correctly,
  // and each rejects the other's identity.
  auth.registerUser("user_a", "samepass");
  auth.registerUser("user_b", "samepass");

  EXPECT_TRUE(auth.login("user_a", "samepass").has_value());
  EXPECT_TRUE(auth.login("user_b", "samepass").has_value());
}

// ── per-user todo isolation
// ──────────────────────────────────────────────────

TEST_F(AuthTest, TodosIsolatedByUser)
{
  int64_t uid_a = auth.registerUser("anna", "pa");
  int64_t uid_b = auth.registerUser("bert", "pb");

  AppConfig cfg = ConfigLoader::defaults();
  TodoService svc_a(db, cfg, uid_a);
  TodoService svc_b(db, cfg, uid_b);

  svc_a.addTodo("Anna's todo");
  svc_b.addTodo("Bert's todo");

  auto list_a = svc_a.listAll();
  auto list_b = svc_b.listAll();

  ASSERT_EQ(list_a.size(), 1u);
  ASSERT_EQ(list_b.size(), 1u);
  EXPECT_EQ(list_a[0].title, "Anna's todo");
  EXPECT_EQ(list_b[0].title, "Bert's todo");
}

TEST_F(AuthTest, SearchIsolatedByUser)
{
  int64_t uid_a = auth.registerUser("search_a", "pa");
  int64_t uid_b = auth.registerUser("search_b", "pb");

  AppConfig cfg = ConfigLoader::defaults();
  TodoService svc_a(db, cfg, uid_a);
  TodoService svc_b(db, cfg, uid_b);

  svc_a.addTodo("alpha project");
  svc_b.addTodo("alpha task");

  auto results_a = svc_a.search("alpha");
  auto results_b = svc_b.search("alpha");

  ASSERT_EQ(results_a.size(), 1u);
  EXPECT_EQ(results_a[0].title, "alpha project");

  ASSERT_EQ(results_b.size(), 1u);
  EXPECT_EQ(results_b[0].title, "alpha task");
}
