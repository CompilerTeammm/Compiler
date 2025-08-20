#include "../../include/IR/Opt/Inliner.hpp"
#include "../../include/lib/CFG.hpp"

std::unique_ptr<InlinePolicy> InlinePolicy::create(Module *m)
{
    auto policies = std::make_unique<PolicySet>();
    policies->addPolicy(std::make_unique<NoSelfCall>(m));
    policies->addPolicy(std::make_unique<BudgetPolicy>());
    return policies;
}

PolicySet::PolicySet() = default;

void PolicySet::addPolicy(std::unique_ptr<InlinePolicy> policy)
{
    policies.push_back(std::move(policy));
}

bool PolicySet::shouldInline(CallInst *call)
{
    for (auto &policy : policies)
    {
        if (!policy->shouldInline(call))
        {
            return false;
        }
    }
    return true;
}

BudgetPolicy::BudgetPolicy() = default;

bool BudgetPolicy::shouldInline(CallInst *call)
{
    auto master = call->GetParent()->GetParent();          // 调用者函数
    auto inlineFunc = call->GetOperand(0)->as<Function>(); // 被调函数
    assert(master != nullptr && inlineFunc != nullptr);

    auto &[masterCodeSize, masterFrameSize] = master->GetInlineInfo();
    auto &[calleeCodeSize, calleeFrameSize] = inlineFunc->GetInlineInfo();

    // 超过 Frame 限制
    if (calleeFrameSize + masterFrameSize > MaxFrameSize)
        return false;

    // 超过 Code 限制
    if (calleeCodeSize + currentCost > MaxInstCount)
        return false;

    // 更新 inline 信息
    currentCost += calleeCodeSize;
    masterCodeSize += calleeCodeSize;
    masterFrameSize += calleeFrameSize;

    return true;
}

NoSelfCall::NoSelfCall(Module *module) : mod(module) {}

bool NoSelfCall::shouldInline(CallInst *call)
{
    auto callee = call->GetOperand(0)->as<Function>(); // 被调函数
    auto caller = call->GetParent()->GetParent();      // 调用者函数
    assert(caller != nullptr && callee != nullptr);

    // 一些特殊 tag 的函数禁止内联
    if (callee->tag == Function::Tag::ParallelBody ||
        caller->tag == Function::Tag::UnrollBody ||
        callee->tag == Function::Tag::BuildIn)
    {
        return false;
    }

    // 如果全局 Inline_Recursion 启动
    if (Singleton<Inline_Recursion>().flag)
    {
        static int inlineTimes = 0; // 保持原版的静态行为
        if (!caller->isRecursive() && !callee->isRecursive())
            return true;

        if (inlineTimes < 3 &&
            (caller->Size() + callee->Size()) * 3 < 100)
        {
            ++inlineTimes;
            return true;
        }
    }
    // 如果没启用全局递归内联
    else
    {
        if (!caller->isRecursive() && !callee->isRecursive())
            return true;
    }

    return false;
}

bool Inliner::run()
{
    bool modified = false;
    initialize(mod);
    modified |= performInline(mod);
    mod->EraseDeadFunc();
    for (auto &func : mod->GetFuncTion())
    {
        func->ClearInlineInfo();
    }
    return modified;
}

void Inliner::initialize(Module *module)
{
    // 移除没有使用的函数（除了 main）
    for (auto it = module->GetFuncTion().begin(); it != module->GetFuncTion().end();)
    {
        if (it->get()->GetValUseListSize() == 0 && it->get()->GetName() != "main")
        {
            it = module->GetFuncTion().erase(it);
        }
        else
        {
            ++it;
        }
    }

    // 构建策略集合
    auto policy = InlinePolicy::create(module);

    // 收集符合条件的调用点
    for (auto &funcPtr : module->GetFuncTion())
    {
        Function *func = funcPtr.get();
        auto &callList = func->GetValUseList();
        for (auto *use : callList)
        {
            auto *callInst = use->GetUser()->as<CallInst>();
            assert(callInst != nullptr);
            if (policy->shouldInline(callInst))
            {
                pendingCalls.push_back(callInst);
            }
        }
    }
}

bool Inliner::performInline(Module *module)
{
    bool modified = false;

    while (!pendingCalls.empty())
    {
        modified = true;

        Instruction *inst = pendingCalls.front();
        pendingCalls.erase(pendingCalls.begin());

        BasicBlock *block = inst->GetParent();
        Function *func = block->GetParent();

        // 在调用点切分基本块
        BasicBlock *splitBlock = block->SplitAt(inst);

        // 递归内联情况：调用自己
        if (inst->GetUserUseList()[0]->usee == func)
        {
            auto *br = new UnCondInst(splitBlock);
            block->push_back(br);
        }

        BasicBlock::List<Function, BasicBlock>::iterator blockPos(block);
        blockPos.InsertAfter(splitBlock);
        ++blockPos;

        // 拷贝被调用函数体
        std::vector<BasicBlock *> blocks = cloneBlocks(inst);

        // 处理调用跳转
        if (inst->GetUserUseList()[0]->usee != func)
        {
            auto *br = new UnCondInst(blocks[0]);
            block->push_back(br);
        }
        else
        {
            delete block->GetBack();
            auto *br = new UnCondInst(blocks[0]);
            block->push_back(br);
        }

        // 把 AllocaInst 移到函数入口块
        for (auto it = blocks[0]->begin(); it != blocks[0]->end();)
        {
            auto *allocaInst = dynamic_cast<AllocaInst *>(*it);
            ++it;
            if (allocaInst)
            {
                BasicBlock *entryBlock = func->GetFront();
                allocaInst->EraseFromManager();
                entryBlock->push_front(allocaInst);
            }
        }

        // 插入拷贝出来的基本块
        for (BasicBlock *blk : blocks)
        {
            blockPos.InsertBefore(blk);
        }

        // 返回值处理
        if (inst->GetTypeEnum() != IR_DataType::IR_Value_VOID)
        {
            PhiInst *phi = PhiInst::Create(splitBlock->GetFront(), splitBlock, inst->GetType());
            fixReturnPhi(splitBlock, phi, blocks);

            if (phi->GetUserUseList().size() == 1)
            {
                Value *val = phi->GetUserUseList()[0]->usee;
                inst->ReplaceAllUseWith(val);
                delete phi;
            }
            else
            {
                inst->ReplaceAllUseWith(phi);
            }
        }
        else
        {
            fixVoidReturn(splitBlock, blocks);
        }

        // 删除无用的被调函数
        auto *callee = inst->GetOperand(0)->as<Function>();
        if (callee->GetValUseListSize() == 0)
        {
            module->EraseFunction(callee);
        }

        // 删除 call 指令本身
        delete inst;
    }

    return modified;
}

std::vector<BasicBlock *> Inliner::cloneBlocks(Instruction *inst)
{
    auto *calleeFunc = dynamic_cast<Function *>(inst->GetUserUseList()[0]->usee);
    assert(calleeFunc != nullptr);

    std::unordered_map<Operand, Operand> operandMap;

    // 参数映射：callee 参数 -> call 指令的操作数
    const auto &params = calleeFunc->GetParams();
    int idx = 1; // 第 0 个操作数是函数本身
    for (auto &param : params)
    {
        Value *paramVal = param.get();
        operandMap[paramVal] = inst->GetUserUseList()[idx]->usee;
        ++idx;
    }

    // 克隆每个基本块
    std::vector<BasicBlock *> clonedBlocks;
    for (BasicBlock *block : *calleeFunc)
    {
        clonedBlocks.push_back(block->clone(operandMap));
    }

    return clonedBlocks;
}

void Inliner::fixVoidReturn(BasicBlock *splitBlock, std::vector<BasicBlock *> &blocks)
{
    for (BasicBlock *block : blocks)
    {
        Instruction *inst = block->GetBack();
        if (dynamic_cast<RetInst *>(inst))
        {
            auto *br = new UnCondInst(splitBlock);
            inst->DropAllUsesOfThis();
            inst->EraseFromManager();
            block->push_back(br);
        }
    }
}

void Inliner::fixReturnPhi(BasicBlock *splitBlock, PhiInst *phi, std::vector<BasicBlock *> &blocks)
{
    for (BasicBlock *block : blocks)
    {
        Instruction *inst = block->GetBack();
        if (auto *retInst = dynamic_cast<RetInst *>(inst))
        {
            // 将返回值加入 Phi 节点
            assert(!retInst->GetUserUseList().empty());
            phi->addIncoming(retInst->GetUserUseList()[0]->usee, block);

            // 替换 ret 为无条件跳转到 splitBlock
            auto *br = new UnCondInst(splitBlock);
            retInst->DropAllUsesOfThis();
            retInst->EraseFromManager();
            block->push_back(br);
        }
    }
}
