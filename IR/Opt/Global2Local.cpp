/*
#include "../../include/IR/Opt/Global2Local.hpp"

bool Global2Local::run() {
    createSuccFuncs();
    createCallNum();
    detectRecursive();

    bool modified = false;

    for (auto &gvPtr : module->GetGlobalVariable()) {
        Var* GV = gvPtr.get();
        if (!GV || GV->usage != Var::GlobalVar) continue;

        if (addressEscapes(GV))
            continue; // 地址逃逸的不优化

        std::set<Function*> usingFuncs;
        bool inLoop = false;

        for (auto &funcPtr : module->GetFuncTion()) {
            Function* F = funcPtr.get();
            if (!F || F->GetBBs().empty()) continue;

            if (RecursiveFuncs.count(F)) 
                continue; // 递归函数跳过

            bool usesGV = false;
            int storeCount = 0;

            for (auto &bb_sp : F->GetBBs()) {
                BasicBlock* bb = bb_sp.get();
                if (!bb) continue;

                if (bb->LoopDepth > 0)
                    inLoop = true; // 循环中使用

                for (auto* inst : *bb) {
                    auto &uList = inst->GetUserUseList();
                    for (auto &u_sp : uList) {
                        Use* u = u_sp.get();
                        if (!u) continue;
                        if (u->GetValue() != GV) continue;

                        usesGV = true;
                        if (dynamic_cast<StoreInst*>(inst))
                            storeCount++;
                    }
                }
            }

            if (inLoop) break; // 循环中使用，不优化
            if (usesGV)
                usingFuncs.insert(F);
            // 如果函数内有多次写入，不优化
            if (storeCount > 1) 
                usingFuncs.clear();
        }

        // 多函数使用或循环中使用或多次写入的不优化
        if (inLoop || usingFuncs.size() != 1) continue;

        // 安全提升
        Function* targetFunc = *usingFuncs.begin();
        modified |= promoteGlobal(GV, targetFunc);
    }

    return modified;
}


void Global2Local::createSuccFuncs() {
    for (auto &funcPtr : module->GetFuncTion()) {
        Function *F = funcPtr.get();

        for (auto &BBptr : F->GetBBs()) {        // BBptr 是 shared_ptr<BasicBlock>
            BasicBlock *BB = BBptr.get();        // 获取裸指针
            for (auto *inst : *BB) {             // 利用 List 提供的迭代器
                if (auto *call = dynamic_cast<CallInst *>(inst)) {
                    Value *calledVal = call->GetCalledFunction();
                    for (auto &calleePtr : module->GetFuncTion()) {
                        if (calleePtr.get() == calledVal) {
                            FuncSucc[F].insert(calleePtr.get());
                        }
                    }
                }
            }
        }
    }
}


void Global2Local::createCallNum() {
    // 先把所有函数调用次数初始化为 0
    for (auto &funcPtr : module->GetFuncTion()) {
        CallNum[funcPtr.get()] = 0;
    }

    // 遍历每个函数的基本块和指令，统计调用次数
    for (auto &funcPtr : module->GetFuncTion()) {
        Function *F = funcPtr.get();

        for (auto &BBptr : F->GetBBs()) {
            BasicBlock *BB = BBptr.get();
            for (auto *inst : *BB) {  // 利用 List 提供的迭代器
                if (auto *call = dynamic_cast<CallInst *>(inst)) {
                    Value *calledVal = call->GetCalledFunction();
                    for (auto &calleePtr : module->GetFuncTion()) {
                        if (calleePtr.get() == calledVal) {
                            CallNum[calleePtr.get()]++;
                        }
                    }
                }
            }
        }
    }
}

void Global2Local::detectRecursive() {
    // 用于 DFS 访问状态：0 = 未访问, 1 = 访问中, 2 = 已完成
    std::unordered_map<Function*, int> visitStatus;

    // 初始化
    for (auto &funcPtr : module->GetFuncTion()) {
        visitStatus[funcPtr.get()] = 0;
    }

    // 定义 DFS lambda
    std::function<void(Function*)> dfs = [&](Function* F) {
        if (visitStatus[F] == 1) {
            // 正在访问中，又遇到自己 -> 递归
            RecursiveFuncs.insert(F);
            return;
        }
        if (visitStatus[F] == 2)
            return; // 已经处理过

        visitStatus[F] = 1; // 标记为访问中

        // 遍历 F 的后继函数
        for (auto *succ : FuncSucc[F]) {
            dfs(succ);
            // 如果后继是递归函数，也把 F 标记为递归
            if (RecursiveFuncs.count(succ))
                RecursiveFuncs.insert(F);
        }

        visitStatus[F] = 2; // 标记为完成
    };

    // 对每个函数执行 DFS
    for (auto &funcPtr : module->GetFuncTion()) {
        dfs(funcPtr.get());
    }
}

bool Global2Local::addressEscapes(Value *V) {
    for (auto use : V->GetValUseList()) {
        User *user = use->GetUser();
        if (!user) continue;

        if (auto *call = dynamic_cast<CallInst *>(user)) {
            // 被函数调用使用 -> 地址逃逸
            return true;
        } else if (auto *store = dynamic_cast<StoreInst *>(user)) {
            int idx = user->GetUseIndex(use); // 0=val, 1=ptr
            if (idx == 1) {
                // ptr 是 GV 自己或者经过简单转换才安全
                Value* ptrVal = use->GetValue();
                if (ptrVal != user) {
                    // 进一步判断是否是简单 bitcast 或 GEP 指向自己
                    if (!isSimplePtrToSelf(ptrVal, V)) {
                        return true; // 真正逃逸
                    }
                }
            }
        } else if (dynamic_cast<PhiInst *>(user)) {
            return true;
        } else if (auto *gep = dynamic_cast<GepInst *>(user)) {
            return true;
        }
    }
    return false;
}

// 辅助函数：判断 ptr 是否是“简单转换指向自己”，安全不算逃逸
bool Global2Local::isSimplePtrToSelf(Value *ptr, Value *V) {
    if (ptr == V) return true;
    if (auto *gep = dynamic_cast<GepInst *>(ptr)) {
        return gep->GetOperand(0) == V;
    }
    return false;
}


bool Global2Local::promoteGlobal(Var* GV, Function* F) {
    if (!GV || GV->usage != Var::GlobalVar) return false;
    if (!F || F->GetBBs().empty()) return false;

    // 检查全局变量是否在循环中被使用
    for (auto &bb_sp : F->GetBBs()) {
        BasicBlock* bb = bb_sp.get();
        if (!bb) continue;
        if (bb->LoopDepth > 0) {
            for (auto* inst : *bb) {
                auto &uList = inst->GetUserUseList();
                for (auto& u_sp : uList) {
                    Use* u = u_sp.get();
                    if (u && u->GetValue() == GV) {
                        return false; // 循环中使用，不优化
                    }
                }
            }
        }
    }
}
*/
