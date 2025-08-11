#include "../../include/IR/Opt/CondMerge.hpp"
#include "../../include/lib/MyList.hpp"
#include "../../lib/CFG.hpp"
#include <iostream>

bool CondMerge::run() {
    bool modified = false;
    const int max_iter = 15;
    int iter = 0;

    while (iter++ < max_iter) {
        DominantTree _tree(func);
        _tree.BuildDominantTree();

        for (auto& bb_ptr : func->GetBBs()) {
            BasicBlock* bb = bb_ptr.get();
            if (!bb) continue;
            bb->PredBlocks = _tree.getPredBBs(bb);
            bb->NextBlocks = _tree.getSuccBBs(bb);
        }

        bool localChange = false;

        for (auto& bb_ptr : func->GetBBs()) {
            BasicBlock* bb = bb_ptr.get();
            if (!bb) continue;
            Instruction* term = bb->GetBack();
            if (!term) continue;

            if (term->id != Instruction::Op::Cond) continue;

            BasicBlock* trueBlock = dynamic_cast<BasicBlock*>(term->GetOperand(1));
            BasicBlock* falseBlock = dynamic_cast<BasicBlock*>(term->GetOperand(2));
            if (!trueBlock || !falseBlock) continue;

            if (TryMergeCondOr(bb, trueBlock, falseBlock)) {
                localChange = true;
                break;
            }
        }

        if (!localChange) {
            break;
        }
        modified |= localChange;
    }
    return modified;
}

// if (cond1)
//   then {
//     if (cond2)
//       then do X
//     else do Y
//   }
// else do Y
// 优化为:
// if (cond1 || cond2)
//   then do X
// else do Y
bool CondMerge::TryMergeCondOr(BasicBlock* bb, BasicBlock* trueBlock, BasicBlock* falseBlock){
    //保证结构单一
    if(trueBlock->PredBlocks.size()!=1 || trueBlock->PredBlocks[0]!=bb){
        return false;
    }
    //trueblock的终结指令得是跳转
    Instruction* trueT=trueBlock->GetLastInsts();
    if(!trueT ||trueT->id !=Instruction::Op::Cond){
        return false;
    }
    BasicBlock* trueTrueBlock = dynamic_cast<BasicBlock*>(trueT->GetOperand(1));
    BasicBlock* trueFalseBlock = dynamic_cast<BasicBlock*>(trueT->GetOperand(2));
    if (!trueTrueBlock || !trueFalseBlock) {
        return false;
    }

    // 外层falseBlock 和 内层falseBlock 必须相同，才能合并（if else块对应）
    if (falseBlock != trueFalseBlock) {
        return false;
    }

    //符合条件开始构造
    auto cond1=bb->GetLastInsts()->GetOperand(0);
    auto cond2 = trueT->GetOperand(0);

    auto newCond=bb->GenerateBinaryInst(cond1,BinaryInst::Op_Or,cond2);
    CondInst* newCondInst = new CondInst(newCond, trueTrueBlock, falseBlock);
    bb->GetLastInsts()->InstReplace(newCondInst);

    //将trueblock中移除终结指令的所有指令到bb的末尾之前
    auto &trueBBInsts = trueBlock;
    auto &bbInsts = bb;
    auto iterInst = trueBlock->begin();
    while (iterInst != trueBlock->end() && *iterInst != trueT){
        Instruction* inst = *iterInst;
        auto nextIter = iterInst;
        ++nextIter;
        inst->EraseFromManager();
        auto bbTerminatorIter = bb->begin();
        for (; bbTerminatorIter != bb->end(); ++bbTerminatorIter) {
            if (*bbTerminatorIter == bb->GetLastInsts()) break;
        }
        assert(bbTerminatorIter != bb->end());
        bbTerminatorIter.InsertBefore(inst);
        iterInst = nextIter;
    }
    // 更新CFG
    bb->ReplaceNextBlock(trueBlock, trueTrueBlock);
    trueTrueBlock->ReplacePreBlock(trueBlock, bb);
    func->RemoveBBs(trueBlock);
    return true;
}