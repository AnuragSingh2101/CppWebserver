#ifndef USER_STORE_H
#define USER_STORE_H

#include <string>
#include <vector>
#include <mutex>
#include <optional>

struct User
{
    int id;
    std::string name;
    std::string email;
};

enum class StoreResult {
    SUCCESS,
    NOT_FOUND,
    DUPLICATE_EMAIL
};

class UserStore
{
public:
    // Get all users
    static std::vector<User> getAllUsers();

    // Get one user by ID
    static std::optional<User> getUserById(int id);

    // Remove a user
    static bool removeUser(int id);

    // Add a new user
    static std::pair<StoreResult, std::optional<User>> addUser(
        const std::string& name,
        const std::string& email
    );

    // Update user (PUT)
    static std::pair<StoreResult, std::optional<User>> updateUser(
        int id,
        const std::string& name,
        const std::string& email
    );

    // Patch user (PATCH)
    static std::pair<StoreResult, std::optional<User>> patchUser(
        int id,
        const std::string& name,
        const std::string& email
    );

    // Reset/clear user store (useful for tests)
    static void clear();

private:
    static std::vector<User> users;
    static int nextId;
    static std::mutex storeMutex;
};

#endif