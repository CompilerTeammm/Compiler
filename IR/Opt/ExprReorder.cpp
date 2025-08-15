#include "../../include/IR/Opt/ExprReorder.hpp"
#include "../../include/IR/Opt/ConstantFold.hpp"
#include <algorithm>
#include <cassert>
#include <iostream>
#include <ostream>
#include <utility>
#include <vector>

template <typename T>
T PopBack(std::vector<T> &vec)
{
  T tmp = vec.back();
  vec.pop_back();
  return tmp;
}

template <typename T>
void PushVecSingleVal(std::vector<T> &vec, T v)
{
  auto iter = std::find(vec.begin(), vec.end(), v);
  if (iter != vec.end())
    return;
  vec.push_back(v);
}

template <typename T>
Value *GetOperand(T inst, int i)
{
  User *user = dynamic_cast<User *>(inst);
  assert(user);
  return user->GetUserUseList()[i]->GetValue();
}

template <typename T>
void vec_pop(std::vector<T> &vec, int &index)
{
  assert(index < vec.size() && "index can not bigger than size");
  vec[index] = vec[vec.size() - 1];
  vec.pop_back();
  index--;
}

bool ExprReorder::run()
{
  bool changed = false;
  BasicBlock *entryBB = targetFunc->GetFront();
  targetFunc->init_visited_block();
  DominantTree m_dom(targetFunc);
  m_dom.BuildDominantTree();
  TraverseCFGPostOrder(entryBB, &m_dom);

  std::reverse(PostOrderBlocks.begin(), PostOrderBlocks.end());

  BuildRankTable();

  for (auto bb : *targetFunc)
  {
    for (User *inst : *bb)
    {
      changed |= OptimizeInstruction(inst);
    }
  }

  std::vector<User *> worklist(RetryInst.begin(), RetryInst.end());
  RetryInst.clear();

  while (!worklist.empty())
  {
    User *inst = worklist.back();
    worklist.pop_back();

    RemoveDeadInstTrival(inst, worklist);

    if (!RemoveDeadInst(inst, worklist))
    {
      OptimizeInstruction(inst);
    }
  }

  return changed;
}

void ExprReorder::BuildRankTable()
{
  int rank = 2;

  for (auto &param : targetFunc->GetParams())
  {
    ValueOrder[param.get()] = rank++;
  }

  const int SPECIAL_RANK = 99999;
  for (auto bb : PostOrderBlocks)
  {
    for (auto inst : *bb)
    {
      if (dynamic_cast<PhiInst *>(inst) || dynamic_cast<CallInst *>(inst) ||
          dynamic_cast<LoadInst *>(inst) || dynamic_cast<StoreInst *>(inst) ||
          dynamic_cast<RetInst *>(inst))
      {
        ValueOrder[inst] = SPECIAL_RANK;
      }
    }
  }
}

int ExprReorder::GetValueOrder(Value *val)
{
  if (!dynamic_cast<User *>(val))
  {
    auto it =
        std::find_if(targetFunc->GetParams().begin(), targetFunc->GetParams().end(),
                     [val](const auto &ele)
                     { return ele.get() == val; });
    if (it != targetFunc->GetParams().end())
      return ValueOrder[val];
    else
      return 0;
  }
  if (ValueOrder.find(val) != ValueOrder.end())
    return ValueOrder[val];
  int ra = 0;
  User *inst = dynamic_cast<User *>(val);
  for (auto &u : inst->GetUserUseList())
  {
    if (ra > 10000)
      break;
    ra = std::max(ra, GetValueOrder(u->GetValue()));
  }

  ValueOrder[val] = ++ra;
  return ra;
}

bool ExprReorder::OptimizeInstruction(Value *val)
{
  auto binInst = dynamic_cast<BinaryInst *>(val);
  if (!binInst || binInst->IsCmpInst() || binInst->IsAtomic())
    return false;

  if (IsOpCommutative(binInst->GetOp()))
    StandardizeBinaryInst(binInst);
  else
    return false;

  if (IsBinaryFloatOp(binInst))
    return false;

  if (binInst->GetUserUseListSize() == 1)
  {
    auto userVal = binInst->GetUserUseList().front()->GetUser();
    auto binUser = dynamic_cast<BinaryInst *>(userVal);
    if (binUser && binUser->GetOp() == binInst->GetOp())
    {
      PushVecSingleVal(RetryInst, static_cast<User *>(binUser));
      return false;
    }
  }
  return ReorderBinaryExpr(binInst);
}

bool ExprReorder::ReorderBinaryExpr(BinaryInst *binInst)
{
  std::vector<std::pair<Value *, int>> leafList;
  std::vector<std::pair<Value *, int>> valWithRank;

  FlattenExpression(binInst, leafList);

  for (auto &[val, count] : leafList)
    for (int i = 0; i < count; ++i)
      valWithRank.push_back({val, GetValueOrder(val)});

  const int originalSize = valWithRank.size();

  std::stable_sort(valWithRank.begin(), valWithRank.end(),
                   [](const auto &a, const auto &b)
                   { return a.second > b.second; });

  if (Value *newVal = OptimizeExpr(binInst, valWithRank))
  {
    if (newVal != binInst)
    {
      binInst->ReplaceAllUseWith(newVal);
      PushVecSingleVal(RetryInst, static_cast<User *>(binInst));
      return true;
    }
    return false;
  }

  if (valWithRank.size() == 1 && valWithRank[0].first != binInst)
  {
    binInst->ReplaceAllUseWith(valWithRank[0].first);
    PushVecSingleVal(RetryInst, static_cast<User *>(binInst));
    return true;
  }

  if (originalSize == valWithRank.size())
    return false;

  return RewriteExpression(binInst, valWithRank);
}

bool ExprReorder::RewriteExpression(
    BinaryInst *exp, std::vector<std::pair<Value *, int>> &linearizedOps)
{
  std::set<Value *> notWritable;
  std::vector<Value *> rewriteStack;
  BinaryInst *curr = exp;
  Instruction *changed = nullptr;

  for (auto &ele : linearizedOps)
    notWritable.insert(ele.first);

  for (size_t i = 0; i < linearizedOps.size(); ++i)
  {
    Value *val1 = linearizedOps[i].first;
    Value *oldVal1 = GetOperand(curr, 0);

    if (i + 2 == linearizedOps.size())
    {
      Value *val2 = linearizedOps[i + 1].first;
      Value *oldVal2 = GetOperand(curr, 1);

      if (val1 == oldVal1 && val2 == oldVal2)
        break;

      if (val1 != oldVal1)
      {
        if (!notWritable.count(oldVal1) &&
            IsOperandAssociative(oldVal1, curr->GetOp()))
          rewriteStack.push_back(oldVal1);
        curr->SetOperand(0, val1);
      }
      if (val2 != oldVal2)
      {
        if (!notWritable.count(oldVal2) &&
            IsOperandAssociative(oldVal2, curr->GetOp()))
          rewriteStack.push_back(oldVal2);
        curr->SetOperand(1, val2);
      }

      changed = curr;
      break;
    }

    Value *oldValRight = GetOperand(curr, 1);
    if (val1 != oldValRight)
    {
      if (val1 == oldVal1)
        std::swap(curr->GetUserUseList()[0], curr->GetUserUseList()[1]);
      else
      {
        if (IsOperandAssociative(oldValRight, exp->GetOp()))
          rewriteStack.push_back(oldValRight);
        curr->SetOperand(1, val1);
      }
    }

    Value *leftOp = GetOperand(curr, 0);
    if (IsOperandAssociative(leftOp, exp->GetOp()) && !notWritable.count(leftOp))
    {
      curr = dynamic_cast<BinaryInst *>(leftOp);
      continue;
    }

    // 从栈中弹出需要重写的节点
    Value *newVal = nullptr;
    if (!rewriteStack.empty())
    {
      newVal = rewriteStack.back();
      rewriteStack.pop_back();
    }
    else
    {
      assert(false && "Rewrite stack unexpectedly empty");
    }

    curr->SetOperand(0, newVal);
    curr = dynamic_cast<BinaryInst *>(newVal);
    changed = curr;
  }

  if (changed)
  {
    auto bbInsts = exp->GetParent()->begin();
    for (auto iter = bbInsts; iter != exp->GetParent()->end();)
    {
      if (*iter == exp)
      {
        if (changed != exp)
        {
          changed->EraseFromManager();
          iter.InsertBefore(changed);
          changed = static_cast<Instruction *>(changed->GetUserUseList().front()->GetUser());
        }
        break;
      }
      ++iter;
    }
  }

  for (auto &redoVal : rewriteStack)
    PushVecSingleVal(RetryInst, static_cast<User *>(redoVal));

  PushVecSingleVal(RetryInst, static_cast<User *>(exp));
  return true;
}

Value *ExprReorder::OptimizeExpr(BinaryInst *exp,
                                 std::vector<std::pair<Value *, int>> &linearOps)
{
  ConstantData *accumConst = nullptr;

  while (!linearOps.empty())
  {
    Value *val = linearOps.back().first;
    auto constVal = dynamic_cast<ConstantData *>(val);
    if (!constVal)
      break;

    linearOps.pop_back();
    if (!accumConst)
      accumConst = constVal;
    else
      accumConst = ConstantFold::ConstFoldBinaryOps(exp, accumConst, constVal);
  }

  if (linearOps.empty())
    return accumConst;

  if (accumConst && !ShouldIgnoreConstant(exp->GetOp(), accumConst))
  {
    if (CanAbsorbConstant(exp->GetOp(), accumConst))
      exp->ReplaceAllUseWith(accumConst);
    linearOps.push_back({accumConst, 0});
  }

  if (linearOps.size() == 1)
    return linearOps[0].first;

  size_t oldSize = linearOps.size();
  Value *ret = nullptr;

  switch (exp->GetOp())
  {
  case BinaryInst::Op_Add:
    ret = OptimizeAdd(exp, linearOps);
    break;
  case BinaryInst::Op_Mul:
    ret = OptimizeMul(exp, linearOps);
    break;
  default:
    break;
  }

  if (linearOps.size() != oldSize)
    return OptimizeExpr(exp, linearOps);

  return ret;
}

Value *ExprReorder::OptimizeMul(BinaryInst *MulInst,
                                std::vector<std::pair<Value *, int>> &LinerizedOp)
{
  return nullptr;
}

bool ExprReorder::IsOperandAssociative(Value *val, BinaryInst::Operation opcode)
{
  auto binInst = dynamic_cast<BinaryInst *>(val);
  if (!binInst)
    return false;

  if (binInst->GetOp() != opcode)
    return false;

  const auto &users = binInst->GetUserUseList();
  if (users.size() != 1)
    return false;

  Value *userVal = users[0]->GetValue();
  if (!userVal)
    return false;

  // 浮点类型不处理以避免精度丢失
  if (userVal->GetTypeEnum() == IR_Value_Float)
    return false;

  return true;
}

void ExprReorder::FlattenExpression(BinaryInst *root,
                                    std::vector<std::pair<Value *, int>> &leafList)
{
  std::map<Value *, int> leafWeightMap;
  std::set<Value *> visitedLeaves;
  std::vector<std::pair<Value *, int>> worklist{{root, 1}};
  auto rootOp = root->GetOp();

  while (!worklist.empty())
  {
    auto [currentVal, weight] = worklist.back();
    worklist.pop_back();

    for (int i = 0; i < 2; ++i)
    {
      Value *operand = GetOperand(currentVal, i);

      if (IsOperandAssociative(operand, rootOp))
      {
        worklist.emplace_back(operand, weight);
        continue;
      }

      auto insertResult = visitedLeaves.insert(operand);
      if (insertResult.second)
      {
        leafWeightMap[operand] = weight;
      }
      else
      {
        leafWeightMap[operand] += weight;
      }
    }
  }

  leafList.clear();
  for (Value *val : visitedLeaves)
  {
    auto it = leafWeightMap.find(val);
    assert(it != leafWeightMap.end() && "Leaf weight not found");
    leafList.emplace_back(val, it->second);
  }

  assert(!leafList.empty() && "Flattened leaf list cannot be empty");
}

void ExprReorder::TraverseCFGPostOrder(BasicBlock *bb, DominantTree *domTree)
{
  if (!bb || bb->visited)
    return;

  bb->visited = true;

  auto *node = domTree->getNode(bb);
  if (!node)
    return;

  for (auto *childNode : node->idomChild)
  {
    if (childNode && childNode->curBlock)
      TraverseCFGPostOrder(childNode->curBlock, domTree);
  }

  PostOrderBlocks.push_back(bb);
}

bool ExprReorder::IsOpCommutative(BinaryInst::Operation opcode)
{
  switch (opcode)
  {
  default:
    return false;
  case BinaryInst::Op_Add:
  case BinaryInst::Op_E:
  case BinaryInst::Op_Mul:
  case BinaryInst::Op_And:
  case BinaryInst::Op_Or:
    return true;
  }
}

void ExprReorder::StandardizeBinaryInst(User *inst)
{
  auto *bin = dynamic_cast<BinaryInst *>(inst);
  assert(bin && "Expected a BinaryInst");

  Value *lhs = GetOperand(bin, 0);
  Value *rhs = GetOperand(bin, 1);

  if (dynamic_cast<ConstantData *>(rhs))
    return;

  int lhsRank = GetValueOrder(lhs);
  int rhsRank = GetValueOrder(rhs);

  if (dynamic_cast<ConstantData *>(lhs) || lhsRank > rhsRank)
  {
    std::swap(bin->GetUserUseList()[0], bin->GetUserUseList()[1]);
  }
}

bool ExprReorder::ShouldIgnoreConstant(BinaryInst::Operation op,
                                       ConstantData *constData)
{
  if (!constData)
    return false;

  // 尝试获取整数或浮点值
  if (auto intConst = dynamic_cast<ConstIRInt *>(constData))
  {
    switch (op)
    {
    case BinaryInst::Op_Add:
    case BinaryInst::Op_And:
      return intConst->GetVal() == 0;
    case BinaryInst::Op_Mul:
      return intConst->GetVal() == 1;
    default:
      return false;
    }
  }
  else if (auto floatConst = dynamic_cast<ConstIRFloat *>(constData))
  {
    switch (op)
    {
    case BinaryInst::Op_Add:
    case BinaryInst::Op_And:
      return floatConst->GetVal() == 0.0;
    case BinaryInst::Op_Mul:
      return floatConst->GetVal() == 1.0;
    default:
      return false;
    }
  }

  return false;
}

bool ExprReorder::CanAbsorbConstant(BinaryInst::Operation op,
                                    ConstantData *constData)
{
  if (!constData)
    return false;

  if (op != BinaryInst::Op_Mul)
    return false;

  if (auto intConst = dynamic_cast<ConstIRInt *>(constData))
    return intConst->GetVal() == 0;

  if (auto floatConst = dynamic_cast<ConstIRFloat *>(constData))
    return floatConst->GetVal() == 0.0;

  return false;
}

bool ExprReorder::IsBinaryFloatOp(BinaryInst *I)
{
  if (I->GetType()->GetTypeEnum() == IR_Value_Float)
  {
    return true;
  }
  return false;
}

Value *ExprReorder::OptimizeAdd(BinaryInst *AddInst,
                                std::vector<std::pair<Value *, int>> &LinerizedOp)
{
  for (int i = 0; i < LinerizedOp.size(); i++)
  {
    if (i + 1 < LinerizedOp.size() && LinerizedOp[i + 1] == LinerizedOp[i])
    {
      int same = 1;
      int index = i;
      Value *src = LinerizedOp[i].first;

      while (LinerizedOp[index + 1] == LinerizedOp[index])
      {
        same++;
        index++;
      }

      if (same < 3)
        return nullptr;

      BinaryInst *Mul = nullptr;
      auto typeEnum = AddInst->GetUserUseList()[0]->GetValue()->GetTypeEnum();
      if (typeEnum == IR_Value_Float)
        Mul = BinaryInst::CreateInst(src, BinaryInst::Op_Mul, ConstIRFloat::GetNewConstant((float)same));
      else if (typeEnum == IR_Value_INT)
        Mul = BinaryInst::CreateInst(src, BinaryInst::Op_Mul, ConstIRInt::GetNewConstant(same));
      else
        assert(0);

      LinerizedOp.emplace_back(Mul, GetValueOrder(Mul));

      for (auto it = AddInst->GetParent()->begin(); it != AddInst->GetParent()->end(); ++it)
      {
        if (*it == AddInst)
        {
          it.InsertBefore(Mul);
          break;
        }
      }

      LinerizedOp.erase(LinerizedOp.begin() + i, LinerizedOp.begin() + i + same);
      i--;
      PushVecSingleVal(RetryInst, dynamic_cast<User *>(Mul));

      if (LinerizedOp.empty())
        return Mul;
    }
  }

  int Max = 0;
  Value *Maxval = nullptr;
  std::map<Value *, int> RecordAccurance;

  for (int i = 0; i < LinerizedOp.size(); i++)
  {
    if (!IsOperandAssociative(LinerizedOp[i].first, BinaryInst::Op_Mul))
      continue;

    std::vector<Value *> op;
    SplitOpRe(LinerizedOp[i].first, op);
    std::set<Value *> visited;

    for (auto _val : op)
    {
      if (visited.insert(_val).second)
      {
        int occ = ++RecordAccurance[_val];
        if (occ > Max)
        {
          Max = occ;
          Maxval = _val;
        }
      }
    }
  }

  if (Max > 1)
  {
    std::vector<Value *> AddOperands;
    for (int i = 0; i < LinerizedOp.size(); i++)
    {
      if (!IsOperandAssociative(LinerizedOp[i].first, BinaryInst::Op_Mul))
        continue;

      std::vector<std::pair<Value *, int>> Leaf, divisor;
      auto bin = dynamic_cast<BinaryInst *>(LinerizedOp[i].first);
      assert(bin);

      FlattenExpression(bin, Leaf);
      for (auto &[_val, times] : Leaf)
        for (int j = 0; j < times; j++)
          divisor.emplace_back(_val, GetValueOrder(_val));

      bool HaveDiv = false;
      for (int j = 0; j < divisor.size(); j++)
      {
        if (divisor[j].first == Maxval)
        {
          vec_pop(divisor, j);
          HaveDiv = true;
          break;
        }
      }

      if (!HaveDiv)
        continue;

      vec_pop(LinerizedOp, i);

      if (divisor.size() == 1)
        AddOperands.push_back(divisor[0].first);
      else
      {
        RewriteExpression(bin, divisor);
        AddOperands.push_back(bin);
      }
    }

    auto add = CAExpr(AddInst, AddOperands);
    auto add2mul = BinaryInst::CreateInst(add, BinaryInst::Op_Mul, Maxval, AddInst);
    PushVecSingleVal(RetryInst, dynamic_cast<User *>(add));
    PushVecSingleVal(RetryInst, dynamic_cast<User *>(add2mul));

    if (LinerizedOp.empty())
      return add2mul;

    LinerizedOp.emplace_back(add2mul, GetValueOrder(add2mul));
  }

  return nullptr;
}

Value *ExprReorder::CAExpr(User *Inst, std::vector<Value *> &AddOperands)
{
  if (AddOperands.empty())
    return nullptr;

  if (AddOperands.size() == 1)
    return AddOperands[0];

  Value *lhs = PopBack(AddOperands);
  Value *rhs = CAExpr(Inst, AddOperands);

  return BinaryInst::CreateInst(lhs, BinaryInst::Op_Add, rhs, Inst);
}

void ExprReorder::SplitOpRe(Value *I, std::vector<Value *> &ops)
{
  auto bin = dynamic_cast<BinaryInst *>(I);
  if (!bin || bin->GetOp() != BinaryInst::Op_Mul || bin->GetUserUseListSize() != 1)
  {
    ops.push_back(I);
    return;
  }

  SplitOpRe(GetOperand(bin, 0), ops);
  SplitOpRe(GetOperand(bin, 1), ops);
}

bool ExprReorder::RemoveDeadInst(User *I, int idx)
{
  if (I->GetUserUseListSize() == 0)
  {
    vec_pop(RetryInst, idx);
    delete I;
    return true;
  }
  return false;
}

bool ExprReorder::RemoveDeadInst(User *inst, std::vector<User *> &kill)
{
  if (inst->GetUserUseListSize() == 0)
  {
    for (auto &use : inst->GetUserUseList())
    {
      if (auto userVal = dynamic_cast<User *>(use->GetValue()))
        PushVecSingleVal(kill, userVal);
    }
    delete inst;
    return true;
  }
  return false;
}

bool ExprReorder::RemoveDeadInstTrival(User *inst, std::vector<User *> &kill)
{
  if (inst->GetUserUseListSize() == 0)
  {

    if (auto it = std::find(RetryInst.begin(), RetryInst.end(), inst);
        it != RetryInst.end())
    {
      RetryInst.erase(it);
    }

    for (auto &use : inst->GetUserUseList())
    {
      if (auto userVal = dynamic_cast<User *>(use->GetValue()))
        PushVecSingleVal(kill, userVal);
    }
    delete inst;
    return true;
  }
  return false;
}
