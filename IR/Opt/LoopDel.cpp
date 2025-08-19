#include "../../include/IR/Opt/LoopDel.hpp"

bool LoopDeletion::run()
{
  bool changed = false;
  dom = AM.get<DominantTree>(func);
  loop = AM.get<LoopAnalysis>(func, dom, std::ref(DeleteLoop));
  AM.get<SideEffect>(&Singleton<Module>());
  for (auto loop_ : loop->GetLoops())
    changed |= DetectDeadLoop(loop_);
  changed |= DeleteUnReachable();
  return changed;
}

bool LoopDeletion::DetectDeadLoop(LoopInfo *loopInfo)
{
  bool modified = false;
  if (!loop->GetPreHeader(loopInfo))
    return false;

  if (!loopInfo->GetSubLoop().empty())
    return false;

  if (loop->GetExit(loopInfo).size() != 1)
    return false;

  if (CanBeDelete(loopInfo))
    modified |= TryDeleteLoop(loopInfo);
  return modified;
}

bool LoopDeletion::CanBeDelete(LoopInfo *loopInfo)
{
  BasicBlock *ExitBlock = loop->GetExit(loopInfo)[0];

  Value *CommonValue;
  bool flag_common = true;
  bool flag_invariant = true;
  auto inst = ExitBlock->begin();
  while (auto Phi = dynamic_cast<PhiInst *>(*inst))
  {
    std::vector<BasicBlock *> exitingBlocks = loop->GetExitingBlock(loopInfo);
    Value *firstval = Phi->ReturnValIn(exitingBlocks[0]);

    flag_common = std::all_of(exitingBlocks.begin(), exitingBlocks.end(),
                              [Phi, firstval](BasicBlock *block)
                              {
                                return Phi->ReturnValIn(block) == firstval;
                              });

    if (!flag_common)
      break;

    if (auto Inst_Com = dynamic_cast<Instruction *>(firstval))
    {
      if (makeLoopInvariant(Inst_Com, loopInfo,
                            loop->GetPreHeader(loopInfo)->GetBack()))
        Phi->ModifyBlock(exitingBlocks[0], loop->GetPreHeader(loopInfo));
      else
      {
        flag_invariant = false;
        break;
      }
    }
    ++inst;
  }

  if (!flag_common || !flag_invariant)
    return false;

  for (User *inst : *loopInfo->GetHeader())
  {
    if (inst->HasSideEffect())
      return false;
  }

  for (BasicBlock *block : loopInfo->GetLoopBody())
  {
    for (Instruction *inst : *block)
    {
      if (inst->HasSideEffect())
        return false;
    }
  }

  for (BasicBlock *block : loop->GetExitingBlock(loopInfo))
  {
    for (Instruction *inst : *block)
    {
      if (inst->HasSideEffect())
        return false;
    }
  }

  for (Instruction *inst : *loop->GetExit(loopInfo)[0])
  {
    if (inst->HasSideEffect())
      return false;
  }

  return true;
}

// ww改了很多
bool LoopDeletion::isLoopInvariant(std::set<BasicBlock *> &blocks, Instruction *inst)
{
  for (const auto &use : inst->GetValUseList())
  {
    auto *userInst = dynamic_cast<Instruction *>(use->GetUser());
    if (!userInst)
      continue;
    if (userInst == inst)
      continue;
    if (blocks.count(userInst->GetParent()))
      return false;
  }
  return true;
}

bool LoopDeletion::makeLoopInvariant(Instruction *inst, LoopInfo *loopinfo,
                                     Instruction *Termination)
{
  std::set<BasicBlock *> contain{loopinfo->GetLoopBody().begin(),
                                 loopinfo->GetLoopBody().end()};
  bool flag = false;
  if (!IsSafeToMove(inst))
    flag = false;
  if (isLoopInvariant(contain, inst))
    flag = true;
  if (!flag)
    return false;
  for (auto &use : inst->GetUserUseList())
  {
    if (use->usee == inst && dynamic_cast<PhiInst *>(inst))
    {
      auto phi = dynamic_cast<PhiInst *>(inst);
      if (phi->PhiRecord.size() == 2)
      {
        Value *val = nullptr;
        for (auto &[key, value] : phi->PhiRecord)
        {
          if (value.first != inst)
          {
            val = value.first;
            break;
          }
        }
        if (val)
        {
          phi->ReplaceAllUseWith(val);
        }
      }
      continue;
    }

    if (!makeLoopInvariant_val(use->usee, loopinfo, Termination))
      return false;
  }

  return true;
}

bool LoopDeletion::makeLoopInvariant_val(Value *val, LoopInfo *loopinfo,
                                         Instruction *Termination)
{
  if (auto inst = dynamic_cast<Instruction *>(val))
    return makeLoopInvariant(inst, loopinfo, Termination);
  return true;
}

bool LoopDeletion::TryDeleteLoop(LoopInfo *loopInfo)
{
  BasicBlock *Header = loopInfo->GetHeader();
  BasicBlock *Pre_Header = loop->GetPreHeader(loopInfo);
  BasicBlock *exitblock = loop->GetExit(loopInfo)[0];
  std::vector<BasicBlock *> exiting_blocks = loop->GetExitingBlock(loopInfo);
  Instruction *Pre_Header_Exit = Pre_Header->GetBack();
  for (auto &use : Pre_Header_Exit->GetUserUseList())
  {
    auto exit = dynamic_cast<BasicBlock *>(use->usee);
    if (exit && exit == Header)
    {
      Pre_Header_Exit->ReplaceSomeUseWith(use.get(), exitblock);
    }
  }

  // handle exit phi
  auto exit_iter = exitblock->begin();
  while (auto phi = dynamic_cast<PhiInst *>(*exit_iter))
  {
    phi->ModifyBlock(exiting_blocks[0], Pre_Header);
    for (int i = 1; i < exiting_blocks.size(); i++)
    {
      phi->removeIncomingFrom(exiting_blocks[i]);
    }
    ++exit_iter;
  }

  Function *func_ = Pre_Header->GetParent();
  for (BasicBlock *block : loopInfo->GetLoopBody())
  {
    delete block;
  }
  loop->DeleteLoop(loopInfo);
  return true;
}

bool LoopDeletion::IsSafeToMove(Instruction *inst)
{
  auto instid = inst->GetInstId();
  if (instid == Instruction::Op::Load)
    return false;

  if (instid == Instruction::Op::Call)
  {
    if (inst->HasSideEffect())
      return false;
  }

  if (instid == Instruction::Op::Alloca || instid == Instruction::Op::Phi ||
      instid == Instruction::Op::Store || instid == Instruction::Op::Ret ||
      instid == Instruction::Op::UnCond || instid == Instruction::Op::Cond)
    return false;
  return true;
}

bool LoopDeletion::DeleteUnReachable()
{
  bool changed = false;
  std::vector<BasicBlock *> Erase;
  std::set<BasicBlock *> visited;
  std::vector<BasicBlock *> assist;
  assist.push_back(func->GetFront());
  visited.insert(func->GetFront());
  while (!assist.empty())
  {
    auto bb = PopBack(assist);
    auto condition = bb->GetBack();
    if (auto cond = dynamic_cast<CondInst *>(condition))
    {
      auto nxt_1 = dynamic_cast<BasicBlock *>(cond->GetOperand(1));
      auto nxt_2 = dynamic_cast<BasicBlock *>(cond->GetOperand(2));
      if (visited.insert(nxt_1).second)
        assist.push_back(nxt_1);
      if (visited.insert(nxt_2).second)
        assist.push_back(nxt_2);
    }
    else if (auto uncond = dynamic_cast<UnCondInst *>(condition))
    {
      auto nxt = dynamic_cast<BasicBlock *>(uncond->GetOperand(0));
      if (visited.insert(nxt).second)
        assist.push_back(nxt);
    }
  }
  for (auto it = func->begin(); it != func->end();)
  {
    auto bb = *it;
    ++it;
    if (visited.find(bb) == visited.end())
    {
      DeletDeadBlock(bb);
      changed = true;
    }
  }
  return changed;
}

void LoopDeletion::DeletDeadBlock(BasicBlock *bb)
{
  auto node = dom->getNode(bb);

  // 遍历后继节点，移除当前 bb 的前驱引用
  for (auto succNode : node->succNodes)
  {
    BasicBlock *succ = succNode->curBlock;
    succ->RemovePredBB(bb);
  }

  // 替换指令的所有引用为 Undef
  for (auto iter = bb->begin(); iter != bb->end();)
  {
    Instruction *inst = *iter;
    ++iter;
    inst->ReplaceAllUseWith(UndefValue::Get(inst->GetType()));
  }

  // 更新支配树前驱/后继关系
  updateDTinfo(bb);

  // 删除该基本块
  delete bb;

  // 更新 block 数量
  dom->BBsNum--;
}

void LoopDeletion::updateDTinfo(BasicBlock *bb)
{
  auto node = dom->getNode(bb);

  // 从所有前驱的 succNodes 中移除当前节点
  for (auto predNode : node->predNodes)
  {
    predNode->succNodes.remove(node);
  }

  // 从所有后继的 predNodes 中移除当前节点
  for (auto succNode : node->succNodes)
  {
    succNode->predNodes.remove(node);
  }
}
