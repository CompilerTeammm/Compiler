#include "../../include/IR/Opt/ConstantHoist.hpp"

bool ConstantHoist::processBlock(BasicBlock* block)
{
    bool modified = false;

    // 获取当前块的终结指令
    Instruction* termInst = block->GetBack();
    auto* condBr = dynamic_cast<CondInst*>(termInst);
    if (!condBr) return false;

    BasicBlock* trueBlock = dynamic_cast<BasicBlock*>(condBr->GetOperand(1));
    BasicBlock* falseBlock = dynamic_cast<BasicBlock*>(condBr->GetOperand(2));

    // 基本验证：避免环、相等块或使用次数不为 1
    if (!trueBlock || !falseBlock ||
        trueBlock == falseBlock || trueBlock == block || falseBlock == block ||
        trueBlock->GetValUseList().GetSize() != 1 || falseBlock->GetValUseList().GetSize() != 1 ||
        trueBlock->Size() != falseBlock->Size())
    {
        return false;
    }

    // 严格菱形检查：两侧 terminator 必须是 UnCond 且指向同一个 Merge 块
    auto* trueTerm = dynamic_cast<UnCondInst*>(trueBlock->GetBack());
    auto* falseTerm = dynamic_cast<UnCondInst*>(falseBlock->GetBack());
    if (!trueTerm || !falseTerm) return false;

    BasicBlock* mergeBlock = trueTerm->GetOperand(0)->as<BasicBlock>();
    if (!mergeBlock || mergeBlock != falseTerm->GetOperand(0)->as<BasicBlock>()) return false;

    // 避免两侧是 return 或终止函数的指令
    if (dynamic_cast<RetInst*>(trueTerm) || dynamic_cast<RetInst*>(falseTerm)) return false;

    // 尝试提升 True/False 块的指令
    if (!hoistInstructionsBetweenBlocks(trueBlock, falseBlock)) return false;

    // cmp 指令
    Instruction* cmpInst = condBr->GetOperand(0)->as<Instruction>();

    // 处理 HoistList 中的提升项
    while (!hoistEntries.empty())
    {
        modified = true;
        auto hoistNode = std::move(hoistEntries.back());
        hoistEntries.pop_back();

        SelectInst* selInst = new SelectInst(cmpInst, hoistNode->lhsValue, hoistNode->rhsValue);
        BasicBlock::List<BasicBlock, Instruction>::iterator pos(cmpInst);
        pos.InsertAfter(selInst);

        hoistNode->lhsInst->ReplaceSomeUseWith(hoistNode->index, selInst);
        hoistNode->rhsInst->ReplaceSomeUseWith(hoistNode->index, selInst);
    }

    // 将 TrueBlock 的指令移动到当前块（跳过终结指令）
    BasicBlock::List<BasicBlock, Instruction>::iterator insertPos(condBr);
    for (auto it = trueBlock->begin(); it != trueBlock->end();)
    {
        Instruction* inst = *it;
        ++it;
        if (inst == trueBlock->GetBack()) break;
        inst->EraseFromManager();
        insertPos.InsertBefore(inst);
    }

    // 替换原条件分支为无条件跳转到 mergeBlock
    {
        BasicBlock::List<BasicBlock, Instruction>::iterator brPos(condBr);
        UnCondInst* newJmp = new UnCondInst(mergeBlock);
        brPos.InsertBefore(newJmp);
        delete condBr;
    }

    return modified;
}

bool ConstantHoist::hoistInstructionsBetweenBlocks(BasicBlock* trueBlock, BasicBlock* falseBlock)
{
    bool modified = false;
    int index = 0;

    // 第一轮：检查指令类型、ID、使用数一致性，同时记录索引
    for (auto trueIter = trueBlock->begin(), falseIter = falseBlock->begin();
         trueIter != trueBlock->end() && falseIter != falseBlock->end();
         ++trueIter, ++falseIter, ++index)
    {
        Instruction* trueInst = *trueIter;
        Instruction* falseInst = *falseIter;

        if (!trueInst || !falseInst) return false;
        if (trueInst->GetInstId() != falseInst->GetInstId()) return false;
        if (trueInst->GetType() != falseInst->GetType()) return false;
        if (trueInst->GetUserUseList().size() != falseInst->GetUserUseList().size()) return false;

        instructionIndices[trueBlock][trueInst] = index;
        instructionIndices[falseBlock][falseInst] = index;
    }

    // 第二轮：逐操作数检查是否可以提升
    for (auto trueIter = trueBlock->begin(), falseIter = falseBlock->begin();
         trueIter != trueBlock->end() && falseIter != falseBlock->end();
         ++trueIter, ++falseIter)
    {
        Instruction* trueInst = *trueIter;
        Instruction* falseInst = *falseIter;

        if (dynamic_cast<PhiInst*>(trueInst)) continue;

        for (size_t i = 0; i < trueInst->GetUserUseList().size(); ++i)
        {
            Value* trueOp = trueInst->GetOperand(i);
            Value* falseOp = falseInst->GetOperand(i);

            if (!trueOp || !falseOp) return false;
            if (trueOp->GetType() != falseOp->GetType()) return false;

            auto trueUser = dynamic_cast<Instruction*>(trueOp);
            auto falseUser = dynamic_cast<Instruction*>(falseOp);

            if ((dynamic_cast<ConstantData*>(trueOp) && dynamic_cast<ConstantData*>(falseOp)) ||
                (dynamic_cast<Var*>(trueOp) && dynamic_cast<Var*>(falseOp)))
            {
                if (trueOp != falseOp)
                {
                    modified = true;
                    hoistEntries.push_back(std::make_unique<HoistEntry>(trueInst, trueOp, falseInst, falseOp, i));
                }
            }
            else if (dynamic_cast<StoreInst*>(trueInst) && i == 0 && trueUser && falseUser &&
                     trueUser->GetParent() != trueBlock && falseUser->GetParent() != falseBlock)
            {
                if (trueOp != falseOp)
                {
                    modified = true;
                    hoistEntries.push_back(std::make_unique<HoistEntry>(trueInst, trueOp, falseInst, falseOp, i));
                }
            }
            else if (trueUser && falseUser &&
                     trueUser->GetParent() == trueBlock && falseUser->GetParent() == falseBlock)
            {
                if (instructionIndices[trueBlock][trueUser] != instructionIndices[falseBlock][falseUser]) return false;
            }
            else if (trueUser || falseUser)
            {
                if ((trueUser && trueUser->GetParent() == trueBlock) ||
                    (falseUser && falseUser->GetParent() == falseBlock))
                    return false;
            }
        }
    }

    // PHI 检查函数
    auto checkPhiConsistency = [&](BasicBlock* blk) -> bool {
        for (Instruction* inst : *blk)
        {
            if (auto phi = dynamic_cast<PhiInst*>(inst))
            {
                Value* trueVal = phi->ReturnValIn(trueBlock);
                Value* falseVal = phi->ReturnValIn(falseBlock);
                if (trueVal != falseVal)
                {
                    hoistEntries.clear();
                    return false;
                }
            }
            else break;
        }
        return true;
    };

    Instruction* trueTerm = trueBlock->GetBack();
    if (dynamic_cast<CondInst*>(trueTerm))
    {
        BasicBlock* tSucc = trueTerm->GetOperand(1)->as<BasicBlock>();
        BasicBlock* fSucc = trueTerm->GetOperand(2)->as<BasicBlock>();
        if (!checkPhiConsistency(tSucc) || !checkPhiConsistency(fSucc)) return false;
    }
    else if (dynamic_cast<UnCondInst*>(trueTerm))
    {
        BasicBlock* succ = trueTerm->GetOperand(0)->as<BasicBlock>();
        if (!checkPhiConsistency(succ)) return false;
    }

    return modified;
}

bool ConstantHoist::run()
{
    bool modified = false;
    for (BasicBlock* block : *function) {
        // 尝试对每个基本块进行常量提升
        modified |= processBlock(block);
    }
    return modified;
}