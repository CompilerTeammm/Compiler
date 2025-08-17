#pragma once
#include "../Analysis/LoopInfo.hpp"
#include "AnalysisManager.hpp"
class LcSSA : public _PassBase<LcSSA, Function>
{
public:
  LcSSA(Function *func, AnalysisManager &_AM) : m_func(func), AM(_AM) {}
  bool run();
  void PrintPass() {}
  ~LcSSA()
  {
    for (auto l : DeleteLoop)
      delete l;
  }

private:
  bool DFSLoops(LoopInfo *l);
  bool FormalLcSSA(std::vector<Instruction *> &FormingInsts);
  void InsertPhis(Use *u, std::set<BasicBlock *> &exit);
  void FindBBRecursive(std::set<BasicBlock *> &exit,
                       std::set<BasicBlock *> &target,
                       std::set<BasicBlock *> &visited, BasicBlock *bb);
  void FindBBRoot(BasicBlock *src, BasicBlock *dst,
                  std::set<BasicBlock *> &visited,
                  std::stack<BasicBlock *> &assist);
  Function *m_func;
  LoopAnalysis *loops;
  DominantTree *m_dom;
  AnalysisManager &AM;
  std::vector<LoopInfo *> DeleteLoop;
  // 记录lcssa后的phi对应的原值，方便后续替换
  std::unordered_map<PhiInst *, Value *> LcssaRecord;
  std::unordered_map<User *, std::vector<Use *>> UseRerite;
  std::set<PhiInst *> InsertedPhis;
};