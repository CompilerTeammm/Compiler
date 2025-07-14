#include "../../include/IR/Opt/LoopParing.hpp"

bool LoopParing::run()
{
  DominantTree dom(_func);
  dom.BuildDominantTree();
  auto loopTest = std::make_shared<LoopInfoAnalysis>(_func, &dom, DeleteLoop);

  // 1.����normal��main��������Ҫ��8/2ԭ��
  if (_func->tag != Function::Normal || _func->GetName() != "main")
    return false;

  // ����Phi�Ż�Pass����δ��LoopParallel֮ǰִ��,������ѭ��ͷ����Phi
  //  2.����ǰ�������phiָ�ѭ����ڼ�
  deletephi(_func);
  // 3.�Ȳ���������е�ѭ���������δ����ڲ�

  // 3.1����֧���ϵ��������lambda���ʽ
  std::vector<Loop *> loops{_loop_analysis->loopsBegin(), _loop_analysis->loopsEnd()};
  std::sort(loops.begin(), loops.end(),
            [&](Loop *a, Loop *b)
            { return _dominatortree->dominates(a->getHeader(), b->getHeader()); });
  // 3.2�����б��У������ж��Ƿ���Բ���
  // 3.5�ɲ��е�ѭ���������������ȷ����
  // 4.0�����Բ��е�ѭ��������ѭ������ȡ�������߳�thread
  makeit(loops);
  return false;
}

bool LoopParing::makeit(std::vector<Loop *> loops)
{
  for (auto loop : loops)
  {
    // 3.3ÿ��ѭ������һ�ξͿ��ԣ�����ǰ�ȼ��
    // 3.4��ѭ����������������Ҫ�ϴ�i = i - 1 ֮��ģ�
    // 3.5�ɲ��е�ѭ���������������ȷ����
    // ����canbeprallel
    if (processed_Loops.find(loop->getHeader()) == processed_Loops.end() && CanBeParallel(loop))
    {
      every_new_looptrait.Init();
      auto every_call = every_LoopParallelBody(loop);
      evert_thread(loop->trait.initial, loop->trait.boundary, every_call);
      return true;
    }
  }
  return false;
}

void LoopParing::deletephi(Function *_func)
{
  for (auto bb : *_func)
  {
    for (auto iter = bb->begin(); iter != bb->end() && dynamic_cast<PhiInst *>(*iter);)
    {
      auto phi = dynamic_cast<PhiInst *>(*iter);
      ++iter;
      // ���ȷʵ���������phiָ�val����Ϊnullptr
      Value *val = nullptr;
      for (auto &use : phi->GetUserUseList())
      {
        if (!val)
          val = use->GetValue();
        else if (val != use->GetValue())
        {
          val = nullptr;
          break;
        }
      }
      // �ܽ�ȥ��˵���������������phiָ��
      if (val)
      {
        phi->RAUW(val); // ��ͳһֵ�滻����Phi��ʹ��
        delete phi;     // ɾ��Phiָ��
      }
    }
  }
}

bool LoopParing::CanBeParallel(Loop *loop)
{

  return true;
}

bool LoopParing::every_LoopParallelBody(Loop *loop)
{
  return true;
}

void LoopParing::evert_thread(Value *_initial, Value *_boundary, bool every_call)
{
}