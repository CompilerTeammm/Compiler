#include"../include/lib/CoreClass.hpp"

Use::Use(User *_user, Value *_usee) : user(_user), usee(_usee)
{
    usee->add_use(this);
}

void Use::RemoveFromValUseList(User *_user)
{
    assert(_user == user); // 必须是User
    usee->GetValUseList().GetSize()--;
    if (*prev != nullptr)
        *prev = next;
    if (next != nullptr)
        next->prev = prev;
    if (usee->GetValUseList().GetSize() == 0 && next != nullptr)
        assert(0);
    user = nullptr;
    usee = nullptr;
    prev = nullptr;
    next = nullptr;
}