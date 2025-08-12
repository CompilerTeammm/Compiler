#include "../../include/IR/Opt/DAE.hpp"
#include "../../include/IR/Analysis/SideEffect.hpp"
bool DAE::run(){
    AM.get<SideEffect>(&Singleton<Module>());
    wait_del.clear();
    bool modified = false;
    bool changed = true;
    while (changed)
    {
        changed = false;
        for (auto &func_Ptr : mod->GetFuncTion())
        {
            Handle_SameArgs(func_Ptr.get());
        }
        for (auto &func_Ptr : mod->GetFuncTion())
        {
            changed |= DetectDeadargs(func_Ptr.get());
        }
        modified |= changed;
    }

    return modified;
}
void DAE::NormalHandle(Function *func)
{
    for (auto it = func->GetParams().begin(); it != func->GetParams().end(); it++)
    {
        if ((*it)->GetValUseList().is_empty())
            wait_del.push_back(std::make_pair((*it).get(), std::distance(func->GetParams().begin(), it)));
        else
        {
            bool flag = true;
            for (Use *User_ : (*it)->GetValUseList())
            {
                if (auto store = dynamic_cast<StoreInst *>(User_->GetUser()))
                {
                    if ((*it).get() == store->GetOperand(0))
                    {
                        flag = false;
                        break;
                    }
                }
                else
                {
                    flag = false;
                    break;
                }
            }
            if (flag)
                wait_del.push_back(std::make_pair((*it).get(), std::distance(func->GetParams().begin(), it)));
        }
    }
}
void DAE::Handle_SameArgs(Function *func)
{
    if (func->GetParams().size() > 0 && !func->GetValUseList().is_empty())
    {
        bool result = true;
        for (Use *call_ : func->GetValUseList())
        {

            if (auto call = dynamic_cast<CallInst *>(call_->GetUser()))
            {
                auto &uses = call->GetUserUseList();
                auto all_except_first_are_same = [&uses]() {
                    if (uses.size() < 3)
                        return true;
                    return std::all_of(uses.begin() + 2, uses.end(),
                                       [&uses](User::UsePtr &use) { return uses[1]->usee == use->usee; });
                };
                result &= all_except_first_are_same();
            }
            else
                result &= false;
        }
        if (result)
        {
            Value *val = func->GetParams().front().get();
            for (auto it = func->GetParams().begin(); it != func->GetParams().end(); it++)
            {
                if ((*it).get() == val)
                    continue;
                (*it)->ReplaceAllUseWith(val);
            }
        }
    }
}

bool DAE::DetectDeadargs(Function *func)
{
    bool modified = false;
    NormalHandle(func);

    while (!wait_del.empty())
    {
        auto &pair = wait_del.back();
        wait_del.pop_back();

        for (Use *User_ : func->GetValUseList())
        {
            if (auto call = dynamic_cast<CallInst *>(User_->GetUser()))
            {
                call->GetUserUseList().erase(call->GetUserUseList().begin() + pair.second + 1);
            }
        }
        func->GetParams().erase(func->GetParams().begin() + pair.second);
        modified = true;
    }

    return modified;
}