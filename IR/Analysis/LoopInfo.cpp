#include "../../include/IR/Analysis/LoopInfo.hpp"

bool Loop::ContainBB(BasicBlock *bb)
{
  if (bb == Header)
    return true;
  return std::find(this->BBs.begin(), this->BBs.end(), bb) != this->BBs.end();
}

bool Loop::ContainLoop(Loop *loop)
{
  if (loop == LoopsHeader)
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
    std::vector<BasicBlock *> latch;            // 回边列表
    for (auto succbb : _dom->getPredBBs(curbb)) // 查找每个基本块的前驱
    {
      if (_dom->dominates(curbb, succbb)) // 如果curbb支配succbb
      {
        latch.push_back(succbb);
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
        auto node = _dom->getNode(bb);

        // 查找这个基本块，是否还有别的循环
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
          if (bb == curbb)
            continue;
          loop->addLoopBody(bb);
          WorkList.push_back(bb);
        }
      }
      loops.push_back(std::move(loop));
    }
  }
}

void LoopInfoAnalysis::PostOrderDT(BasicBlock *bb)
{
  if (bb->reachable)
    return;
  bb->reachable = true;
  for (auto block : _dom->getPredBBs(bb))
  {
    PostOrderDT(block);
  }
  PostOrder.push_back(bb);
}

bool LoopInfoAnalysis::ContainsBlock(Loop *loop, BasicBlock *bb)
{
  const auto &blocks = loop->getLoopBody();
  return std::find(blocks.begin(), blocks.end(), bb) != blocks.end();
}

bool LoopInfoAnalysis::isLoopExiting(Loop *loop, BasicBlock *bb)
{
  return std::find(getExitingBlocks(loop).begin(), getExitingBlocks(loop).end(), bb) != getExitingBlocks(loop).end();
}

void LoopInfoAnalysis::getLoopDepth(Loop *loop, int depth)
{
  if (loop->GetLoopsHeader() == nullptr)
    return;
  if (loop->isVisited())
    return;

  loop->setVisited();
  loop->addLoopsDepth(depth);
  for (auto l : loop->getLoops())
  {
    getLoopDepth(l, depth + 1);
  }
}

BasicBlock *LoopInfoAnalysis::getPreHeader(Loop *loop, Flag flag)
{
  if (loop->getPreHeader() != nullptr)
    return loop->getPreHeader();

  BasicBlock *Header = loop->getHeader();
  BasicBlock *preheader = nullptr;
  for (auto pred : _dom->getPredBBs(Header))
  {
    // 出现前驱不属于这个循环的情况
    if (!ContainsBlock(loop, pred))
    {
      if (preheader == nullptr)
      {
        preheader = pred;
        continue;
      }
      if (preheader != pred)
      {
        preheader = nullptr;
        return preheader;
      }
    }
  }
  if (preheader && flag == Strict)
  {
    for (auto des : _dom->getSuccBBs(preheader))
      if (des != Header)
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
      if (!ContainsBlock(loop, curbb))
        PushVecSingleVal(workList, curbb);
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
      if (ContainsBlock(loop, rev))
        PushVecSingleVal(exiting, rev);
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