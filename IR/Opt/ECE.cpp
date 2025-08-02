// #include "../../include/IR/Opt/ECE.hpp"

// bool ECE::run() {
//   SplitCriticalEdges();
//   return false;
// }

// void ECE::SplitCriticalEdges() {
//   for (auto it = m_func->begin(); it != m_func->end(); ++it) {
//     BasicBlock *bb = *it;
//     int SuccessorCount = bb->GetSuccessorCount();
//     if (SuccessorCount > 1)
//       for (int i = 0; i < SuccessorCount; i++)
//         AddEmptyBlock(bb->GetBack(), i);
//   }
// }


// void ECE::AddEmptyBlock(Instruction *inst, int succ) {
//   auto CI = dynamic_cast<CondInst *>(inst);
//   assert(CI && "inst transferred in must be a CondInst");

//   BasicBlock *DstBB =
//       dynamic_cast<BasicBlock *>(CI->GetUserUseList()[succ + 1]->GetValue());
//   assert(DstBB);
//   int PredNum = DstBB->GetPredecessorCount();
//   if (PredNum < 2) 
//     return;

//   BasicBlock *CurrBB = inst->GetParent();
//   BasicBlock *criticalbb = new BasicBlock();
//   m_func->push_back(criticalbb);

//   m_func->InsertBlock(CurrBB, DstBB, criticalbb);
//   criticalbb->num = ++m_func->num;
//   m_func->PushBothBB(criticalbb);
//   for (auto iter = DstBB->begin();
//        iter != DstBB->end() && dynamic_cast<PhiInst *>(*iter) != nullptr;
//        ++iter) {
//     auto phi = dynamic_cast<PhiInst *>(*iter);
//     auto it1 = std::find_if(
//         phi->PhiRecord.begin(), phi->PhiRecord.end(),
//         [CurrBB](const std::pair<int, std::pair<Value *, BasicBlock *>> &ele) {
//           return ele.second.second == CurrBB;
//         });
//     if (it1 == phi->PhiRecord.end())
//       continue;
//     it1->second.second = criticalbb;
//     phi->SetIncomingBlock(it1->first,criticalbb);
//   }
// }

#include "../../include/IR/Opt/ECE.hpp"

bool ECE::run() {
  SplitCriticalEdges();
  return false;
}

void ECE::SplitCriticalEdges() {
  for (auto it = m_func->begin(); it != m_func->end(); ++it) {
    BasicBlock *bb = *it;
    int SuccessorCount = bb->GetSuccessorCount();
    if (SuccessorCount > 1) {
      // 找该BB的条件跳转指令
      Instruction *termInst = bb->GetTerminator();
      auto CI = dynamic_cast<CondInst *>(termInst);
      if (!CI) continue;

      for (int i = 0; i < SuccessorCount; i++) {
        AddEmptyBlock(CI, i);
      }
    }
  }
}

void ECE::AddEmptyBlock(Instruction *inst, int succ) {
  auto CI = dynamic_cast<CondInst *>(inst);
  assert(CI && "inst transferred in must be a CondInst");

  BasicBlock *CurrBB = inst->GetParent();
  BasicBlock *DstBB =dynamic_cast<BasicBlock *>(CI->GetUserUseList()[succ + 1]->GetValue());
  assert(DstBB);

  // 关键边判断
  if (CurrBB->GetSuccessorCount() <= 1 || DstBB->GetPredecessorCount() <= 1) {
    return;  // 非关键边，不拆分
  }

  // 避免重复拆分
  std::pair<BasicBlock*, BasicBlock*> edge = {CurrBB, DstBB};
  if (splitEdges.count(edge) > 0) {
    return;
  }
  splitEdges.insert(edge);

  BasicBlock *criticalbb = new BasicBlock();
  m_func->push_back(criticalbb);

  m_func->InsertBlock(CurrBB, DstBB, criticalbb);
  criticalbb->num = ++m_func->num;
  m_func->PushBothBB(criticalbb);

  // 更新PHI节点
  for (auto iter = DstBB->begin();
       iter != DstBB->end() && dynamic_cast<PhiInst *>(*iter) != nullptr;
       ++iter) {
    auto phi = dynamic_cast<PhiInst *>(*iter);

    std::unordered_map<BasicBlock*, int> bbToIndex;
    for (const auto& record : phi->PhiRecord) {
      bbToIndex[record.second.second] = record.first;
    }

    auto it = bbToIndex.find(CurrBB);
    if (it == bbToIndex.end())
      continue;

    int idx = it->second;
    for (auto &record : phi->PhiRecord) {
      if (record.first == idx) {
        record.second.second = criticalbb;
        break;
      }
    }
    phi->SetIncomingBlock(idx, criticalbb);
  }
}