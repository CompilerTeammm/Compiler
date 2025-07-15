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
  // 1.获取循环基本信息
  BasicBlock *header = loop->getHeader();
  if (!header)
    return false;

  // 2.检查是否为标准while结构：
  // header有两条入边（循环外入口和循环尾部）
  // 至少有一个条件分支
  if (header->GetPredBlocks().size() != 2)
  {
    std::cerr << "Loop header does not have exactly two predecessors" << std::endl;
    return false;
  }

  // 3.查找循环尾部（可能没有显式latch，需通过支配关系查找）
  BasicBlock *tail = nullptr;
  for (auto bb : loop->getLoopBody())
  {
    for (auto succ : bb->GetNextBlocks())
    {
      if (succ == header)
      {
        tail = bb;
        break;
      }
    }
    if (tail)
      break;
  }
  if (!tail)
  {
    std::cerr << "Cannot find loop tail block" << std::endl;
    return false;
  }

  // 4.Phi节点处理（放宽限制）
  // 检查phi函数结点的数量
  PhiInst *resPhi = nullptr;
  int phiCnt = 0;
  for (auto inst : *header)
  {
    if (auto phi = dynamic_cast<PhiInst *>(inst))
    {
      phiCnt++;
      if (inst == loop->trait.indvar)
        continue;
      resPhi = phi;
      // 放宽到最多3个Phi（原为2个）
      if (phiCnt > 3)
      {
        std::cerr << "Too many Phi nodes: " << phiCnt << std::endl;
        return false;
      }
    }
    else
    {
      break;
    }
  }

  // 终止条件检查,二元指令 > < >= <=
  // （循环展开和向量化可能有点用）
  auto cmp = dynamic_cast<BinaryInst *>(loop->trait.cmp);
  if (!cmp)
    return false;
  switch (cmp->GetOp())
  {
  case BinaryInst::Op_L:
  case BinaryInst::Op_G:
  case BinaryInst::Op_LE:
  case BinaryInst::Op_GE:
    break;
  default:
    return false;
  }

  // 特殊：计算一下迭代次数，后面用于判断是否进行
  calculate_iteration(loop);
  // 检查Phi节点的使用模式
  CheckPhiNodeUsage(resPhi, loop, tail);

  // 最后检查一下数据流分析
  checkDataFlowAnalysis(loop);
  return true;
}

bool LoopParing::every_LoopParallelBody(Loop *loop)
{
  return true;
}

void LoopParing::evert_thread(Value *_initial, Value *_boundary, bool every_call)
{
}

bool LoopParing::calculate_iteration(Loop *loop)
{
  auto in = dynamic_cast<ConstIRInt *>(loop->trait.initial);
  auto bou = dynamic_cast<ConstIRInt *>(loop->trait.boundary);
  if (in && bou)
  {
    auto indata = in->GetVal(), boudata = bou->GetVal();
    auto bin = dynamic_cast<BinaryInst *>(loop->trait.change);
    auto op = bin->GetOp();
    auto step = loop->trait.step;
    int iteration = 0;
    switch (op)
    {
    case BinaryInst::Op_Add:
      iteration = (boudata - indata + step + (step > 0 ? -1 : 1)) / step;
      break;
    case BinaryInst::Op_Sub:
      iteration = (indata - boudata + step + (step > 0 ? -1 : 1)) / step;
      break;
    case BinaryInst::Op_Mul:
      iteration = std::log(boudata / indata) / std::log(step);
      break;
    case BinaryInst::Op_Div:
      iteration = std::log(indata / boudata) / std::log(step);
      break;
    default:
      assert(0 && "what op?");
    }
    if (iteration <= 100)
      return false;
  }
}

// 检查Phi节点的使用模式是否符合优化要求
bool LoopParing::CheckPhiNodeUsage(PhiInst *resPhi, Loop *loop, BasicBlock *latch)
{
  // 初始化状态标记
  bool usedOutsideLoop = false; // 标记是否在循环外被使用
  bool usedInsideLoop = false;  // 标记是否在循环内被合法使用
  bool hasBinaryUse = false;    // 标记是否被二元指令使用

  // 如果没有Phi节点直接通过检查
  if (!resPhi)
    return true;

  // 获取Phi节点在latch块中的返回值
  Value *phiResult = resPhi->ReturnValIn(latch);

  // 检查返回值必须是加法指令
  BinaryInst *resultBinOp = dynamic_cast<BinaryInst *>(phiResult);
  if (!resultBinOp || resultBinOp->GetOp() != BinaryInst::Op_Add)
  {
    return false;
  }

  // 准备要忽略的指令集合（避免重复检查）
  std::set<Value *> ignoredValues = {resPhi, phiResult};

  // 检查返回值的使用情况
  for (auto use : phiResult->GetValUseList())
  {
    auto test1 = dynamic_cast<Instruction *>(use->GetUser());
    BasicBlock *userBB = test1->GetParent();

    // 检查是否在循环外使用
    if (!loop->ContainBB(userBB))
    {
      usedOutsideLoop = true;
      continue;
    }

    // 检查循环内使用是否合法
    if (ignoredValues.find(use->GetUser()) == ignoredValues.end())
    {
      BinaryInst *userBinOp = dynamic_cast<BinaryInst *>(use->GetUser());
      if (!userBinOp || userBinOp->GetOp() != BinaryInst::Op_Add)
      {
        return false; // 循环内使用必须也是加法指令
      }
      usedInsideLoop = true;
      hasBinaryUse = true;
    }
  }

  // 检查Phi节点本身的使用情况
  for (auto use : resPhi->GetValUseList())
  {
    auto test2 = dynamic_cast<Instruction *>(use->GetUser());
    BasicBlock *userBB = test2->GetParent();
    if (!loop->ContainBB(userBB))
    {
      usedOutsideLoop = true;
    }
    else if (ignoredValues.find(use->GetUser()) == ignoredValues.end())
    {
      usedInsideLoop = true;
    }
  }

  // 最终合法性判断
  if (usedOutsideLoop && usedInsideLoop)
  {
    return false; // 禁止同时在循环内外使用
  }
  if (!hasBinaryUse)
  {
    return false; // 必须被至少一个二元指令使用
  }

  return true; // 通过所有检查
}

bool LoopParing::checkDataFlowAnalysis(Loop *loop)
{
  return true;
}