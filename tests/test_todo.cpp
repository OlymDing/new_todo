#include <gtest/gtest.h>
#include "../src/todo.h"

TEST(Todo, DefaultConstruction)
{
  Todo t;
  EXPECT_EQ(t.id, 0);
  EXPECT_EQ(t.parent_id, 0);
  EXPECT_TRUE(t.title.empty());
  EXPECT_TRUE(t.status.empty());
  EXPECT_TRUE(t.ext_info.empty());
  EXPECT_EQ(t.create_time, 0);
  EXPECT_EQ(t.update_time, 0);
}

TEST(Todo, IsRootLevelTrueWhenParentIdZero)
{
  Todo t;
  t.parent_id = 0;
  EXPECT_TRUE(t.isRootLevel());
}

TEST(Todo, IsRootLevelFalseWhenParentIdNonZero)
{
  Todo t;
  t.parent_id = 42;
  EXPECT_FALSE(t.isRootLevel());
}

TEST(Todo, NowTimestampIsPositive)
{
  Timestamp ts = now_timestamp();
  EXPECT_GT(ts, 0);
}

TEST(Todo, NowTimestampIsRecentUnixTime)
{
  Timestamp ts = now_timestamp();
  // After 2020-01-01 00:00:00 UTC = 1577836800
  EXPECT_GT(ts, 1577836800LL);
}

TEST(Todo, NowTimestampIsMonotonic)
{
  Timestamp t1 = now_timestamp();
  Timestamp t2 = now_timestamp();
  EXPECT_GE(t2, t1);
}
