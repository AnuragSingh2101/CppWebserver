#ifndef USER_STORE_H
#define USER_STORE_H

#include <string>
#include <vector>

struct User
{
    int id;
    std::string name;
    std::string email;
};

class UserStore
{
public:
    // Get all users
    static std::vector<User> getAllUsers();

    // Get one user by ID
    static User* getUserById(int id);

    // Remove a user
    static bool removeUser(int id);

    // Add a new user
    static User addUser(
        const std::string& name,
        const std::string& email
    );

    // Update user (PUT)
    static User* updateUser(
        int id,
        const std::string& name,
        const std::string& email
    );

    // Patch user (PATCH)
    static User* patchUser(
        int id,
        const std::string& name,
        const std::string& email
    );

private:
    static std::vector<User> users;
    static int nextId;
};

#endif