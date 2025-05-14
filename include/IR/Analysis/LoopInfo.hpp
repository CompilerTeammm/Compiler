#include "../../lib/CFG.hpp"
#include "Dominant.hpp"
#include "../../../util/my_stl.hpp"
#include "../Opt/AnalysisManager.hpp"
/// 单个循环的结构
class Loop
{
public:
  // friend class DominantTree;
  Loop() = default;
  Loop(BasicBlock *_Header) : Header(_Header) { BBs.push_back(_Header); }
  Loop(BasicBlock *_Header, BasicBlock *_Latch) : Header(_Header), Latch(_Latch) { BBs.push_back(_Header); }

  // 单个循环重写成一个“基本块列表”
  void addHeader(BasicBlock *BB) { Header == BB; }
  void addLoopBody(BasicBlock *BB) { PushVecSingleVal(BBs, BB); }
  void addLatch(BasicBlock *BB)
  {
    Latch = BB;
    PushVecSingleVal(BBs, BB);
  }
  void deleteBB(BasicBlock *BB)
  {
    auto iter = std::find(BBs.begin(), BBs.end(), BB);
    if (iter != BBs.end())
      BBs.erase(iter);
  }
  BasicBlock *getHeader() { return Header; }
  BasicBlock *getLatch() { return Latch; }

  // 嵌套循环
  void setLoopsHeader(Loop *loop) { LoopsHeader = loop; }
  void addLoopsBody(Loop *signalLoop) { Loops.push_back(signalLoop); }

  void addLoopsDepth(int depth) { LoopsDepth += depth; }
  Loop *GetLoopsHeader() { return LoopsHeader; }
  int getLoopsDepth() { return LoopsDepth; }
  int getLoopsSize() { return Loops.size(); }

  // 设置引用,外部访问
  std::vector<BasicBlock *> &getLoopBody() { return BBs; }
  std::vector<Loop *> &getLoops() { return Loops; }

  // 使用情况
  bool isVisited() { return visited; }
  void setVisited() { visited = true; }
  void setUnVisited() { visited = false; }
  bool ContainBB(BasicBlock *bb);
  bool ContainLoop(Loop *loop);

  // 迭代器
  std::vector<BasicBlock *>::iterator begin() { return BBs.begin(); }
  std::vector<BasicBlock *>::iterator end() { return BBs.end(); }
  std::vector<BasicBlock *>::reverse_iterator rbegin() { return BBs.rbegin(); }
  std::vector<BasicBlock *>::reverse_iterator rend() { return BBs.rend(); }
  std::vector<Loop *>::iterator loopsBegin() { return Loops.begin(); }
  std::vector<Loop *>::iterator loopsEnd() { return Loops.end(); }
  std::vector<Loop *>::reverse_iterator loopsRbegin() { return Loops.rbegin(); }
  std::vector<Loop *>::reverse_iterator loopsRend() { return Loops.rend(); }

  // 获取前驱头
  void setPreHeader(BasicBlock *bb) { PreHeader = bb; }
  BasicBlock *getPreHeader() { return PreHeader; }

  void clear()
  {
    Latch = nullptr;
    PreHeader = nullptr;
    LoopsDepth = 0;
  }

private:
  // 单个循环
  std::vector<BasicBlock *> BBs; // 一个循环重写成“基本块列表”
  BasicBlock *Header = nullptr;  // 单个循环头
  BasicBlock *Latch = nullptr;   // 单个循环尾

  // 嵌套循环
  std::vector<Loop *> Loops;   // 嵌套循环列表
  Loop *LoopsHeader = nullptr; // 嵌套循环的父循环
  int LoopsDepth = 0;          // 嵌套深度

  // 使用情况
  bool visited = false; // 是否被访问过

  // 健壮性
  BasicBlock *PreHeader = nullptr; // 前驱头
};

class LoopInfoAnalysis : public _PassBase<LoopInfoAnalysis, Function>
{
public:
  enum Flag
  {
    Strict,
    Loose,
  };
  LoopInfoAnalysis(Function *func, DominantTree *dom, std::vector<Loop *> &deleteLoop) : _func(func), _dom(dom), _deleteloop(deleteLoop)
  {
    setBBs();
    // setDest();
  }
  void setBBs() { _BBs = &_func->GetBBs(); }
  // void setDest(BasicBlock *bb) { Dest = &_dom->getSuccBBs(bb); }

  // run
  void runAnalysis(Function &F, AnalysisManager &AM);

  void PostOrderDT(BasicBlock *bb);
  // 数据获取、判断
  // bool ContainsBlockByIndex(Loop *Loop, int index);
  bool ContainsBlock(Loop *loop, BasicBlock *bb);
  bool isLoopExiting(Loop *loop, BasicBlock *bb);
  void getLoopDepth(Loop *loop, int depth);

  // 获取循环头、前继
  BasicBlock *getPreHeader(Loop *loop, Flag flag = Strict);
  BasicBlock *getLoopHeader(BasicBlock *bb);

  // 循环退出、循环跳转
  std::vector<BasicBlock *> getOverBlocks(Loop *loop);
  std::vector<BasicBlock *> getExitingBlocks(Loop *loop);

  // 删除
  // void deleteLoop(Loop *loop);
  // void deleteBB(BasicBlock *bb);

  // 功能
  // void newBB(BasicBlock *oldBB, BasicBlock *newBB);
  bool canBeOpte() { return loops.size() != 0; }

  ~LoopInfoAnalysis()
  {
    for (auto loop : loops)
    {
      delete loop;
    }
    loops.clear();
  }

private:
  Function *_func;
  DominantTree *_dom;
  std::vector<Loop *> &_deleteloop;
  std::vector<Loop *> loops;            // 存储所有循环
  std::vector<BBPtr> *_BBs;             // 存储所有基本块的引用
  std::vector<BasicBlock *> PostOrder;  // 存储后序遍历的基本块
  std::map<BasicBlock *, Loop *> Loops; // 基本块与循环的映射
  // std::vector<BasicBlock *> *Dest; // CFG中的后继
  int depth = 0;
  int index = 0;
};