#include "../../include/IR/Analysis/LoopInfo.hpp"
#include "../../include/lib/CFG.hpp"
#include "../../include/IR/Opt/AnalysisManager.hpp"

void LoopAnalysis::Analysis() {
for (auto curbb : PostOrder) {
    std::vector<BasicBlock *> latch;
    auto node = m_dom->getNode(curbb);

    // 找到循环的latch节点
    for (auto predNode : node->predNodes) {
        BasicBlock* predBB = predNode->curBlock;
        if (m_dom->dominates(curbb, predBB))
            latch.push_back(predBB);
    }

    if (!latch.empty()) {
        LoopInfo *loop = new LoopInfo(curbb);
        std::vector<BasicBlock *> WorkLists(latch.begin(), latch.end());

        while (!WorkLists.empty()) {
            BasicBlock *bb = WorkLists.back();
            WorkLists.pop_back();
            auto n = m_dom->getNode(bb);

            LoopInfo *tmp = LookUp(bb);
            if (tmp == nullptr) {
                setLoop(bb, loop);
                if (bb == curbb)
                    continue;
                loop->InsertLoopBody(bb);

                for (auto predNode : n->predNodes) {
                    WorkLists.push_back(predNode->curBlock);
                }
            } else {
                while (tmp->GetParent() != nullptr)
                    tmp = tmp->GetParent();
                if (tmp == loop) continue;
                tmp->setParent(loop);
                loop->setSubLoop(tmp);

                BasicBlock *header = tmp->GetHeader();
                auto headerNode = m_dom->getNode(header);
                for (auto predNode : headerNode->predNodes) {
                    if (LookUp(predNode->curBlock) != tmp)
                        WorkLists.push_back(predNode->curBlock);
                }
            }
        }
        LoopRecord.push_back(std::move(loop));
    }
}
}

void LoopAnalysis::ExpandSubLoops() {
  for (auto loop : LoopRecord)
    for (auto subloop : loop->GetSubLoop())
      for (auto bb : subloop->ContainBlocks)
        PushVecSingleVal(loop->ContainBlocks, bb);
}

bool LoopAnalysis::IsLoopIncludeBB(LoopInfo *loop, int index) {
  std::set<BasicBlock *> contain{loop->ContainBlocks.begin(),
                                 loop->ContainBlocks.end()};
  contain.insert(loop->GetHeader());
  auto iter =
      std::find(contain.begin(), contain.end(), GetCorrespondBlock(index));
  if (iter == contain.end())
    return false;
  return true;
}

bool LoopAnalysis::IsLoopIncludeBB(LoopInfo *loop, BasicBlock *bb) {
  auto iter =
      std::find(loop->ContainBlocks.begin(), loop->ContainBlocks.end(), bb);
  if (iter == loop->ContainBlocks.end())
    return false;
  return true;
}

bool LoopAnalysis::IsLoopExiting(LoopInfo *loop, BasicBlock *bb) {
  auto exiting = GetExitingBlock(loop);
  auto it = std::find(exiting.begin(), exiting.end(), bb);
  if (it != exiting.end())
    return true;
  return false;
}

bool LoopInfo::Contain(BasicBlock *bb) {
  std::unordered_set<BasicBlock *> assist{this->ContainBlocks.begin(),
                                          this->ContainBlocks.end()};
  assist.insert(Header);
  if (assist.find(bb) != assist.end())
    return true;
  return false;
}

bool LoopInfo::Contain(LoopInfo *loop) {
  if (loop == this)
    return true;
  if (!loop)
    return false;
  return Contain(loop->GetParent());
}

LoopInfo *LoopAnalysis::LookUp(BasicBlock *bb) {
  auto iter = Loops.find(bb);
  if (iter != Loops.end())
    return iter->second;
  return nullptr;
}

void LoopAnalysis::setLoop(BasicBlock *bb, LoopInfo *loop) {
  if (Loops.find(bb) != Loops.end())
    return;
  Loops.emplace(bb, loop);
}

void LoopAnalysis::PostOrderDT(BasicBlock *entry) {
    auto node = m_dom->getNode(entry);
    for (auto childNode : node->idomChild) {
        BasicBlock *childBB = childNode->curBlock;
        if (!childBB->visited) {
            childBB->visited = true;
            PostOrderDT(childBB);
        }
    }
    PostOrder.push_back(entry);
}

BasicBlock *LoopAnalysis::FindLoopHeader(BasicBlock *bb) {
  auto iter = Loops.find(bb);
  if (iter == Loops.end())
    return nullptr;
  return iter->second->GetHeader();
}

void LoopAnalysis::LoopAnaly() {
  for (auto lps : LoopRecord) {
    LoopInfo *root = lps;
    while (root->GetParent() != nullptr)
      root = root->GetParent();
    CalculateLoopDepth(root, 1);
  }
}

void LoopAnalysis::CloneToBB() {
  for (auto loops : LoopRecord) {
    _DeleteLoop.push_back(loops);
    int loopdepth = loops->GetLoopDepth();
    loops->GetHeader()->LoopDepth = loopdepth;
    for (auto contain : loops->GetLoopBody())
      contain->LoopDepth = loopdepth;
  }
}

void LoopAnalysis::CalculateLoopDepth(LoopInfo *loop, int depth) {
  if (!loop->IsVisited()) {
    loop->AddLoopDepth(depth);
    loop->setVisited(true);
    for (auto sub : loop->GetSubLoop())
      CalculateLoopDepth(sub, depth + 1);
  }
  return;
}

BasicBlock *LoopAnalysis::GetPreHeader(LoopInfo *loopinfo, Flag f) {
    BasicBlock *preheader = nullptr;
    if (loopinfo->Pre_Header != nullptr)
        return loopinfo->Pre_Header;

    BasicBlock *header = loopinfo->GetHeader();
    auto headerNode = m_dom->getNode(header);

    // 遍历前驱节点
    for (auto predNode : headerNode->predNodes) {
        BasicBlock *predBB = predNode->curBlock;

        // 出现前驱不属于这个循环的情况
        if (!IsLoopIncludeBB(loopinfo, predBB)) {
            if (preheader == nullptr) {
                preheader = predBB;
                continue;
            }
            if (preheader != predBB) {
                preheader = nullptr;
                return preheader;
            }
        }
    }

    // 严格模式下检查 preheader 是否唯一跳转到 header
    if (preheader && f == Strict) {
        auto preheaderNode = m_dom->getNode(preheader);
        for (auto succNode : preheaderNode->succNodes) {
            if (succNode->curBlock != header) {
                preheader = nullptr;
                return preheader;
            }
        }
    }

    if (preheader != nullptr)
        loopinfo->setPreHeader(preheader);

    return preheader;
}


BasicBlock *LoopAnalysis::GetLatch(LoopInfo *loop) { 
    auto header = loop->GetHeader();
    auto preheader = GetPreHeader(loop);
    BasicBlock *latch = nullptr;

    auto headerNode = m_dom->getNode(header);

    // 遍历前驱节点寻找 latch
    for (auto predNode : headerNode->predNodes) {
        BasicBlock *B = predNode->curBlock;
        if (B != preheader && loop->Contain(B)) {
            if (!latch)
                latch = B;
            else
                break; // 已经找到多个 latch，退出
        }
    }

    return latch;
}


std::vector<BasicBlock *> LoopAnalysis::GetExitingBlock(LoopInfo *loopinfo) { 
    std::vector<BasicBlock *> exit = GetExit(loopinfo);
    std::vector<BasicBlock *> exiting;

    for (auto bb : exit) {
        auto node = m_dom->getNode(bb);

        // 遍历前驱节点
        for (auto predNode : node->predNodes) {
            BasicBlock *predBB = predNode->curBlock;
            if (IsLoopIncludeBB(loopinfo, predBB))
                PushVecSingleVal(exiting, predBB);
        }
    }

    return exiting;
}


std::vector<BasicBlock *> LoopAnalysis::GetExit(LoopInfo *loopinfo) {
    std::vector<BasicBlock *> workList;

    for (auto bb : loopinfo->ContainBlocks) {
        auto node = m_dom->getNode(bb);

        // 遍历后继节点
        for (auto succNode : node->succNodes) {
            BasicBlock *B = succNode->curBlock;
            if (!IsLoopIncludeBB(loopinfo, B))
                PushVecSingleVal(workList, B);
        }
    }

    return workList;
}

void LoopAnalysis::DeleteLoop(LoopInfo *loop) {
  auto parent = loop->GetParent();
  if (parent) {
    auto it = std::find(parent->GetSubLoop().begin(),
                        parent->GetSubLoop().end(), loop);
    assert(it != parent->GetSubLoop().end());
    parent->GetSubLoop().erase(it);
    while (parent) {
      for (auto loopbody : loop->GetLoopBody()) {
        auto iter = std::find(parent->GetLoopBody().begin(),
                              parent->GetLoopBody().end(), loopbody);
        if (iter != parent->GetLoopBody().end()) {
          parent->GetLoopBody().erase(iter);
        }
      }
      parent = parent->GetParent();
    }
  }
  auto it1 = std::find(LoopRecord.begin(), LoopRecord.end(), loop);
  assert(it1 != LoopRecord.end());
  LoopRecord.erase(it1);
  return;
}

void LoopAnalysis::DeleteBlock(BasicBlock *bb) {
  auto loop = LookUp(bb);
  if (!loop)
    return;
  while (loop) {
    loop->DeleteBlock(bb);
    loop = loop->GetParent();
  }
}

void LoopAnalysis::ReplBlock(BasicBlock *Old, BasicBlock *New) {
  auto loop = LookUp(Old);
  if (!loop)
    return;
  while (loop) {
    loop->DeleteBlock(Old);
    loop->InsertLoopBody(New);
    loop = loop->GetParent();
  }
}

bool LoopAnalysis::IsLoopInvariant(const std::set<BasicBlock *> &contain,
                                   Instruction *I, LoopInfo *curloop) {
  bool res =
      std::all_of(I->GetUserUseList().begin(), I->GetUserUseList().end(),
                  [curloop, &contain](auto &ele) {
                    auto val = ele->GetValue();
                    if (auto user = dynamic_cast<Instruction *>(val)) {
                      if (user->isParam())
                        return true;
                      if (contain.find(user->GetParent()) != contain.end())
                        return false;
                    }
                    return true;
                  });
  return res;
}