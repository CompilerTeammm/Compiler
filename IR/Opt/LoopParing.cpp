#include "../../include/IR/Opt/LoopParing.hpp"

bool LoopParing::run()
{
  DominantTree dom(_func);
  dom.BuildDominantTree();
  auto loopTest = std::make_shared<LoopInfoAnalysis>(_func, &dom, DeleteLoop);

  // 1.处理normal和main函数，主要是8/2原则
  if (_func->tag != Function::Normal || _func->GetName() != "main")
    return false;

  // 其他Phi优化Pass可能未在LoopParallel之前执行,仅处理循环头部的Phi
  //  2.消除前面的冗余phi指令，循环入口简化
  deletephi(_func);
  // 3.先并行外层所有的循环，再依次处理内部

  // 3.1根据支配关系排序，利用lambda表达式
  std::vector<Loop *> loops{_loop_analysis->loopsBegin(), _loop_analysis->loopsEnd()};
  std::sort(loops.begin(), loops.end(),
            [&](Loop *a, Loop *b)
            { return _dominatortree->dominates(a->getHeader(), b->getHeader()); });
  // 3.2在新列表中，依次判断是否可以并行
  // 3.5可并行的循环，其迭代次数是确定的
  // 4.0将可以并行的循环，进行循环体提取，创建线程thread
  makeit(loops);
  return false;
}

bool LoopParing::makeit(std::vector<Loop *> loops)
{
  for (auto loop : loops)
  {
    // 3.3每个循环并行一次就可以，处理前先检查
    // 3.4无循环依赖（不存在需要上次i = i - 1 之类的）
    // 3.5可并行的循环，其迭代次数是确定的
    // 面向canbeprallel
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
      // 如果确实是有意义的phi指令，val最终为nullptr
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
      // 能进去，说明最终是无意义的phi指令
      if (val)
      {
        phi->RAUW(val); // 用统一值替换所有Phi的使用
        delete phi;     // 删除Phi指令
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