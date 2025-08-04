#include "../../include/IR/Analysis/LoopInfo.hpp"

bool Loop::ContainBB(BasicBlock *bb)
{
  if (bb == Header)
    return true;
  return std::find(this->BBs.begin(), this->BBs.end(), bb) != this->BBs.end();
}

bool Loop::ContainLoop(Loop *loop)
{
  if (loop == Parent)
    return true;
  return std::find(Loops.begin(), Loops.end(), loop) != Loops.end();
}

void LoopInfoAnalysis::runAnalysis()
{
  // 通过后续遍历CFG，来获取每个基本块的后序遍历
  // 然后通过支配树来查找每个基本块的前驱，通过for循环查找，来判断前驱中哪些是支配这个基本块
  // 如果找到前驱，那就将curbb存储在latch中，也就是目前正在遍历的基本块
  // 通过这种方法，构建不同的latch前驱序列
  //
  for (auto curbb : PostOrder)
  {
    std::vector<BasicBlock *> latch;
    auto node = _dom->getNode(curbb);     // 回边列表
    for (auto succNode : node->predNodes) // 查找每个基本块的前驱
    {
      BasicBlock *succbb = succNode->curBlock;
      if (_dom->dominates_(curbb, succbb))
      {
        latch.push_back(succbb); // 如果是回边，加入列表
      }
    }

    if (!latch.empty())
    {
      Loop *loop = new Loop(curbb);
      std::vector<BasicBlock *> WorkList = {latch.begin(), latch.end()};

      while (!WorkList.empty())
      {
        auto bb = WorkList.back();
        WorkList.pop_back();
        auto n = _dom->getNode(bb);

        Loop *tmp = LookUp(bb);
        if (tmp == nullptr)
        {
          setLoop(bb, loop);
          if (bb == curbb)
            continue;
          loop->InsertLoopBody(bb);
          for (auto bbbb : n->predNodes)
            WorkList.push_back(bbbb->curBlock);
        }
        else
        {
          while (tmp->GetParent() != nullptr)
          {
            tmp = tmp->GetParent();
          }
          if (tmp == loop)
            continue;
          tmp->setLoopsHeader(loop);
          loop->setSubLoop(tmp);

          BasicBlock *header = tmp->getHeader();
          n = _dom->getNode(header);
          for (auto rev : n->predNodes)
          {
            if (LookUp(rev->curBlock) != tmp)
              WorkList.push_back(rev->curBlock);
          }
        }
      }
      LoopRecord.push_back(std::move(loop));

      /*  // 查找这个基本块，是否还有别的循环
       if (auto iter = Loops.find(bb); iter != Loops.end())
       {
         // 找到嵌套循环的最外层
         auto tmp = iter->second;
         while (tmp->GetLoopsHeader() != nullptr)
         {
           tmp = tmp->GetLoopsHeader();
         }
         if (tmp == loop)
           continue;
         tmp->setLoopsHeader(loop);
         loop->addLoopsBody(tmp);

         BasicBlock *header = tmp->getHeader();
         for (auto n : _dom->getPredBBs(header))
         {
           if (auto iter_ = Loops.find(header); iter != Loops.end())
             WorkList.push_back(header);
         }
       }
       else
       {
         Loops.emplace(bb, loop);
         if (bb != curbb)
         {
           loop->addLoopBody(bb);
           for (auto pred : _dom->getPredBBs(bb))
           {
             if (Loops.find(pred) == Loops.end())
             {
               WorkList.push_back(pred);
             }
           }
         }
       }
     }
     loops.push_back(std::move(loop)); */
    }
  }
}

void LoopInfoAnalysis::PostOrderDT(BasicBlock *bb)
{
  auto b = _dom->getNode(bb);
  for (auto block : b->idomChild)
  {
    if (!block->curBlock->visited)
    {
      block->curBlock->visited = true;
      PostOrderDT(block->curBlock);
    }
  }
  PostOrder.push_back(bb);
  /*  for (auto dst : _dom->getIdomVec(bb))
   {
     if (!dst->visited)
     {
       dst->visited = true;
       PostOrderDT(dst);
     }
   }
   PostOrder.push_back(bb); */
}

bool LoopInfoAnalysis::ContainsBlock(Loop *loop, BasicBlock *bb)
{
  /*   // 1. 获取循环的所有基本块（包括 header）
    std::set<BasicBlock *> loopBlocks{loop->getLoopBody().begin(), loop->getLoopBody().end()};
    loopBlocks.insert(loop->getHeader());
    // 2. 检查目标块是否在循环内
    return loopBlocks.find(bb) != loopBlocks.end(); */
  std::set<BasicBlock *> contain{loop->getLoopBody().begin(), loop->getLoopBody().end()};
  contain.insert(loop->getHeader());
  auto iter = std::find(contain.begin(), contain.end(), bb);
  if (iter == contain.end())
    return false;
  return true;
}

bool LoopInfoAnalysis::isLoopExiting(Loop *loop, BasicBlock *bb)
{
  return std::find(getExitingBlocks(loop).begin(), getExitingBlocks(loop).end(), bb) != getExitingBlocks(loop).end();
}

void LoopInfoAnalysis::getLoopDepth(Loop *loop, int depth)
{
  if (!loop->IsVisited())
  {
    loop->addLoopsDepth(depth);
    loop->setVisited(true);
    for (auto sub : loop->GetSubLoop())
      CalculateLoopDepth(sub, depth + 1);
  }
  return;
}

BasicBlock *LoopInfoAnalysis::getPreHeader(Loop *loop, Flag flag)
{
  BasicBlock *preheader = nullptr;
  if (loop->getPreHeader() != nullptr)
    return loop->getPreHeader();

  BasicBlock *header = loop->getHeader();
  for (auto pred : _dom->getNode(header)->predNodes)
  {
    // 出现前驱不属于这个循环的情况
    if (!ContainsBlock(loop, pred->curBlock))
    {
      if (preheader == nullptr)
      {
        preheader = pred->curBlock;
        continue;
      }
      if (preheader != pred->curBlock)
      {
        preheader = nullptr;
        return preheader;
      }
    }
  }
  if (preheader && flag == Strict)
  {
    for (auto des : _dom->getNode(preheader)->succNodes)
      if (des->curBlock != header)
      {
        preheader = nullptr;
        return preheader;
      }
  }
  if (preheader != nullptr)
    loop->setPreHeader(preheader);
  return preheader;
}

BasicBlock *LoopInfoAnalysis::getLoopHeader(BasicBlock *bb)
{
  auto iter = Loops.find(bb);
  if (iter == Loops.end())
    return nullptr;
  return iter->second->getHeader();
}

std::vector<BasicBlock *> LoopInfoAnalysis::getOverBlocks(Loop *loop)
{
  std::vector<BasicBlock *> workList;
  for (auto bb : loop->getLoopBody())
  {
    for (auto curbb : _dom->getSuccBBs(bb))
    {
      // if (!ContainsBlock(loop, curbb))
      // PushVecSingleVal(workList, curbb);
    }
  }
  return workList;
}

std::vector<BasicBlock *> LoopInfoAnalysis::GetExit(Loop *loop)
{
  std::vector<BasicBlock *> workList;
  for (auto bb : loop->getLoopBody())
  {
    for (auto des : _dom->getSuccBBs(bb))
    {
      BasicBlock *B = _dom->getNode(des)->curBlock;
      if (!IsLoopIncludeBB(loop, B))
        workList.push_back(B);
    }
  }
  return workList;
}

std::vector<BasicBlock *> LoopInfoAnalysis::getExitingBlocks(Loop *loop)
{
  std::vector<BasicBlock *> exit = getOverBlocks(loop);
  std::vector<BasicBlock *> exiting;
  for (auto bb : exit)
  {
    for (auto rev : _dom->getPredBBs(bb))
    {
      // if (ContainsBlock(loop, rev))
      // PushVecSingleVal(exiting, rev);
    }
  }
  return exiting;
}
BasicBlock *LoopInfoAnalysis::getLatch(Loop *loop)
{
  auto header = loop->getHeader();
  auto preheader = getPreHeader(loop);
  BasicBlock *latch = nullptr;
  for (auto rev : _dom->getPredBBs(header))
  {
    if (rev != preheader && loop->ContainBB(rev))
    {
      if (!latch)
        latch = rev;
      else
        break;
    }
  }
  return latch;
}

Loop *LoopInfoAnalysis::LookUp(BasicBlock *bb)
{
  auto iter = Loops.find(bb);
  if (iter != Loops.end())
    return iter->second;
  return nullptr;
}
void LoopInfoAnalysis::deleteBB(BasicBlock *bb)
{
  auto loop = LookUp(bb);
  if (!loop)
    return;
  while (loop)
  {
    loop->deleteBB(bb);
    loop = loop->GetParent();
  }
}

void LoopInfoAnalysis::newBB(BasicBlock *Old, BasicBlock *New)
{
  auto loop = LookUp(Old);
  if (!loop)
    return;
  while (loop)
  {
    loop->deleteBB(Old);
    loop->addLoopBody(New);
    loop = loop->GetParent();
  }
}

void LoopInfoAnalysis::setLoop(BasicBlock *bb, Loop *loop)
{
  if (Loops.find(bb) != Loops.end())
    return;
  Loops.emplace(bb, loop);
}

void LoopInfoAnalysis::deleteLoop(Loop *loop)
{
  auto parent = loop->GetParent();
  if (parent)
  {
    auto it1 = std::find(LoopRecord.begin(), LoopRecord.end(), loop);
    assert(it1 != LoopRecord.end()); // 确保存在
    parent->getLoops().erase(std::remove(parent->getLoops().begin(), parent->getLoops().end(), loop), parent->getLoops().end());
  }
  while (parent)
  {
    for (auto loopbody : loop->getLoopBody())
    {
      // 从父循环的 LoopBody 中移除当前循环的 LoopBody 成员
      auto iter = std::find(parent->getLoopBody().begin(), parent->getLoopBody().end(), loopbody);
      if (iter != parent->getLoopBody().end())
      {
        parent->getLoopBody().erase(iter);
      }
    }
    parent = parent->GetParent(); // 向上遍历所有祖先循环
  }
  auto it1 = std::find(LoopRecord.begin(), LoopRecord.end(), loop);
  assert(it1 != LoopRecord.end()); // 确保存在
  LoopRecord.erase(it1);
}

void LoopInfoAnalysis::ExpandSubLoops()
{
  for (auto loop : LoopRecord)
    for (auto subloop : loop->GetSubLoop())
      for (auto bb : subloop->getLoopBody())
      {
        loop->getLoopBody().push_back(bb);
      }
}

void LoopInfoAnalysis::LoopAnaly()
{
  for (auto lps : LoopRecord)
  {
    Loop *root = lps;
    while (root->GetParent() != nullptr)
      root = root->GetParent();
    CalculateLoopDepth(root, 1);
  }
}
void LoopInfoAnalysis::CloneToBB()
{
  for (auto loops : LoopRecord)
  {
    _deleteloop.push_back(loops);
    int loopdepth = loops->GetLoopDepth();
    loops->getHeader()->LoopDepth = loopdepth;
    for (auto contain : loops->getLoopBody())
      contain->LoopDepth = loopdepth;
  }
}

void LoopInfoAnalysis::CalculateLoopDepth(Loop *loop, int depth)
{
  if (!loop->IsVisited())
  {
    loop->addLoopsDepth(depth);
    loop->setVisited(true);
    for (auto sub : loop->GetSubLoop())
      CalculateLoopDepth(sub, depth + 1);
  }
  return;
}

bool LoopInfoAnalysis::IsLoopIncludeBB(Loop *loop, BasicBlock *bb)
{
  auto iter =
      std::find(loop->getLoopBody().begin(), loop->getLoopBody().end(), bb);
  if (iter == loop->getLoopBody().end())
    return false;
  return true;
}

void LoopInfoAnalysis::ReplBlock(BasicBlock *Old, BasicBlock *New)
{
  auto loop = LookUp(Old);
  if (!loop)
    return;
  while (loop)
  {
    loop->deleteBB(Old);
    loop->InsertLoopBody(New);
    loop = loop->GetParent();
  }
}
