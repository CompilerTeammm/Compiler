#include "../../include/IR/Opt/ECE.hpp"

bool EdgeCriticalElim::run() {
  splitCriticalEdges();
  return false;
}

void EdgeCriticalElim::splitCriticalEdges() {
    for (auto* block : *func) {
        int succCount = block->GetSuccessorCount();
        if (succCount <= 1) continue;

        // 获取条件跳转指令
        auto* terminator = block->GetTerminator();
        auto* condInst = dynamic_cast<CondInst*>(terminator);
        if (!condInst) continue;

        // 为每个后继创建空块
        for (int i = 0; i < succCount; ++i) {
            insertEmptyBlock(condInst, i);
        }
    }
}

void EdgeCriticalElim::insertEmptyBlock(Instruction* inst, int succIndex) {
    auto* condInst = dynamic_cast<CondInst*>(inst);

    auto* currBlock = inst->GetParent();
    auto* succBlock = dynamic_cast<BasicBlock*>(condInst->GetUserUseList()[succIndex + 1]->GetValue());

    // 判断是否为关键边
    if (currBlock->GetSuccessorCount() <= 1 || succBlock->GetPredecessorCount() <= 1) 
        return;  // 非关键边，不拆分

    // 避免重复拆分
    auto edge = std::make_pair(currBlock, succBlock);
    if (splitEdges.count(edge) > 0) 
        return;
    splitEdges.insert(edge);

    // 创建新的基本块作为关键边中间块
    auto* splitBlock = new BasicBlock();
    func->push_back(splitBlock);
    func->InsertBlock(currBlock, succBlock, splitBlock);
    splitBlock->num = ++func->num;
    func->PushBothBB(splitBlock);

    // 更新 PHI 节点，将原来的 currBlock 替换为 splitBlock
    for (auto* instIter : *succBlock) {
        auto* phi = dynamic_cast<PhiInst*>(instIter);
        if (!phi) break;

        std::unordered_map<BasicBlock*, int> bbToIndex;
        for (const auto& record : phi->PhiRecord) {
            bbToIndex[record.second.second] = record.first;
        }

        auto it = bbToIndex.find(currBlock);
        if (it == bbToIndex.end()) continue;

        int idx = it->second;
        for (auto& record : phi->PhiRecord) {
            if (record.first == idx) {
                record.second.second = splitBlock;
                break;
            }
        }
        phi->SetIncomingBlock(idx, splitBlock);
    }
}