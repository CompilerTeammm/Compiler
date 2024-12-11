#include"Type.hpp"
#include <string>
#include <iostream>
#include<stdbool.h>
#include "Type.hpp"
// namespace llvm
// {
    class Value;
    class Use;
    class User;
    class Instruction;  
    class UserList;
// }

class Value
{
public:
    std::string GetName() {return name;};
    void SetName(std::string name)  { this->name = name;  }

    void RAUW();
    void Print();
    void GetUserList();
private:
    UserList* UserList;
protected:
    std::string name;
    Type type;

    Value(Type ty,std::string name)
    :type(ty)
    ,name(name)
    {}

    ~Value();
};
// 本来应该在 value类中去实现 use_iterator use_begin()
// 但是我这里选择学习去年学长的  实现一个  class  UserList的类
// 一个辅助类  辅助遍历的  这里不放在value的内部类之中了
class UserList
{
private:
    Use* head;
    int Opnums = 0;
public:
    // 迭代器的实现
    class iterator
    {

    };

    iterator begin();
    iterator end();
    bool is_Empty();
    int GetOpnums() { return Opnums; }
};

class Use
{
public:
    Use(const Use &U)=delete;
    User* GetUser()  { return user;}
    Value* GetValue()  {return usee;}

    Use* GetNext() {return next;}
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
protected:
    void* operator new(size_t Size);
public: 
    User(Type tp,std::string name)
    :Value(tp,name)
    {}

    bool addUse();
    bool removeUse();
    int getNumOperands();
    ~User() = default;
    User(const User &) = delete;

private:

};

class Instruction : User
{
private:
    // 目前是从学长那里直接拿的
    enum OpID
    {
        None,
        // Terminators
        UnCond,
        Cond,
        Ret,
        // Memory
        Alloca,
        Load,
        Store,
        Memcpy,
        // Binary
        Add,
        Sub,
        Mul,
        Div,
        Mod,
        And,
        Or,
        Xor,
        Eq,
        Ne,
        Ge,
        L,
        Le,
        G,
        // Other
        Gep,
        Phi,
        Call,
        Zext,
        Sext,
        Trunc,
        FP2SI,
        SI2FP,
        BinaryUnknown,
        Max,
        Min,
        Select
    };
    OpID id;
public:
    OpID GetOpid()  { return id;}

    void removeFromParent();

};