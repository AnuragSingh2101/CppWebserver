#include "user_store.h"

#include <vector>
#include <mutex>
#include <optional>

// Initial users
std::vector<User> UserStore::users =
{
    {1, "Anurag", "anurag@example.com"},
    {2, "Rahul", "rahul@example.com"}
};

// ID for the next newly created user
int UserStore::nextId = 3;

// Define static mutex
std::mutex UserStore::storeMutex;


// Get all users
std::vector<User> UserStore::getAllUsers(){
    std::lock_guard<std::mutex> lock(storeMutex);
    return users;
}


// Get user by ID
std::optional<User> UserStore::getUserById(int id){
    std::lock_guard<std::mutex> lock(storeMutex);
    for (const User& currentUser : users){
        if (currentUser.id == id){
            return currentUser;
        }
    }

    return std::nullopt;
}


// Remove a user
bool UserStore::removeUser(int id){
    std::lock_guard<std::mutex> lock(storeMutex);
    for (auto it = users.begin(); it != users.end(); ++it){
        if (it->id == id){
            users.erase(it);
            return true;
        }
    }

    return false;
}


// Add a new user
User UserStore::addUser(
    const std::string& name,
    const std::string& email){
    std::lock_guard<std::mutex> lock(storeMutex);
    User newUser ={
        nextId,
        name,
        email
    };
    users.push_back(newUser);
    nextId++;
    return newUser;
}

// Update user (PUT)
std::optional<User> UserStore::updateUser(
    int id,
    const std::string& name,
    const std::string& email)
{
    std::lock_guard<std::mutex> lock(storeMutex);
    for (User& currentUser : users) {
        if (currentUser.id == id) {
            currentUser.name = name;
            currentUser.email = email;
            return currentUser;
        }
    }
    return std::nullopt;
}

// Patch user (PATCH)
std::optional<User> UserStore::patchUser(
    int id,
    const std::string& name,
    const std::string& email)
{
    std::lock_guard<std::mutex> lock(storeMutex);
    for (User& currentUser : users) {
        if (currentUser.id == id) {
            if (!name.empty()) {
                currentUser.name = name;
            }
            if (!email.empty()) {
                currentUser.email = email;
            }
            return currentUser;
        }
    }
    return std::nullopt;
}