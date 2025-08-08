#pragma once
#include "../Analysis/LoopInfo.hpp"
#include "../Analysis/Dominant.hpp"
#include "AnalysisManager.hpp"
#include "Passbase.hpp"
#include <unordered_map>
#include <vector>

class LoopRotaing : public _PassBase<LoopRotaing, Function>
{
public:
  LoopRotaing(Function *func, AnalysisManager &_AM) : m_func(func), AM(_AM) {}
  bool run() override
  {
    // ���������е� Run() �߼�����ֱ��ʵ�ֹ���
    return Run(); // ������� Run() ��ʵ��
  }
  void PrintPass();
  ~LoopRotaing()
  {
    for (auto l : DeleteLoop)
      delete l;
  }

private:
  bool Run();
  bool RotateLoop(Loop *loop, bool Succ);
  bool TryRotate(Loop *loop, DominantTree *m_dom);
  bool CanBeMove(User *I);
  void SimplifyBlocks(BasicBlock *Header, Loop *loop, DominantTree *m_dom);
  void PreservePhi(BasicBlock *header, BasicBlock *Latch, Loop *loop,
                   BasicBlock *preheader, BasicBlock *new_header,
                   std::unordered_map<Value *, Value *> &PreHeaderValue,
                   LoopInfoAnalysis *loopAnlasis, DominantTree *m_dom);
  void PreserveLcssa(BasicBlock *new_exit, BasicBlock *old_exit,
                     BasicBlock *pred);
  LoopInfoAnalysis *loopAnlasis;
  Function *m_func;
  DominantTree *m_dom;
  AnalysisManager &AM;
  std::unordered_map<Value *, Value *> CloneMap;
  std::vector<Loop *> DeleteLoop;
  const int Heuristic = 8;
};