#include "../../include/IR/Opt/Inliner.hpp"
#include "../../include/lib/CFG.hpp"

std::unique_ptr<InlineHeuristic> InlineHeuristic::CreateDefaultHeuristic(Module* m) {
    auto heuristic = std::make_unique<InlineHeuristicManager>(m);
    heuristic->push_back(std::make_unique<NoRecursiveHeuristic>(m));
    heuristic->push_back(std::make_unique<SizeLimitHeuristic>());
    return heuristic;
}

SizeLimitHeuristic::SizeLimitHeuristic(size_t maxSize) : maxSize_(maxSize) {}

bool SizeLimitHeuristic::CanBeInlined(CallInst* call) {
    if (!call) return false; 

    Function* callee = dynamic_cast<Function*>(call->GetCalledFunction());
    if (!callee) return false;

    size_t calleeSize = callee->GetInstructionCount();

    return calleeSize <= maxSize_;
}

NoRecursiveHeuristic::NoRecursiveHeuristic(Module* m) : module_(m) {}

bool NoRecursiveHeuristic::CanBeInlined(CallInst* call) {
    if (!call) return false;

    Function* callee = dynamic_cast<Function*>(call->GetCalledFunction());
    if (!callee) return false;

    BasicBlock* parentBB = call->GetParent();
    if (!parentBB) return false;
    Function* caller = parentBB->GetParent();
    if (!caller) return false;

    if (callee == caller) {
        return false;
    }

    return true;
}

Inliner::Inliner(Module* m) : module_(m) {
    heuristic_ = InlineHeuristic::CreateDefaultHeuristic(module_);
}

bool Inliner::run() {
    bool modified = false;
    init(module_);
    modified |= Inline(module_);
    module_->EraseDeadFunc();

    for (auto& func : module_->GetFuncTion())
        func->ClearInlineInfo();

    return modified;
}

void Inliner::init(Module* m) {
    for (auto it = m->GetFuncTion().begin(); it != m->GetFuncTion().end(); ) {
        if ((*it)->GetValUseListSize() == 0 && (*it)->GetName() != "main")
            it = m->GetFuncTion().erase(it);
        else
            ++it;
    }

    for (auto& funcptr : m->GetFuncTion()) {
        Function* func = funcptr.get();
        for (auto* user : func->GetValUseList()) {
            auto* call = dynamic_cast<CallInst*>(user->GetUser());
            if (!call) continue;
            if (heuristic_->CanBeInlined(call))
                callsToInline_.push_back(call);
        }
    }
}

bool Inliner::Inline(Module* m) {
    bool changed = false;
    for (auto* call : callsToInline_) {
        changed |= InlineCall(call);
    }
    return changed;
}

bool Inliner::InlineCall(CallInst* call) {
    if (!call || !call->GetCalledFunction())
        return false;

    Function* callee = dynamic_cast<Function*>(call->GetCalledFunction());
    if (callee->GetTag() == Function::BuildIn)
        return false;

    std::vector<BasicBlock*> copiedBlocks = CopyBlocks(call);
    if (copiedBlocks.empty())
        return false;

    Function* caller = call->GetParent()->GetParent();
    BasicBlock* insert_after = call->GetParent();
    for (auto* new_bb : copiedBlocks) {
        caller->InsertBlockAfter(insert_after, new_bb);
        insert_after = new_bb; // 继续往后插入
    }

    if (call->GetTypeEnum() == IR_Value_VOID) {
        HandleVoidRet(call->GetParent(), copiedBlocks);
    } else {
        for (auto* bb : copiedBlocks) {
            for (auto* inst : *bb) {
                if (auto* phi = dynamic_cast<PhiInst*>(inst)) {
                    HandleRetPhi(bb, phi, copiedBlocks);
                }
            }
        }
    }
    call->DropAllUsesOfThis();
    call->GetParent()->erase(call);
    return true;
}

std::vector<BasicBlock *> Inliner::CopyBlocks(CallInst *call)
{
    Function *Func = dynamic_cast<Function *>(call->GetCalledFunction());
    std::unordered_map<Operand, Operand> OperandMapping;

    std::vector<BasicBlock *> copied_bbs;
    int num = 1;
    for (auto &param : Func->GetParams())
    {
        Value *Param = param.get();
        OperandMapping[Param] = call->GetUserUseList()[num]->usee;
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