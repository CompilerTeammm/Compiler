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

  // 3.1����֧���ϵ��������lambda����ʽ
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
        //phi->RAUW(val); // ��ͳһֵ�滻����Phi��ʹ��
        delete phi;     // ɾ��Phiָ��
      }
    }
  }
}

bool LoopParing::CanBeParallel(Loop *loop)
{
  // 1.��ȡѭ��������Ϣ
  BasicBlock *header = loop->getHeader();
  if (!header)
    return false;

  // 2.����Ƿ�Ϊ��׼while�ṹ��
  // header��������ߣ�ѭ������ں�ѭ��β����
  // ������һ��������֧
  if (header->GetPredBlocks().size() != 2)
  {
    std::cerr << "Loop header does not have exactly two predecessors" << std::endl;
    return false;
  }

  // 3.����ѭ��β��������û����ʽlatch����ͨ��֧���ϵ���ң�
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

  // 4.Phi�ڵ㴦�����ſ����ƣ�
  // ���phi������������
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
      // �ſ������3��Phi��ԭΪ2����
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

  // ��ֹ�������,��Ԫָ�� > < >= <=
  // ��ѭ��չ���������������е��ã�
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

  // ���⣺����һ�µ������������������ж��Ƿ����
  calculate_iteration(loop);
  // ���Phi�ڵ��ʹ��ģʽ
  CheckPhiNodeUsage(resPhi, loop, tail);

  // �����һ������������
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
  return true;
}

// ���Phi�ڵ��ʹ��ģʽ�Ƿ�����Ż�Ҫ��
bool LoopParing::CheckPhiNodeUsage(PhiInst *resPhi, Loop *loop, BasicBlock *latch)
{
  // ��ʼ��״̬���
  bool usedOutsideLoop = false; // ����Ƿ���ѭ���ⱻʹ��
  bool usedInsideLoop = false;  // ����Ƿ���ѭ���ڱ��Ϸ�ʹ��
  bool hasBinaryUse = false;    // ����Ƿ񱻶�Ԫָ��ʹ��

  // ���û��Phi�ڵ�ֱ��ͨ�����
  if (!resPhi)
    return true;

  // ��ȡPhi�ڵ���latch���еķ���ֵ
  Value *phiResult = resPhi->ReturnValIn(latch);

  // ��鷵��ֵ�����Ǽӷ�ָ��
  BinaryInst *resultBinOp = dynamic_cast<BinaryInst *>(phiResult);
  if (!resultBinOp || resultBinOp->GetOp() != BinaryInst::Op_Add)
  {
    return false;
  }

  // ׼��Ҫ���Ե�ָ��ϣ������ظ���飩
  std::set<Value *> ignoredValues = {resPhi, phiResult};

  // ��鷵��ֵ��ʹ�����
  for (auto use : phiResult->GetValUseList())
  {
    auto test1 = dynamic_cast<Instruction *>(use->GetUser());
    BasicBlock *userBB = test1->GetParent();

    // ����Ƿ���ѭ����ʹ��
    if (!loop->ContainBB(userBB))
    {
      usedOutsideLoop = true;
      continue;
    }

    // ���ѭ����ʹ���Ƿ�Ϸ�
    if (ignoredValues.find(use->GetUser()) == ignoredValues.end())
    {
      BinaryInst *userBinOp = dynamic_cast<BinaryInst *>(use->GetUser());
      if (!userBinOp || userBinOp->GetOp() != BinaryInst::Op_Add)
      {
        return false; // ѭ����ʹ�ñ���Ҳ�Ǽӷ�ָ��
      }
      usedInsideLoop = true;
      hasBinaryUse = true;
    }
  }

  // ���Phi�ڵ㱾����ʹ�����
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

  // ���պϷ����ж�
  if (usedOutsideLoop && usedInsideLoop)
  {
    return false; // ��ֹͬʱ��ѭ������ʹ��
  }
  if (!hasBinaryUse)
  {
    return false; // ���뱻����һ����Ԫָ��ʹ��
  }

  return true; // ͨ�����м��
}

bool LoopParing::checkDataFlowAnalysis(Loop *loop)
{
  return true;
}