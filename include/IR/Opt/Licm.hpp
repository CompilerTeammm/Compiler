#pragma once
#include "../Analysis/LoopInfo.hpp"
#include "../Analysis/AliasAnalysis.hpp"
#include "../Analysis/SideEffect.hpp"

class LICMPass : public _PassBase<LICMPass, Function>
{
public:
  LICMPass(Function *func, AnalysisManager &_AM) : AM(_AM), m_func(func) {}
  bool run();
  bool RunOnLoop(LoopInfo *l);
  void PrintPass() {};
  bool change = false;
  ~LICMPass()
  {
    for (auto l : DeleteLoop)
      delete l;
  }

private:
  bool licmSink(const std::set<BasicBlock *> &contain, LoopInfo *l,
                BasicBlock *bb);
  bool licmHoist(const std::set<BasicBlock *> &contain, LoopInfo *l,
                 BasicBlock *bb);
  bool UserInsideLoop(User *I, LoopInfo *curloop);
  bool UserOutSideLoop(const std::set<BasicBlock *> &contain, User *I,
                       LoopInfo *curloop);
  bool CanBeMove(LoopInfo *curloop, User *I);
  bool IsDomExit(User *I, std::vector<BasicBlock *> &exit);
  DominantTree *m_dom;
  Function *m_func;
  AliasAnalysis *alias;
  AnalysisManager &AM;
  LoopAnalysis *loop;
  std::vector<LoopInfo *> DeleteLoop;
};