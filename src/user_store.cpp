#include "user_store.h"

#include <vector>

// Initial users
std::vector<User> UserStore::users =
{
    {1, "Anurag", "anurag@example.com"},
    {2, "Rahul", "rahul@example.com"}
};

// ID for the next newly created user
int UserStore::nextId = 3;


// Get all users
std::vector<User> UserStore::getAllUsers(){
    return users;
}


// Get user by ID
User* UserStore::getUserById(int id){
    for (User& currentUser : users){
        if (currentUser.id == id){
            return &currentUser;
        }
    }

    return nullptr;
}


// Remove a user
bool UserStore::removeUser(int id){
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
    User newUser ={
        nextId,
        name,
        email
    };
    users.push_back(newUser);
    nextId++;
    return newUser;
}