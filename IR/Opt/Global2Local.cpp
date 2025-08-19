#include "../../include/IR/Opt/Global2Local.hpp"

bool Global2Local::run()
{
    createSuccFuncs();
    createCallNum();
    detectRecursive();

    bool modified = false;
    std::set<std::string> ExcludeGVs = {"next", "dst", "src"};

    for (auto &gvPtr : module->GetGlobalVariable())
    {
        Var *GV = gvPtr.get();
        if (!GV || GV->usage != Var::GlobalVar)
            continue;

        std::string gvName = GV->GetName();
        if (ExcludeGVs.count(gvName))
            continue;

        // *** 仅处理“标量”全局：跳过数组/结构体/指针等复杂类型
        auto *Ty = GV->GetType();
        if (!Ty)
            continue;

        IR_DataType tyKind = Ty->GetTypeEnum();
        if (tyKind == IR_ARRAY || tyKind == IR_PTR || tyKind == BACKEND_PTR)
        {
            continue;
        }
        // 地址逃逸或没有初始化的不优化
        if (addressEscapes(GV))
            continue;
        if (!GV->GetInitializer())
            continue;
        if (auto *CI = dynamic_cast<ConstIRInt *>(GV->GetInitializer()))
        {
            if (CI->GetVal() == 0)
                continue;
        }
        else
        {
            // 不是整型常量初始化的不做
            continue;
        }
        std::set<Function *> candidateFuncs;

        for (auto &funcPtr : module->GetFuncTion())
        {
            Function *F = funcPtr.get();
            if (!F || F->GetBBs().empty())
                continue;
            if (RecursiveFuncs.count(F))
                continue;

            // 安全检查：函数内是否有同名局部变量或参数
            // 这个难道没作用?
            if (HasLocalVarNamed(GV->GetName(), F))
                continue;

            bool inLoop = false;
            bool usesGV = false;
            int storeCount = 0;
            bool localDependsOnGV = false;

            for (auto &bb_sp : F->GetBBs())
            {
                BasicBlock *bb = bb_sp.get();
                if (!bb)
                    continue;
                if (bb->LoopDepth > 0)
                    inLoop = true;

                for (auto *inst : *bb)
                {
                    // 检查局部变量是否依赖 GV
                    if (auto *allocaInst = dynamic_cast<AllocaInst *>(inst))
                    {
                        for (int i = 0; i < allocaInst->GetOperandNums(); i++)
                        {
                            Value *op = allocaInst->GetOperand(i);
                            if (usesValue(op, GV))
                            {
                                localDependsOnGV = true;
                                break;
                            }
                        }
                    }

                    // 检查指令是否使用全局变量
                    auto &uList = inst->GetUserUseList();
                    for (auto &u_sp : uList)
                    {
                        Use *u = u_sp.get();
                        if (!u)
                            continue;
                        if (u->GetValue() != GV)
                            continue;

                        usesGV = true;
                        if (dynamic_cast<StoreInst *>(inst))
                            storeCount++;
                    }
                }
            }

            // 跳过不安全情况
            if (inLoop || storeCount > 1 || localDependsOnGV)
                continue;
            if (usesGV)
                candidateFuncs.insert(F);
        }

        // 只优化单函数候选
        if (candidateFuncs.size() != 1)
            continue;

        Function *targetFunc = *candidateFuncs.begin();
        modified |= promoteGlobal(GV, targetFunc);
    }

    return modified;
}

void Global2Local::createSuccFuncs()
{
    for (auto &funcPtr : module->GetFuncTion())
    {
        Function *F = funcPtr.get();

        for (auto &BBptr : F->GetBBs())
        {                                 // BBptr 是 shared_ptr<BasicBlock>
            BasicBlock *BB = BBptr.get(); // 获取裸指针
            for (auto *inst : *BB)
            { // 利用 List 提供的迭代器
                if (auto *call = dynamic_cast<CallInst *>(inst))
                {
                    Value *calledVal = call->GetCalledFunction();
                    for (auto &calleePtr : module->GetFuncTion())
                    {
                        if (calleePtr.get() == calledVal)
                        {
                            FuncSucc[F].insert(calleePtr.get());
                        }
                    }
                }
            }
        }
    }
}

void Global2Local::createCallNum()
{
    // 先把所有函数调用次数初始化为 0
    for (auto &funcPtr : module->GetFuncTion())
    {
        CallNum[funcPtr.get()] = 0;
    }

    // 遍历每个函数的基本块和指令，统计调用次数
    for (auto &funcPtr : module->GetFuncTion())
    {
        Function *F = funcPtr.get();

        for (auto &BBptr : F->GetBBs())
        {
            BasicBlock *BB = BBptr.get();
            for (auto *inst : *BB)
            { // 利用 List 提供的迭代器
                if (auto *call = dynamic_cast<CallInst *>(inst))
                {
                    Value *calledVal = call->GetCalledFunction();
                    for (auto &calleePtr : module->GetFuncTion())
                    {
                        if (calleePtr.get() == calledVal)
                        {
                            CallNum[calleePtr.get()]++;
                        }
                    }
                }
            }
        }
    }
}

void Global2Local::detectRecursive()
{
    // 用于 DFS 访问状态：0 = 未访问, 1 = 访问中, 2 = 已完成
    std::unordered_map<Function *, int> visitStatus;

    // 初始化
    for (auto &funcPtr : module->GetFuncTion())
    {
        visitStatus[funcPtr.get()] = 0;
    }

    // 定义 DFS lambda
    std::function<void(Function *)> dfs = [&](Function *F)
    {
        if (visitStatus[F] == 1)
        {
            // 正在访问中，又遇到自己 -> 递归
            RecursiveFuncs.insert(F);
            return;
        }
        if (visitStatus[F] == 2)
            return; // 已经处理过

        visitStatus[F] = 1; // 标记为访问中

        // 遍历 F 的后继函数
        for (auto *succ : FuncSucc[F])
        {
            dfs(succ);
            // 如果后继是递归函数，也把 F 标记为递归
            if (RecursiveFuncs.count(succ))
                RecursiveFuncs.insert(F);
        }

        visitStatus[F] = 2; // 标记为完成
    };

    // 对每个函数执行 DFS
    for (auto &funcPtr : module->GetFuncTion())
    {
        dfs(funcPtr.get());
    }
}

bool Global2Local::addressEscapes(Value *V)
{
    if (!V || !V->GetType())
        return true;
    auto *Ty = V->GetType();
    if (!Ty)
        return true; // 保守处理

    IR_DataType TKind = Ty->GetTypeEnum();
    if (TKind == IR_ARRAY)
        return true;

    for (auto use : V->GetValUseList())
    {
        User *user = use->GetUser();
        if (!user)
            continue;

        if (dynamic_cast<CallInst *>(user))
        {
            return true; // 传入调用
        }

        if (auto *store = dynamic_cast<StoreInst *>(user))
        {
            // store val, ptr
            int idx = user->GetUseIndex(use);
            if (idx == 0)
            {
                // 把 GV 作为“被存的值” —— 基本不该出现，保守处理为逃逸
                return true;
            }
            else if (idx == 1)
            {
                // 存到以 GV 为基的地址
                Value *ptrVal = use->GetValue();
                if (!isSimplePtrToSelf(ptrVal, V))
                    return true;
            }
        }

        if (dynamic_cast<PhiInst *>(user))
            return true;

        if (auto *gep = dynamic_cast<GepInst *>(user))
        {
            // 任何出现 GEP 都认为会形成地址计算，保守视作逃逸
            return true;
        }
    }
    return false;
}

// 辅助函数：判断 ptr 是否是“简单转换指向自己”，安全不算逃逸
bool Global2Local::isSimplePtrToSelf(Value *ptr, Value *V)
{
    if (ptr == V)
        return true;
    if (auto *gep = dynamic_cast<GepInst *>(ptr))
    {
        return gep->GetOperand(0) == V && !HasVariableIndex(gep);
    }
    return false;
}

bool Global2Local::HasVariableIndex(GepInst *gep)
{
    for (int i = 0; i < gep->GetOperandNums(); i++)
    {
        Value *idx = gep->GetOperand(i);
        if (!dynamic_cast<ConstIRInt *>(idx))
        {
            return true; // 遇到非常量，就是变量下标
        }
    }
    return false;
}

// 辅助函数：判断某值是否直接或间接依赖 GV
bool Global2Local::usesValue(Value *val, Var *GV)
{
    if (!val)
        return false;
    if (val == GV)
        return true;

    if (auto *inst = dynamic_cast<Instruction *>(val))
    {
        int n = inst->GetOperandNums();
        for (int i = 0; i < n; ++i)
        {
            Value *op = inst->GetOperand(i);
            if (usesValue(op, GV))
                return true;
        }
    }
    return false;
}

bool Global2Local::promoteGlobal(Var *GV, Function *F)
{
    if (!GV || GV->usage != Var::GlobalVar)
        return false;
    if (!F || F->GetBBs().empty())
        return false;

    auto *Ty = GV->GetType();
    if (!Ty)
        return false;

    IR_DataType TKind = Ty->GetTypeEnum();
    if (TKind == IR_ARRAY || TKind == IR_PTR || TKind == BACKEND_PTR)
        return false;
    // 再次检查函数内是否有同名局部变量或参数
    if (HasLocalVarNamed(GV->GetName(), F))
        return false;

    // 在 entry block 插入局部 alloca
    BasicBlock *entryBB = F->GetBBs().front().get();
    if (!entryBB)
        return false;

    AllocaInst *localAlloca = new AllocaInst(GV->GetType());
    entryBB->push_front(localAlloca);

    // 初始化值放在 entry block 的 first instruction 之后
    if (GV->GetInitializer())
    {
        Operand initVal = GV->GetInitializer();
        entryBB->hu1_GenerateStoreInst(initVal, localAlloca, localAlloca);
    }

    bool modified = false;

    // 遍历函数内所有指令，将使用 GV 的地方替换为 localAlloca
    for (auto &bb_sp : F->GetBBs())
    {
        BasicBlock *bb = bb_sp.get();
        if (!bb)
            continue;

        for (auto *inst : *bb)
        {
            auto &uList = inst->GetUserUseList();
            for (auto &u_sp : uList)
            {
                Use *u = u_sp.get();
                if (!u)
                    continue;
                if (u->GetValue() != GV)
                    continue;

                // 只替换真正使用全局变量的指针操作
                if (auto *loadInst = dynamic_cast<LoadInst *>(inst))
                {
                    // Load 的第 0 个操作数是指针
                    if (loadInst->GetOperand(0) == GV)
                    {
                        inst->ReplaceSomeUseWith(u, localAlloca);
                        modified = true;
                    }
                }
                else if (auto *storeInst = dynamic_cast<StoreInst *>(inst))
                {
                    // Store 的第 1 个操作数是指针
                    if (storeInst->GetOperand(1) == GV)
                    {
                        inst->ReplaceSomeUseWith(u, localAlloca);
                        modified = true;
                    }
                }
            }
        }
    }

    // 删除全局变量
    auto &globalVars = module->GetGlobalVariable();
    for (auto iter = globalVars.begin(); iter != globalVars.end();)
    {
        if (iter->get() == GV)
        {
            iter = globalVars.erase(iter);
        }
        else
        {
            ++iter;
        }
    }

    return modified;
}

bool Global2Local::HasLocalVarNamed(const std::string &name, Function *F)
{
    for (auto &bb_sp : F->GetBBs())
    {
        BasicBlock *bb = bb_sp.get();
        if (!bb)
            continue;
        for (auto *inst : *bb)
        {
            if (auto *allocaInst = dynamic_cast<AllocaInst *>(inst))
            {
                if (allocaInst->GetName() == name)
                    return true;
            }
        }
    }
    return false;
}
