#include "../../include/IR/Opt/LoopUnrolling.hpp"

bool LoopUnrolling::run()
{
  DominantTree dom(_func);
  dom.InitNodes();         // 初始化节点
  dom.BuildDominantTree(); // 计算支配关系
  dom.buildTree();         // 构建支配树
  LoopInfoAnalysis *loopAnalysis = new LoopInfoAnalysis(_func, &dom, DeleteLoop);
  std::vector<Loop *> loopAnalysis_Unroll{loopAnalysis->loopsBegin(), loopAnalysis->loopsEnd()};

  for (auto iter = loopAnalysis_Unroll.begin(); iter != loopAnalysis_Unroll.end();)
  {
    auto currLoop = *iter;
    ++iter;
    if (!CanBeUnroll(currLoop))
    {
      continue; // 跳过不可展开的循环
    }
    auto unrollbody = GetLoopBody(currLoop);
    if (unrollbody)
    {
      auto bb = Unroll(currLoop, unrollbody);
      // CleanUp(currLoop, bb);
      return true;
    }
  }
  return false;
}

bool LoopUnrolling::CanBeUnroll(Loop *loop)
{
  auto body = loop->getLoopBody();
  auto header = loop->getHeader();
  auto latch = loopAnalysis->getLatch(loop);
  if (header != latch)
    return false;
  if (!dynamic_cast<ConstIRInt *>(loop->trait.initial) || !dynamic_cast<ConstIRInt *>(loop->trait.boundary))
    return false;

  int Lit_count = 0;
  auto initial = dynamic_cast<ConstIRInt *>(loop->trait.initial)->GetVal();
  auto bound = dynamic_cast<ConstIRInt *>(loop->trait.boundary)->GetVal();
  auto bin = dynamic_cast<BinaryInst *>(loop->trait.change); //
  auto op = bin->GetOp();
  auto step = loop->trait.step;

  switch (op)
  {
  case BinaryInst::Op_Add:
    Lit_count = (bound - initial + step + (step > 0 ? -1 : 1)) / step;
    break;
  case BinaryInst::Op_Sub:
    Lit_count = (initial - bound + step + (step > 0 ? -1 : 1)) / step;
    break;
  case BinaryInst::Op_Mul:
    Lit_count = std::log(bound / initial) / std::log(step);
    break;
  case BinaryInst::Op_Div:
    Lit_count = std::log(initial / bound) / std::log(step);
    break;
  default:
    assert(0 && "循环展开,步长op,不支持的操作符");
  }

  int cost = CaculatePrice(body, _func, Lit_count);
  if (cost > MaxInstCost)
    return false;
  return true;
}

CallInst *LoopUnrolling::GetLoopBody(Loop *loop)
{
  // 循环分析
  auto body = loop->getLoopBody();
  auto header = loop->getHeader();
  auto latch = loop->getLatch();
  auto preheader = loop->getPreHeader();

  // 处理 Phi 指令，找到需要作为参数传入展开函数的变量
  auto phi = dynamic_cast<PhiInst *>(header->GetLastInsts());
  if (phi == nullptr || phi->getNumIncomingValues() != 2)
  {
    _DEBUG(std::cerr << "Cant Find Phi" << std::endl;)
    return nullptr;
  }

  std::vector<PhiInst *> validPhis;
  for (auto inst : *header)
  {
    auto phi = dynamic_cast<PhiInst *>(inst);
    if (!phi)
      break;
    if (inst == loop->trait.indvar)
      continue;
    validPhis.push_back(phi);
  }
  if (validPhis.size() > 2)
  {
    _DEBUG(std::cerr << "too many phi" << std::endl;)
    return nullptr;
  }
  /*  PhiInst *res = nullptr;
   int count = 0;
   for (auto inst : *header)
   {
     if (auto phi = dynamic_cast<PhiInst *>(inst))
     {
       count++;
       if (inst == loop->trait.indvar) // 归纳变量
         continue;                     // 与循环条件和步长有关,无需处理
       res = phi;
       if (count > 2)
       {
         _DEBUG(std::cerr << "too many phi" << std::endl;)
         return nullptr;
       }
     }
     else
     {
       break;
     }
   } */
  loop->trait.res = validPhis[0];
  // loop->trait.indvar = dynamic_cast<PhiInst *>(header->GetLastInsts());
  // loop->trait.call = dynamic_cast<CallInst *>(header->GetLastInsts());
  //  创建新的函数用于存放展开后的循环体

  // 将 indvar 和 res 加入参数表
  // 定义 judge 函数，判断哪些值需要作为参数传递
  // 遍历循环体中的所有指令，收集参数
  // 替换 Header 块，构造调用展开函数的指令
}

BasicBlock *LoopUnrolling::Unroll(Loop *loop, CallInst *UnrollBody)
{
}

int LoopUnrolling::CaculatePrice(std::vector<BasicBlock *> body, Function *curfunc, int Lit_count)
{
  int cost = 0;
  for (auto bb : body)
  {
    for (auto inst : *bb)
    {
      if (dynamic_cast<LoadInst *>(inst) || dynamic_cast<StoreInst *>(inst))
      {
        cost += 4;
      }
      else if (dynamic_cast<GepInst *>(inst))
      {
        cost += 2;
      }
      else if (dynamic_cast<CallInst *>(inst))
      {
        auto call = dynamic_cast<Function *>(inst->GetOperand(0));
        if (call)
        {
          if (call == curfunc || call->tag == Function::ParallelBody || call->tag == Function::BuildIn)
            return MaxInstCost + 1;
          cost += CaculatePrice(body, call, Lit_count);
        }
        else
        {
          cost += 2;
        }
      }
      else
      {
        cost++;
      }
    }
  }
  return cost * Lit_count;
}
