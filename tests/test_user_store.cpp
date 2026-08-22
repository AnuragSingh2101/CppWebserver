#include <gtest/gtest.h>
#include "../src/user_store.h"
#include <thread>
#include <vector>

TEST(UserStoreTest, UniqueEmailAddition) {
    // Clear and prepare test environment is not needed, UserStore has initial users: 1: Anurag, 2: Rahul
    
    // Add unique email
    auto res1 = UserStore::addUser("TestUser", "unique@test.com");
    EXPECT_EQ(res1.first, StoreResult::SUCCESS);
    EXPECT_TRUE(res1.second.has_value());
    int createdId = res1.second.value().id;

    // Add duplicate email (case-insensitive check)
    auto res2 = UserStore::addUser("AnotherUser", "UNIQUE@TEST.COM");
    EXPECT_EQ(res2.first, StoreResult::DUPLICATE_EMAIL);
    EXPECT_FALSE(res2.second.has_value());

    // Cleanup
    UserStore::removeUser(createdId);
}

TEST(UserStoreTest, UpdateUniqueness) {
    auto res1 = UserStore::addUser("UserA", "usera@test.com");
    auto res2 = UserStore::addUser("UserB", "userb@test.com");
    
    EXPECT_EQ(res1.first, StoreResult::SUCCESS);
    EXPECT_EQ(res2.first, StoreResult::SUCCESS);
    
    int idA = res1.second.value().id;
    int idB = res2.second.value().id;

    // Update User B to User A's email -> conflict
    auto updateRes = UserStore::updateUser(idB, "UserB", "usera@test.com");
    EXPECT_EQ(updateRes.first, StoreResult::DUPLICATE_EMAIL);

    // Update User B to same email -> success
    updateRes = UserStore::updateUser(idB, "UserB_New", "userb@test.com");
    EXPECT_EQ(updateRes.first, StoreResult::SUCCESS);

    // Cleanup
    UserStore::removeUser(idA);
    UserStore::removeUser(idB);
}

TEST(UserStoreTest, ConcurrencySafe) {
    std::vector<std::thread> threads;
    // Launch threads adding users concurrently
    for (int i = 0; i < 20; ++i) {
        threads.push_back(std::thread([i]() {
            UserStore::addUser("ConUser" + std::to_string(i), "con" + std::to_string(i) + "@test.com");
        }));
    }

    for (auto& t : threads) {
        t.join();
    }

    // Clean up con users
    auto all = UserStore::getAllUsers();
    for (const auto& u : all) {
        if (u.name.find("ConUser") == 0) {
            UserStore::removeUser(u.id);
        }
    }
}
