#include "../../include/IR/Opt/ECE.hpp"

bool ECE::run() {
  DominantTree _tree(m_func);
    _tree.BuildDominantTree();
    for (auto& bb_ptr : m_func->GetBBs()) {
        BasicBlock* B = bb_ptr.get();
        if (!B) continue;
        B->PredBlocks = _tree.getPredBBs(B);
        B->NextBlocks = _tree.getSuccBBs(B);
    }
  SplitCriticalEdges();
  return false;
}

void ECE::SplitCriticalEdges() {
  for (auto it = m_func->begin(); it != m_func->end(); ++it) {
    BasicBlock *bb = *it;
    int succ_num = bb->GetSuccessorCount();
    if (succ_num <= 1)
      continue;

    for (int i = 0; i < succ_num; i++) {
      BasicBlock *succ = bb->NextBlocks[i];
      if (succ->GetPredecessorCount() > 1) {
        AddEmptyBlock(bb->GetBack(), i);
      }
    }
  }
}

// void ECE::SplitCriticalEdges() {
//   int total_edges = 0;
//   int split_edges = 0;

//   for (auto it = m_func->begin(); it != m_func->end(); ++it) {
//     BasicBlock *bb = *it;
//     int succ_num = bb->GetSuccessorCount();
//     if (succ_num <= 1)
//       continue;

//     for (int i = 0; i < succ_num; i++) {
//       total_edges++;

//       // 调试输出当前正在检查的边
//       if (i < bb->NextBlocks.size()) {
//         std::cerr << "[ECE] Checking edge: BB" << bb->index << " -> BB" << bb->NextBlocks[i]->index << "\n";
//       }

//       BasicBlock *succ = bb->NextBlocks[i];
//       if (succ->GetPredecessorCount() > 1) {
//         std::cerr << "[ECE] Found critical edge: BB" << bb->index << " -> BB" << succ->index << "\n";
//         AddEmptyBlock(bb->GetBack(), i);
//         split_edges++;
//       }
//     }
//   }

//   std::cerr << "[ECE] Total edges: " << total_edges << ", Split critical edges: " << split_edges << "\n";
// }



void ECE::AddEmptyBlock(Instruction *inst, int succ) {
  auto CI = dynamic_cast<CondInst *>(inst);
  assert(CI && "inst transferred in must be a CondInst");

  BasicBlock *CurrBB = inst->GetParent();
  auto successors = CurrBB->GetNextBlocks();
  assert(succ < successors.size());
  BasicBlock *DstBB = successors[succ];
  assert(DstBB);

  // 这里假设调用方已经判断过是关键边，不再判断 PredNum

  BasicBlock *criticalbb = new BasicBlock();
  m_func->push_back(criticalbb);

  m_func->InsertBlock(CurrBB, DstBB, criticalbb);
  criticalbb->num = ++m_func->num;
  m_func->PushBothBB(criticalbb);

  // Phi节点更新，封装为成员函数
  for (auto iter = DstBB->begin();
       iter != DstBB->end() && dynamic_cast<PhiInst *>(*iter) != nullptr;
       ++iter) {
    auto phi = dynamic_cast<PhiInst *>(*iter);
    phi->addIncoming(CurrBB, criticalbb);
  }
}