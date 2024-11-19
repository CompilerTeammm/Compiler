#include"Type.h"
#include <string>
#include <iostream>
#include<stdbool.h>
namespace llvm
{
    class Value;
    class Use;
    class User;
    class Instruction;  
}

class Value
{
public:
    std::string GetName() {return name;};
    void SetName(std::string name)  { this->name = name;  }

    void RAUW();
private:
    Use* UserList;
protected:
    std::string name;
    Type type;
};

class UseList
{

};

class UserList
{

};

class Use
{
public:
    Use(const Use &U)=delete;


private:
    User* user = nullptr;
    Value* usee = nullptr;
    Use* next = nullptr;
    Use** prev = nullptr;

    ~Use() { if(usee) { removeFrontList(); } }
    Use(User *Parent) 
        : user(user)
        {}

    void addToList(Use** List)
    {
        next = *List;
        if(next)
            next->prev = &next;
        prev = List;
        *prev = this;
    }

    void removeFrontList()
    {
        *prev = next;
        if(next)
            next->prev = prev;
    }
};

class User : Value
{

};

class Instruction : User
{
    public:
    enum OpID
    {

    };
    OpID id;
};