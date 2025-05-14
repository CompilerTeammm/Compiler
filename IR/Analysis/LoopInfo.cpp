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
  // ����һ��֧����
  DominantTree dt(&F);
  dt.BuildDominantTree();
  // ͨ����������CFG������ȡÿ��������ĺ������
  // Ȼ��ͨ��֧����������ÿ���������ǰ����ͨ��forѭ�����ң����ж�ǰ������Щ��֧�����������
  // ����ҵ�ǰ�����Ǿͽ�curbb�洢��latch�У�Ҳ����Ŀǰ���ڱ����Ļ�����
  // ͨ�����ַ�����������ͬ��latchǰ������
  //
  for (auto curbb : PostOrder)
  {
    std::vector<BasicBlock *> latch;            // �ر��б�
    for (auto succbb : _dom->getPredBBs(curbb)) // ����ÿ���������ǰ��
    {
      if (_dom->dominates(curbb, succbb)) // ���curbb֧��succbb
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

        // ������������飬�Ƿ��б��ѭ��
        if (auto iter = Loops.find(bb); iter != Loops.end())
        {
          // �ҵ�Ƕ��ѭ���������
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
