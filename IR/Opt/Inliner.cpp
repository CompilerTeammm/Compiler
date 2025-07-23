#include "../../include/IR/Opt/Inliner.hpp"
#include "../../include/lib/CFG.hpp"

std::unique_ptr<InlineHeuristic> InlineHeuristic::get(Module *m)
{
    auto heuristic = std::make_unique<InlineHeuristicManager>();
    heuristic->push_back(std::make_unique<NoRecursive>(m));
    heuristic->push_back(std::make_unique<SizeLimit>());
    return heuristic;
}

InlineHeuristicManager::InlineHeuristicManager()
{
}

bool InlineHeuristicManager::CanBeInlined(CallInst *call)
{
    for (auto &heuristic : *this)
        if (!heuristic->CanBeInlined(call))
            return false;
    return true;
}

SizeLimit::SizeLimit()
{
}

bool SizeLimit::CanBeInlined(CallInst *call)
{
    // static size_t cost = 0;
    auto master = call->GetParent()->GetParent();
    auto inline_func = call->GetOperand(0)->as<Function>();
    assert(master != nullptr && inline_func != nullptr);
    auto &[master_code_size, master_frame_size] = master->GetInlineInfo();
    auto &[inline_code_size, inline_frame_size] = inline_func->GetInlineInfo();
    if (inline_frame_size + master_frame_size > maxframesize)
        return false;
    if (inline_code_size + cost > maxsize)
        return false;
    cost += inline_code_size;
    master_code_size += inline_code_size;
    master_frame_size += inline_frame_size;
    return true;
}

NoRecursive::NoRecursive(Module *_m) : m(_m)
{
}

bool NoRecursive::CanBeInlined(CallInst *call)
{
    auto &&slave = call->GetOperand(0)->as<Function>();
    auto &&master = call->GetParent()->GetParent();
    if (slave->tag == Function::Tag::ParallelBody || master->tag == Function::Tag::UnrollBody || slave->tag == Function::Tag::BuildIn)
        return false;
    if (Singleton<Inline_Recursion>().flag)
    {
        static int InlineTimes = 0;
        if (!master->isRecursive() && !slave->isRecursive())
            return true;
        if (InlineTimes<3&&(master->Size()+slave->Size())*3<100)
        {
            InlineTimes++;
            return true;
        }
    }
    else
    {
        if (!master->isRecursive() && !slave->isRecursive())
            return true;
    }
    return false;
}

bool Inliner::run()
{
    bool modified = false;
    init(m);
    modified |= Inline(m);
    m->EraseDeadFunc();
    for (auto &func : m->GetFuncTion())
        func->ClearInlineInfo();
    return modified;
}

void Inliner::init(Module *m)
{
    for (auto it = m->GetFuncTion().begin(); it != m->GetFuncTion().end();)
    {
        if (it->get()->GetValUseListSize() == 0 && it->get()->GetName() != "main")
            it = m->GetFuncTion().erase(it);
        else
            it++;
    }

    auto judge = InlineHeuristic::get(m);

    for (auto &funcptr : m->GetFuncTion())
    {
        Function *func = funcptr.get();
        auto &calllists = func->GetValUseList();
        for (auto callinst : calllists)
        {
            auto call = callinst->GetUser()->as<CallInst>();
            assert(call != nullptr);
            if (judge->CanBeInlined(call))
                NeedInlineCall.push_back(call);
        }
    }
}

bool Inliner::Inline(Module *m)
{
    bool modified = false;
    while (!NeedInlineCall.empty())
    {
        modified |= true;
        Instruction *inst = NeedInlineCall.front();
        NeedInlineCall.erase(NeedInlineCall.begin());
        BasicBlock *block = inst->GetParent();
        Function *func = block->GetParent();
        BasicBlock *SplitBlock = block->SplitAt(inst);
        if (inst->GetUserUseList()[0]->usee == func)
        {
            auto Br = new UnCondInst(SplitBlock);
            block->push_back(Br);
        }
        BasicBlock::List<Function, BasicBlock>::iterator Block_Pos(block);
        Block_Pos.InsertAfter(SplitBlock);
        ++Block_Pos;
        std::vector<BasicBlock *> blocks = CopyBlocks(inst);
        if (inst->GetUserUseList()[0]->usee != func)
        {
            UnCondInst *Br = new UnCondInst(blocks[0]);
            block->push_back(Br);
        }
        else
        {
            delete block->GetBack();
            UnCondInst *Br = new UnCondInst(blocks[0]);
            block->push_back(Br);
        }

        for (auto it = blocks[0]->begin(); it != blocks[0]->end();)
        {
            auto shouldmvinst = dynamic_cast<AllocaInst *>(*it);
            ++it;
            if (shouldmvinst)
            {
                BasicBlock *front_block = func->GetFront();
                shouldmvinst->EraseFromManager();
                front_block->push_front(shouldmvinst);
            }
        }
        for (BasicBlock *block_ : blocks)
            Block_Pos.InsertBefore(block_);
        if (inst->GetTypeEnum() != IR_DataType::IR_Value_VOID)
        {
            PhiInst *Phi = PhiInst::Create(SplitBlock->GetFront(), SplitBlock, inst->GetType());
            HandleRetPhi(SplitBlock, Phi, blocks);
            if (Phi->GetUserUseList().size() == 1)
            {
                Value *val = Phi->GetUserUseList()[0]->usee;
                inst->ReplaceAllUseWith(val);
                delete Phi;
            }
            else
                inst->ReplaceAllUseWith(Phi);
        }
        else
            HandleVoidRet(SplitBlock, blocks);
        auto &&inlined_func = inst->GetOperand(0)->as<Function>();
        if (inlined_func->GetValUseListSize() == 0)
            m->EraseFunction(inlined_func);
        delete inst;
    }
    return modified;
}

std::vector<BasicBlock *> Inliner::CopyBlocks(Instruction *inst)
{
    Function *Func = dynamic_cast<Function *>(inst->GetUserUseList()[0]->usee);
    std::unordered_map<Operand, Operand> OperandMapping;

    std::vector<BasicBlock *> copied_bbs;
    int num = 1;
    for (auto &param : Func->GetParams())
    {
        Value *Param = param.get();
        OperandMapping[Param] = inst->GetUserUseList()[num]->usee;
        num++;
    }
    for (BasicBlock *block : *Func)
        copied_bbs.push_back(block->clone(OperandMapping));
    return copied_bbs;
}

void Inliner::HandleVoidRet(BasicBlock *spiltBlock, std::vector<BasicBlock *> &blocks)
{
    for (BasicBlock *block : blocks)
    {
        Instruction *inst = block->GetBack();
        if (dynamic_cast<RetInst *>(inst))
        {
            UnCondInst *Br = new UnCondInst(spiltBlock);
            inst->DropAllUsesOfThis();
            inst->EraseFromManager();
            block->push_back(Br);
        }
    }
}

void Inliner::HandleRetPhi(BasicBlock *RetBlock, PhiInst *Phi, std::vector<BasicBlock *> &blocks)
{
    for (BasicBlock *block : blocks)
    {
        Instruction *inst = block->GetBack();
        if (dynamic_cast<RetInst *>(inst))
        {
            Phi->addIncoming(inst->GetUserUseList()[0]->usee, block);
            UnCondInst *Br = new UnCondInst(RetBlock);
            inst->DropAllUsesOfThis();
            inst->EraseFromManager();
            block->push_back(Br);
        }
    }
}
