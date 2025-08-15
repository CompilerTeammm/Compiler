#include "../../include/IR/Opt/Global2Local.hpp"

// bool Global2Local::run() {
//     createSuccFuncs();
//     createCallNum();
//     detectRecursive();
    

//     bool modified = false;

//     for (auto &gvPtr : module->GetGlobalVariable()) {
//         Var* GV = gvPtr.get();
//         if (!GV || GV->usage != Var::GlobalVar) continue;

//         if (!addressEscapes(GV)) {
//             safeGlobals.insert(GV);

//             // 遍历模块函数，找使用 GV 的函数
//             for (auto &funcPtr : module->GetFuncTion()) {
//                 Function* F = funcPtr.get();
//                 if (!F || F->GetBBs().empty()) continue;

//                 bool usesGV = false;
//                 for (auto &bb_sp : F->GetBBs()) {
//                     BasicBlock* bb = bb_sp.get();
//                     if (!bb) continue;

//                     for (auto* inst : *bb) {
//                         auto &uList = inst->GetUserUseList();
//                         for (auto& u_sp : uList) {
//                             Use* u = u_sp.get();
//                             if (!u) continue;
//                             if (u->GetValue() == GV) {
//                                 usesGV = true;
//                                 break;
//                             }
//                         }
//                         if (usesGV) break;
//                     }
//                     if (usesGV) break;
//                 }

//                 if (usesGV) {
//                     modified |= promoteGlobal(GV, F);
//                 }
//             }
//         }
//     }

//     return modified;
// }
bool Global2Local::run() {
    createSuccFuncs();
    createCallNum();
    detectRecursive();

    bool modified = false;

    for (auto &gvPtr : module->GetGlobalVariable()) {
        Var* GV = gvPtr.get();
        if (!GV || GV->usage != Var::GlobalVar) continue;

        if (addressEscapes(GV))
            continue; // 地址逃逸的不做优化

        // 遍历所有函数，记录使用GV的函数
        std::set<Function*> usingFuncs;
        bool inLoop = false;

        for (auto &funcPtr : module->GetFuncTion()) {
            Function* F = funcPtr.get();
            if (!F || F->GetBBs().empty()) continue;

            bool usesGV = false;

            for (auto &bb_sp : F->GetBBs()) {
                BasicBlock* bb = bb_sp.get();
                if (!bb) continue;

                if (bb->LoopDepth > 0) {
                    // 该BB在循环中，涉及循环的全局变量直接不优化
                    for (auto* inst : *bb) {
                        auto &uList = inst->GetUserUseList();
                        for (auto &u_sp : uList) {
                            Use* u = u_sp.get();
                            if (u && u->GetValue() == GV) {
                                inLoop = true;
                                break;
                            }
                        }
                        if (inLoop) break;
                    }
                }
                if (inLoop) break;

                // 普通使用检查
                for (auto* inst : *bb) {
                    auto &uList = inst->GetUserUseList();
                    for (auto &u_sp : uList) {
                        Use* u = u_sp.get();
                        if (u && u->GetValue() == GV) {
                            usesGV = true;
                            break;
                        }
                    }
                    if (usesGV) break;
                }
                if (usesGV) break;
            }
            if (usesGV)
                usingFuncs.insert(F);
            if (inLoop)
                break;
        }

        // 如果全局变量在循环中使用或者被多个函数共享，则不优化
        if (inLoop || usingFuncs.size() != 1)
            continue;

        // 此时安全提升
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



// bool Global2Local::promoteGlobal(Var* GV, Function* F) {
//     if (!GV || GV->usage != Var::GlobalVar) return false;
//     if (!F || F->GetBBs().empty()) return false;

//     BasicBlock* entryBB = F->GetBBs().front().get();
//     if (!entryBB) return false;

//     // 创建局部 alloca，带名字，插在入口开头
//     AllocaInst* localAlloca = new AllocaInst(GV->GetType());
//     entryBB->push_front(localAlloca);

//     if (GV->GetInitializer()) {
//         Operand initVal = GV->GetInitializer();
//         entryBB->hu1_GenerateStoreInst(initVal, localAlloca,localAlloca);
//     }

//     bool modified = false;

//     for (auto &bb_sp : F->GetBBs()) {
//         BasicBlock* bb = bb_sp.get();
//         if (!bb) continue;

//         for (auto* inst : *bb) {
//             auto &uList = inst->GetUserUseList();
//             for (auto& u_sp : uList) {
//                 Use* u = u_sp.get();
//                 if (!u) continue;
//                 if (u->GetValue() != GV) continue;

//                 inst->ReplaceSomeUseWith(u, localAlloca);
//                 modified = true;
//             }
//         }
//     }
//     auto &globalVars = module->GetGlobalVariable();
//     for (auto iter = globalVars.begin(); iter != globalVars.end(); ) {
//         if (iter->get() && iter->get() == GV) {
//             iter = globalVars.erase(iter);  // 删除GV并安全更新迭代器
//         } else {
//             ++iter;
//         }
//     }
//     return modified;
// }
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

    // 检查全局变量是否被多个函数使用
    int useFuncCount = 0;
    for (auto &funcPtr : module->GetFuncTion()) {
        Function* checkF = funcPtr.get();
        if (!checkF || checkF->GetBBs().empty()) continue;

        bool usesGV = false;
        for (auto &bb_sp : checkF->GetBBs()) {
            BasicBlock* bb = bb_sp.get();
            if (!bb) continue;

            for (auto* inst : *bb) {
                auto &uList = inst->GetUserUseList();
                for (auto &u_sp : uList) {
                    Use* u = u_sp.get();
                    if (u && u->GetValue() == GV) {
                        usesGV = true;
                        break;
                    }
                }
                if (usesGV) break;
            }
            if (usesGV) break;
        }
        if (usesGV) {
            useFuncCount++;
            if (useFuncCount > 1) return false; // 多函数使用，不优化
        }
    }

    // 创建局部 alloca，插在函数入口
    BasicBlock* entryBB = F->GetBBs().front().get();
    if (!entryBB) return false;

    AllocaInst* localAlloca = new AllocaInst(GV->GetType());
    entryBB->push_front(localAlloca);

    if (GV->GetInitializer()) {
        Operand initVal = GV->GetInitializer();
        entryBB->hu1_GenerateStoreInst(initVal, localAlloca, localAlloca);
    }

    bool modified = false;

    for (auto &bb_sp : F->GetBBs()) {
        BasicBlock* bb = bb_sp.get();
        if (!bb) continue;

        for (auto* inst : *bb) {
            auto &uList = inst->GetUserUseList();
            for (auto &u_sp : uList) {
                Use* u = u_sp.get();
                if (!u) continue;
                if (u->GetValue() != GV) continue;

                inst->ReplaceSomeUseWith(u, localAlloca);
                modified = true;
            }
        }
    }

    // 删除模块全局变量
    auto &globalVars = module->GetGlobalVariable();
    for (auto iter = globalVars.begin(); iter != globalVars.end(); ) {
        if (iter->get() && iter->get() == GV) {
            iter = globalVars.erase(iter);
        } else {
            ++iter;
        }
    }

    return modified;
}
