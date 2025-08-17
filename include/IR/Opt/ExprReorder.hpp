#pragma once
#include "../../../include/IR/Analysis/Dominant.hpp"
#include "../../../include/IR/Opt/Passbase.hpp"
#include "../../../include/lib/CoreClass.hpp"
#include "../../../include/lib/CFG.hpp"
#include <vector>
#include <map>

class _AnalysisManager;

class ExprReorder : public _PassBase<ExprReorder, Function>
{
public:
  ExprReorder(Function *func) : targetFunc(func) {}

  bool run();
  void PrintPass() {}

private:
  void BuildRankTable();
  void TraverseCFGPostOrder(BasicBlock *root, DominantTree *m_dom);

  bool OptimizeInstruction(Value *inst);
  bool ReorderBinaryExpr(BinaryInst *inst);

  void FlattenExpression(BinaryInst *inst, std::vector<std::pair<Value *, int>> &Leaf);
  void SplitOpRe(Value *inst, std::vector<Value *> &ops);
  Value *CAExpr(User *inst, std::vector<Value *> &AddOperands);
  Value *EliminateOperand(Value *inst);

  Value *OptimizeExpr(BinaryInst *exp, std::vector<std::pair<Value *, int>> &LinerizedOp);
  Value *OptimizeAdd(BinaryInst *AddInst, std::vector<std::pair<Value *, int>> &LinerizedOp);
  Value *OptimizeMul(BinaryInst *MulInst, std::vector<std::pair<Value *, int>> &LinerizedOp);

  bool IsBinaryFloatOp(BinaryInst *inst);
  bool IsOpCommutative(BinaryInst::Operation opcode);
  bool IsOperandAssociative(Value *op, BinaryInst::Operation opcode);
  void StandardizeBinaryInst(User *inst);
  bool RewriteExpression(BinaryInst *exp, std::vector<std::pair<Value *, int>> &LinerizedOp);
  bool ShouldIgnoreConstant(BinaryInst::Operation Op, ConstantData *constdata);
  bool CanAbsorbConstant(BinaryInst::Operation Op, ConstantData *constdata);

  bool RemoveDeadInst(User *inst, int idx);
  bool RemoveDeadInst(User *inst, std::vector<User *> &kill);
  bool RemoveDeadInstTrival(User *inst, std::vector<User *> &kill);

  int GetValueOrder(Value *val);

  std::map<Value *, int> ValueOrder;
  std::vector<BasicBlock *> PostOrderBlocks;
  std::vector<User *> RetryInst;
  Function *targetFunc;
};
