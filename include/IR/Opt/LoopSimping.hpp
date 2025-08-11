#include "../../lib/CFG.hpp"
#include "../Analysis/LoopInfo.hpp"
#include "Passbase.hpp"
#include "AnalysisManager.hpp"

class LoopSimping : public _PassBase<LoopSimping, Function>
{
public:
  LoopSimping(Function *func, AnalysisManager &_AM) : m_func(func), AM(_AM) {} // 初始所有成员
  bool run() override
  {
    // 调用您现有的 Run() 逻辑，或直接实现功能
    return Run(); // 如果已有 Run() 的实现
  }
  void PrintPass();
  static void CaculateLoopInfo(Loop *loop, LoopInfoAnalysis *Anlay, DominantTree *m_dom);
  ~LoopSimping()
  {
    for (auto l : DeleteLoop)
      delete l;
  }

private:
  bool Run();
  bool SimplifyLoopsImpl(Loop *loop, DominantTree *m_dom);
  void SimplifyPhi();
  void InsertPreHeader(Loop *loop);
  void FormatExit(Loop *loop, BasicBlock *exit, DominantTree *m_dom);
  void FormatLatch(Loop *loop, BasicBlock *PreHeader, std::vector<BasicBlock *> &latch, DominantTree *m_dom);
  void UpdatePhiNode(PhiInst *phi, std::set<BasicBlock *> &worklist, BasicBlock *target);
  Loop *SplitNewLoop(Loop *L);
  void UpdateInfo(std::vector<BasicBlock *> &bbs, BasicBlock *insert, BasicBlock *head, Loop *loop, DominantTree *m_dom);
  void UpdateLoopInfo(BasicBlock *Old, BasicBlock *New, const std::vector<BasicBlock *> &pred);
  LoopInfoAnalysis *loopAnlay;
  std::vector<Loop *> DeleteLoop;
  Function *m_func;
  DominantTree *m_dom;
  AnalysisManager &AM;
};