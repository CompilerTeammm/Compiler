#include "../../include/IR/Opt/ConstantHoist.hpp"

bool ConstantHoist::run()
{
  bool modified = false;
  for (BasicBlock *block : *func)
    modified |= RunOnBlock(block);
  return modified;
}
bool ConstantHoist::RunOnBlock(BasicBlock *block)
{
  bool modified = false;
  Instruction *Terminal = block->GetBack();
  if (!dynamic_cast<CondInst *>(Terminal))
    return false;
  CondInst *Br = dynamic_cast<CondInst *>(Terminal);
  BasicBlock *TrueBlock = dynamic_cast<BasicBlock *>(Br->GetOperand(1));
  BasicBlock *FalseBlock = dynamic_cast<BasicBlock *>(Br->GetOperand(2));

  if (TrueBlock == FalseBlock || TrueBlock == block || FalseBlock == block)
    return false;
  if (TrueBlock->GetValUseList().GetSize() != 1 || FalseBlock->GetValUseList().GetSize() != 1)
    return false;
  if (TrueBlock->Size() != FalseBlock->Size())
    return false;

  // 新增：严格菱形检查（两侧的 terminator 必须是 UnCond 且指向同一个 Merge）
  Instruction *TTerm = TrueBlock->GetBack();
  Instruction *FTerm = FalseBlock->GetBack();
  auto *TUn = dynamic_cast<UnCondInst *>(TTerm);
  auto *FUn = dynamic_cast<UnCondInst *>(FTerm);
  if (!TUn || !FUn)
    return false;
  BasicBlock *Merge = TUn->GetOperand(0)->as<BasicBlock>();
  if (!Merge || Merge != FUn->GetOperand(0)->as<BasicBlock>())
    return false;

  // 如果任一边是 return（或其他直接终止函数的指令），放弃（非常危险）
  if (dynamic_cast<RetInst *>(TTerm) || dynamic_cast<RetInst *>(FTerm))
    return false;

  if (!HoistInstInBlock(TrueBlock, FalseBlock))
    return false;

  // cmp 指令
  Instruction *cmp = Br->GetOperand(0)->as<Instruction>();
  while (!HoistList.empty())
  {
    modified = true;
    HoistNode *node = HoistList.back();
    HoistList.pop_back();
    Instruction *LHS_Inst = node->LHS_Inst;
    Value *LHS = node->LHS;
    Instruction *RHS_Inst = node->RHS_Inst;
    Value *RHS = node->RHS;
    int index = node->index;
    SelectInst *new_select = new SelectInst(cmp, LHS, RHS);
    BasicBlock::List<BasicBlock, Instruction>::iterator Pos(cmp);
    Pos.InsertAfter(new_select);
    LHS_Inst->ReplaceSomeUseWith(index, new_select);
    RHS_Inst->ReplaceSomeUseWith(index, new_select);
    modified = true;
  }

  BasicBlock::List<BasicBlock, Instruction>::iterator Pos(Br);
  for (auto inst = TrueBlock->begin(); inst != TrueBlock->end();)
  {
    Instruction *handle1 = *inst;
    ++inst;
    // 如果是 terminator（TrueBlock->GetBack()），跳过，不移动
    if (handle1 == TrueBlock->GetBack())
      break;
    auto handle = dynamic_cast<Instruction *>(handle1);
    handle->EraseFromManager();
    Pos.InsertBefore(handle);
  }

  // 用无条件跳转到 Merge 替换原来的条件分支（不要直接 delete Br）
  {
    BasicBlock::List<BasicBlock, Instruction>::iterator BrPos(Br);
    UnCondInst *newJmp = new UnCondInst(Merge);
    BrPos.InsertBefore(newJmp);
    delete Br;
  }

  return modified;
}

bool ConstantHoist::HoistInstInBlock(BasicBlock *TrueBlock, BasicBlock *FalseBlock)
{
  bool modified = false;
  int index = 0;
  for (auto True_iter = TrueBlock->begin(), False_iter = FalseBlock->begin();
       True_iter != TrueBlock->end() && False_iter != FalseBlock->end(); ++True_iter, ++False_iter)
  {
    Instruction *True_Inst = *True_iter;
    Instruction *False_Inst = *False_iter;
    if (True_Inst->GetInstId() != False_Inst->GetInstId())
      return false;
    if (True_Inst->GetType() != False_Inst->GetType())
      return false;
    if (True_Inst->GetUserUseList().size() != False_Inst->GetUserUseList().size())
      return false;
    InstIndex[TrueBlock][True_Inst] = index;
    InstIndex[FalseBlock][False_Inst] = index;
    index++;
  }

  // 这里,改&&了
  for (auto True_iter = TrueBlock->begin(), False_iter = FalseBlock->begin();
       True_iter != TrueBlock->end() && False_iter != FalseBlock->end(); ++True_iter, ++False_iter)
  {
    Instruction *True_Inst = *True_iter;
    Instruction *False_Inst = *False_iter;
    if (dynamic_cast<PhiInst *>(True_Inst))
      continue;
    for (int i = 0; i < True_Inst->GetUserUseList().size(); i++)
    {
      Value *True_Op = True_Inst->GetOperand(i);
      Value *False_Op = False_Inst->GetOperand(i);
      auto True_User = dynamic_cast<Instruction *>(True_Op);
      auto False_User = dynamic_cast<Instruction *>(False_Op);
      if (True_Op->GetType() != False_Op->GetType())
        return false;
      if (dynamic_cast<ConstantData *>(True_Op) && dynamic_cast<ConstantData *>(False_Op))
      {
        if (dynamic_cast<ConstantData *>(True_Op) != dynamic_cast<ConstantData *>(False_Op))
        {
          modified = true;
          HoistList.push_back(new HoistNode(True_Inst, True_Op, False_Inst, False_Op, i));
        }
      }
      else if (dynamic_cast<Var *>(True_Op) && dynamic_cast<Var *>(False_Op))
      {
        if (dynamic_cast<Var *>(True_Op) != dynamic_cast<Var *>(False_Op))
        {
          modified = true;
          HoistList.push_back(new HoistNode(True_Inst, True_Op, False_Inst, False_Op, i));
        }
      }
      else if (dynamic_cast<StoreInst *>(True_Inst) && i == 0 && True_User && False_User &&
               True_User->GetParent() != TrueBlock && False_User->GetParent() != FalseBlock)
      {
        if (True_Op != False_Op)
        {
          modified = true;
          HoistList.push_back(new HoistNode(True_Inst, True_Op, False_Inst, False_Op, i));
        }
      }
      else if (True_User && False_User && True_User->GetParent() == TrueBlock &&
               False_User->GetParent() == FalseBlock)
      {
        if (InstIndex[TrueBlock][True_User] != InstIndex[FalseBlock][False_User])
          return false;
      }
      else if (True_User && False_User &&
               (True_User->GetParent() == TrueBlock || False_User->GetParent() == FalseBlock))
        return false;
    }
  }

  // 检查 TrueBlock 的 terminator，如果是 Cond，需要确保后续块内 PHI 相同
  Instruction *True_Br = TrueBlock->GetBack();
  if (dynamic_cast<CondInst *>(True_Br))
  {
    BasicBlock *Block_t = True_Br->GetOperand(1)->as<BasicBlock>();
    BasicBlock *Block_f = True_Br->GetOperand(2)->as<BasicBlock>();
    for (Instruction *inst : *Block_t)
    {
      if (auto phi = dynamic_cast<PhiInst *>(inst))
      {
        Value *True_Value = phi->ReturnValIn(TrueBlock);
        Value *False_Value = phi->ReturnValIn(FalseBlock);
        if (True_Value != False_Value)
        {
          HoistList.clear();
          return false;
        }
      }
      else
        break;
    }
    for (Instruction *inst : *Block_f)
    {
      if (auto phi = dynamic_cast<PhiInst *>(inst))
      {
        Value *True_Value = phi->ReturnValIn(TrueBlock);
        Value *False_Value = phi->ReturnValIn(FalseBlock);
        if (True_Value != False_Value)
        {
          HoistList.clear();
          return false;
        }
      }
      else
        break;
    }
  }
  else if (dynamic_cast<UnCondInst *>(True_Br))
  {
    BasicBlock *Block_t = True_Br->GetOperand(0)->as<BasicBlock>();
    for (Instruction *inst : *Block_t)
    {
      if (auto phi = dynamic_cast<PhiInst *>(inst))
      {
        Value *True_Value = phi->ReturnValIn(TrueBlock);
        Value *False_Value = phi->ReturnValIn(FalseBlock);
        if (True_Value != False_Value)
        {
          HoistList.clear();
          return false;
        }
      }
      else
        break;
    }
  }
  return modified;
}

