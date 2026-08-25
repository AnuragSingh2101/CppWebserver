#include "user_store.h"

#include <vector>
#include <mutex>
#include <optional>
#include <algorithm>
#include <cctype>

namespace {
    bool iequals(const std::string& a, const std::string& b) {
        if (a.size() != b.size()) return false;
        for (size_t i = 0; i < a.size(); ++i) {
            if (std::tolower(static_cast<unsigned char>(a[i])) != std::tolower(static_cast<unsigned char>(b[i]))) {
                return false;
            }
        }
        return true;
    }
}

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
std::pair<StoreResult, std::optional<User>> UserStore::addUser(
    const std::string& name,
    const std::string& email)
{
    std::lock_guard<std::mutex> lock(storeMutex);
    
    // Uniqueness check
    for (const User& user : users) {
        if (iequals(user.email, email)) {
            return {StoreResult::DUPLICATE_EMAIL, std::nullopt};
        }
    }

    User newUser = {
        nextId,
        name,
        email
    };
    users.push_back(newUser);
    nextId++;
    return {StoreResult::SUCCESS, newUser};
}

// Update user (PUT)
std::pair<StoreResult, std::optional<User>> UserStore::updateUser(
    int id,
    const std::string& name,
    const std::string& email)
{
    std::lock_guard<std::mutex> lock(storeMutex);
    
    bool found = false;
    for (const User& user : users) {
        if (user.id == id) {
            found = true;
            break;
        }
    }
    if (!found) {
        return {StoreResult::NOT_FOUND, std::nullopt};
    }

    // Uniqueness check
    for (const User& user : users) {
        if (user.id != id && iequals(user.email, email)) {
            return {StoreResult::DUPLICATE_EMAIL, std::nullopt};
        }
    }

    for (User& currentUser : users) {
        if (currentUser.id == id) {
            currentUser.name = name;
            currentUser.email = email;
            return {StoreResult::SUCCESS, currentUser};
        }
    }
    return {StoreResult::NOT_FOUND, std::nullopt};
}

// Patch user (PATCH)
std::pair<StoreResult, std::optional<User>> UserStore::patchUser(
    int id,
    const std::string& name,
    const std::string& email)
{
    std::lock_guard<std::mutex> lock(storeMutex);

    bool found = false;
    User* targetUser = nullptr;
    for (User& user : users) {
        if (user.id == id) {
            found = true;
            targetUser = &user;
            break;
        }
    }
    if (!found) {
        return {StoreResult::NOT_FOUND, std::nullopt};
    }

    // Uniqueness check if email is updated
    if (!email.empty()) {
        for (const User& user : users) {
            if (user.id != id && iequals(user.email, email)) {
                return {StoreResult::DUPLICATE_EMAIL, std::nullopt};
            }
        }
    }

    if (!name.empty()) {
        targetUser->name = name;
    }
    if (!email.empty()) {
        targetUser->email = email;
    }
    return {StoreResult::SUCCESS, *targetUser};
}

void UserStore::clear() {
    std::lock_guard<std::mutex> lock(storeMutex);
    users.clear();
    nextId = 1;
}