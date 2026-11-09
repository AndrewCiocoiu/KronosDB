#include <gtest/gtest.h>
#include "engine.hpp"
#include "threadpool.hpp"
#include <filesystem>
#include <vector>

class KronosTest : public ::testing::Test {
protected:
    void SetUp() override {
        std::filesystem::remove_all("logs");
    }
    void TearDown() override {
        std::filesystem::remove_all("logs");
    }
};

TEST_F(KronosTest, BasicSetGetDel) {
    Engine db("TestDB", 4, false);
    db.set("key1", "value1");

    auto val = db.get("key1");
    ASSERT_TRUE(val.has_value());
    EXPECT_EQ(val.value(), "value1");

    db.del("key1");
    EXPECT_FALSE(db.get("key1").has_value());
}

TEST_F(KronosTest, WALCrashRecoveryAndMultiline) {
    {
        Engine db("RecoveryDB", 4, true);
        db.set("user:42", "Andrei Ciocoiu\nRole: Systems Engineer");
        db.set("temp", "to_be_deleted");
        db.del("temp");
    } // db goes out of scope and flushes WAL

    // We rebot new engine instance over the same logs
    Engine db_recovered("RecoveryDB", 4, true);
    auto user = db_recovered.get("user:42");
    ASSERT_TRUE(user.has_value());
    EXPECT_EQ(user.value(), "Andrei Ciocoiu\nRole: Systems Engineer");
    EXPECT_FALSE(db_recovered.get("temp").has_value());
}

TEST_F(KronosTest, ConcurrentWritesAndCheckpoints) {
    Engine db("ConcurrentDB", 16, true);
    Threadpool pool(8);
    std::vector<std::future<void>> futures;

    for (int i = 0; i < 1000; ++i) {
        futures.push_back(pool.submit([&db, i]() {
            db.set("k_" + std::to_string(i), "v_" + std::to_string(i));
        }));
    }
    for (auto& f : futures) f.get();

    EXPECT_EQ(db.size(), 1000);
    db.create_checkpoint();

    // Recover from checkpoint
    Engine db_restored("ConcurrentDB", 16, true);
    EXPECT_EQ(db_restored.size(), 1000);
    EXPECT_EQ(db_restored.get("k_500").value_or(""), "v_500");
}