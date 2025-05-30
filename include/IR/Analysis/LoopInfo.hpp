#include "../../lib/CFG.hpp"
#include "Dominant.hpp"
#include "../../../util/my_stl.hpp"
#include "../Opt/AnalysisManager.hpp"
#include <cmath>
// 这一坨就直接加了
struct LoopTrait
{
  // for(int i=0;i<N;i+=step)的循环特性
  //  主要是为了在LoopInfoAnalysis中使用
  BasicBlock *head = nullptr; // 循环头块
  Value *boundary = nullptr;  // 循环终止边界值（如 i < N 中的 N）
  Value *initial = nullptr;   // 初始值（如 i = 0）
  int step = 0;               // 步长（如 i += step 中的 step）
  PhiInst *indvar = nullptr;  // 循环变量本身（如 i）
  User *change = nullptr;     // 修改循环变量的操作（如 i += step）
  PhiInst *res = nullptr;     // 可能是某种结果Phi节点
  CallInst *call = nullptr;   // 调用指令（可能在判断循环特性时有用）
  BinaryInst *cmp = nullptr;  // 控制循环的比较操作（如 i < N）
  bool CmpEqual = false;      // 是否包含等于（如 i <= N）
  void Init()
  {
    head = nullptr;
    boundary = nullptr;
    initial = nullptr;
    step = 0;
    indvar = nullptr;
    change = nullptr;
    res = nullptr;
    call = nullptr;
    CmpEqual = false;
  }
};

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

  // 获取前驱头
  void setPreHeader(BasicBlock *bb) { PreHeader = bb; }
  BasicBlock *getPreHeader() { return PreHeader; }

  void clear()
  {
    Latch = nullptr;
    PreHeader = nullptr;
    LoopsDepth = 0;
  }
  LoopTrait trait;
  // 扩展
  Loop *GetParent() { return Parent; }

private:
  // 单个循环
  std::vector<BasicBlock *> BBs; // 一个循环重写成“基本块列表”
  BasicBlock *Header = nullptr;  // 单个循环头
  BasicBlock *Latch = nullptr;   // 单个循环尾
  Loop *Parent = nullptr;

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

  virtual bool run() override
  {
    runAnalysis(); // 委托给现有的runAnalysis
  }
  // run
  void runAnalysis();

  void PostOrderDT(BasicBlock *bb);
  // 数据获取、判断
  // bool ContainsBlockByIndex(Loop *Loop, int index);
  bool ContainsBlock(Loop *loop, BasicBlock *bb);
  bool isLoopExiting(Loop *loop, BasicBlock *bb);
  void getLoopDepth(Loop *loop, int depth);

  // 获取循环头、前继
  BasicBlock *getPreHeader(Loop *loop, Flag flag = Strict);
  BasicBlock *getLoopHeader(BasicBlock *bb);
  BasicBlock *getLatch(Loop *loop);

  // 循环退出、循环跳转
  std::vector<BasicBlock *> getOverBlocks(Loop *loop);
  std::vector<BasicBlock *> getExitingBlocks(Loop *loop);

  // 删除
  // void deleteLoop(Loop *loop);
  void deleteBB(BasicBlock *bb);

  // 功能
  void newBB(BasicBlock *oldBB, BasicBlock *newBB);
  bool canBeOpte() { return loops.size() != 0; }

  std::vector<Loop *>::const_iterator loopsBegin() { return loops.begin(); }
  std::vector<Loop *>::const_iterator loopsEnd() { return loops.end(); }
  std::vector<Loop *>::const_reverse_iterator loopsRbegin() { return loops.rbegin(); }
  std::vector<Loop *>::const_reverse_iterator loopsRend() { return loops.rend(); }

  Loop *LookUp(BasicBlock *bb);
  // 扩展
  void setLoop(BasicBlock *bb, Loop *loop);

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