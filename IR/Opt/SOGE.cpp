#include "../../include/IR/Opt/SOGE.hpp"

bool SOGE::run()
{
    auto &globals = module->GetGlobalVariable();

    ScanStoreOnlyGlobal();

    while (!StoreOnlyGlobal.empty())
    {
        Var *global = StoreOnlyGlobal.back();
        StoreOnlyGlobal.pop_back();

        for (Use *user : global->GetValUseList())
        {
            if (auto inst = dynamic_cast<Instruction *>(user->GetUser()))
            {
                delete inst;
            }
        }

        globals.erase(std::remove_if(globals.begin(), globals.end(),
                                     [global](const std::unique_ptr<Var> &ptr) { return ptr.get() == global; }),
                      globals.end());
    }
    return true;
}


bool IsMemcpyCall(User* user) {
    if (auto call = dynamic_cast<CallInst*>(user)) {
        auto &userUseList = call->GetUserUseList();
        if (!userUseList.empty()) {
            if (userUseList[0]->usee->GetName() == "llvm.memcpy.p0.p0.i32")
                return true;
        }
    }
    return false;
}

bool IsStoreOnlyUsage(User* var) {
    for (Use* user : var->GetValUseList()) {
        User* u = user->GetUser();
        if (dynamic_cast<StoreInst*>(u))
            continue;

        if (IsMemcpyCall(u))
            continue;

        return false;
    }
    return true;
}

void SOGE::ScanStoreOnlyGlobal() {
    for (auto &ptr : module->GetGlobalVariable()) {
        Var* var = ptr.get();

        if (var->usage == Var::UsageTag::Param || var->ForParallel)
            continue;

        if (var->usage == Var::UsageTag::Constant) {
            CallInst* memcpyCall = nullptr;

            for (Use* user : var->GetValUseList()) {
                if (IsMemcpyCall(user->GetUser())) {
                    memcpyCall = dynamic_cast<CallInst*>(user->GetUser());
                    break;
                }
            }

            if (memcpyCall) {
                if (memcpyCall->GetUserUseList().size() < 2)
                    continue;

                auto alloca = dynamic_cast<AllocaInst*>(memcpyCall->GetUserUseList()[1]->usee);
                if (!alloca)
                    continue;

                if (IsStoreOnlyUsage(alloca)) {
                    StoreOnlyGlobal.push_back(var);
                }
            }
        } else {

            if (var->GetValUseList().is_empty()) {
                StoreOnlyGlobal.push_back(var);
                continue;
            }

            if (auto subtype = dynamic_cast<HasSubType*>(var->GetType())) {
                if (subtype->GetSubType()->GetTypeEnum() == IR_DataType::IR_ARRAY) {
                    bool isStoreOnly = true;

                    for (Use* user : var->GetValUseList()) {
                        auto gep = dynamic_cast<GepInst*>(user->GetUser());
                        if (!gep) {
                            isStoreOnly = false;
                            break;
                        }

                        if (!IsStoreOnlyUsage(gep)) {
                            isStoreOnly = false;
                            break;
                        }
                    }

                    if (isStoreOnly)
                        StoreOnlyGlobal.push_back(var);

                    continue;
                }
            }

            if (IsStoreOnlyUsage(var)) {
                StoreOnlyGlobal.push_back(var);
            }
        }
    }
}
