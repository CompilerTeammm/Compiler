#include "../../include/IR/Opt/SOGE.hpp"

bool StoreOnlyGlobalElim::run() {
    auto& globalVars = module->GetGlobalVariable();

    scanStoreOnlyGlobals();

    while (!storeOnlyGlobals.empty()) {
        auto* var = storeOnlyGlobals.back();
        storeOnlyGlobals.pop_back();

        // 删除所有使用该全局变量的指令
        for (auto* use : var->GetValUseList()) {
            if (auto* inst = dynamic_cast<Instruction*>(use->GetUser())) {
                delete inst;
            }
        }

        // 从全局变量列表中删除该变量
        globalVars.erase(
            std::remove_if(globalVars.begin(), globalVars.end(),
                           [var](const std::unique_ptr<Var>& ptr) { return ptr.get() == var; }),
            globalVars.end()
        );
    }

    return true;
}

bool isMemcpyCall(User* valUser) {
    auto* call = dynamic_cast<CallInst*>(valUser);
    if (!call) return false;

    const auto& uses = call->GetUserUseList();
    if (!uses.empty() && uses[0]->usee && uses[0]->usee->GetName() == "llvm.memcpy.p0.p0.i32") {
        return true;
    }
    return false;
}

bool isStoreOnlyUsage(User* varUser) {
    for (auto* use : varUser->GetValUseList()) {
        auto* inst = use->GetUser();
        if (!inst) continue;

        // 如果是 store 或 memcpy 调用，则继续
        if (dynamic_cast<StoreInst*>(inst) || isMemcpyCall(inst)) continue;

        // 遇到其他类型使用，返回 false
        return false;
    }
    return true;
}

void StoreOnlyGlobalElim::scanStoreOnlyGlobals() {
    for (auto& varPtr : module->GetGlobalVariable()) {
        Var* varObj = varPtr.get();

        // 跳过参数或并行变量
        if (varObj->usage == Var::UsageTag::Param || varObj->ForParallel)
            continue;

        // 处理常量全局变量
        if (varObj->usage == Var::UsageTag::Constant) {
            CallInst* memcpyInst = nullptr;

            // 查找 memcpy 调用
            for (Use* use : varObj->GetValUseList()) {
                if (isMemcpyCall(use->GetUser())) {
                    memcpyInst = dynamic_cast<CallInst*>(use->GetUser());
                    break;
                }
            }

            if (memcpyInst) {
                if (memcpyInst->GetUserUseList().size() < 2)
                    continue;

                auto* allocaInst = dynamic_cast<AllocaInst*>(memcpyInst->GetUserUseList()[1]->usee);
                if (!allocaInst) 
                    continue;

                if (isStoreOnlyUsage(allocaInst))
                    storeOnlyGlobals.push_back(varObj);

                continue;
            }
        }

        // 空使用列表的全局变量直接加入
        if (varObj->GetValUseList().is_empty()) {
            storeOnlyGlobals.push_back(varObj);
            continue;
        }

        // 处理数组类型子类型
        if (auto* subtype = dynamic_cast<HasSubType*>(varObj->GetType())) {
            if (subtype->GetSubType()->GetTypeEnum() == IR_DataType::IR_ARRAY) {
                bool allStoreOnly = true;

                for (Use* use : varObj->GetValUseList()) {
                    auto* gep = dynamic_cast<GepInst*>(use->GetUser());
                    if (!gep || !isStoreOnlyUsage(gep)) {
                        allStoreOnly = false;
                        break;
                    }
                }

                if (allStoreOnly)
                    storeOnlyGlobals.push_back(varObj);

                continue;
            }
        }

        // 默认检查是否仅 store 使用
        if (isStoreOnlyUsage(varObj))
            storeOnlyGlobals.push_back(varObj);
    }
}