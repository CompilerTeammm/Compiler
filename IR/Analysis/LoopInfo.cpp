#include "../../include/IR/Analysis/LoopInfo.hpp"

bool Loop::ContainBB(BasicBlock *bb)
{
  if (bb == Header)
    return true;
  return std::find(BBs.begin(), BBs.end(), bb) != BBs.end();
}

bool Loop::ContainLoop(Loop *loop)
{
  if (loop == LoopsHeader)
    return true;
  return std::find(Loops.begin(), Loops.end(), loop) != Loops.end();
}

void LoopInfoAnalysis::runAnalysis(Function &F, AnalysisManager &AM)
{
  // 建立一下支配树
  DominantTree dt(&F);
  dt.BuildDominantTree();
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
