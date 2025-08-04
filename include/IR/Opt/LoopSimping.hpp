#include "../../lib/CFG.hpp"
#include "../Analysis/LoopInfo.hpp"

class LoopSimping : public _PassBase<LoopSimping, Function>
{
public:
  LoopSimping(Function *func) : m_func(func) {} // ��ʼ���г�Ա
  bool run() override
  {
    // ���������е� Run() �߼�����ֱ��ʵ�ֹ���
    return Run(); // ������� Run() ��ʵ��
  }
  void PrintPass();
  static void CaculateLoopInfo(Loop *loop, LoopInfoAnalysis *Anlay);
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
  // AnalysisManager &AM;
  std::vector<Loop *> DeleteLoop;
  Function *m_func;
  DominantTree *m_dom;
};