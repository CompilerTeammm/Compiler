#pragma once
#include "../../include/Backend/RISCVContext.hpp"
#include "../../include/Backend/RISCVMIR.hpp"
#include "../../include/Backend/RISCVMOperand.hpp"
#include "../../include/Backend/RISCVRegister.hpp"
#include "../../include/Backend/RISCVType.hpp"
#include "../../include/lib/CFG.hpp"
#include "../../util/my_stl.hpp"
#include <unordered_map>
#include <unordered_set>

using MOperand = Register *;

class Liveness
{
private:
  void GetBlockLivein(RISCVBasicBlock *block);
  void GetBlockLiveout(RISCVBasicBlock *block);
  RISCVFunction *m_func;

public:
  void CalCulateSucc(RISCVBasicBlock *block);
  std::unordered_map<RISCVBasicBlock *, std::list<RISCVBasicBlock *>> SuccBlocks;
  std::unordered_map<RISCVBasicBlock *, std::unordered_set<MOperand>> BlockLivein;
  std::unordered_map<RISCVBasicBlock *, std::unordered_set<MOperand>> BlockLiveout;
  std::unordered_map<RISCVMIR *, std::unordered_set<MOperand>> InstLive;

  // 机器寄存器的集合，每个寄存器预先指派一种颜色
  std::unordered_set<MOperand> Precolored; // reg
  // 算法最后会为每一个operand选择的颜色
  std::unordered_map<MOperand, PhyRegister *> color;

  // 从一个结点到与该节点相关的传送指令表的映射
  std::unordered_map<MOperand, std::unordered_set<RISCVMIR *>> moveList; // reg2mov
  std::unordered_set<RISCVMIR *> NotMove;
  // 有可能合并的传送指令
  std::vector<RISCVMIR *> worklistMoves;
  // 图中冲突边（u，v）的集合，如果（u，v）\in adjset,那么（v,u）也
  std::unordered_map<MOperand, std::unordered_set<MOperand>> adjSet;
  // 临时寄存器集合，其中的元素既没有被预着色，也没有被处理
  std::unordered_set<MOperand> initial;
  std::unordered_map<MOperand, std::unordered_set<MOperand>> AdjList;
  std::unordered_map<MOperand, int> Degree;
  RegisterList &reglist;
  void RunOnFunction();
  void PrintPass();
  void PrintEdge();
  void Build();
  void AddEdge(Register *u, Register *v);
  bool count(MOperand Op, RISCVMIR *inst)
  {
    return InstLive[inst].count(Op);
  }
  bool Count(Register *op);
  Liveness(RISCVFunction *f) : m_func(f), BlockLivein{}, BlockLiveout{}, InstLive{}, reglist(RegisterList::GetPhyRegList()) {}
};

class LiveInterval : public Liveness
{
  RISCVFunction *func; // 记录当前正在分析的RISCV目标函数

public:
  // 代表一个变量的活跃区间
  struct RegLiveInterval
  {
    int start;
    int end;
    bool operator<(const RegLiveInterval &other) const
    {
      return start < other.start; // 重载运算符
    }
  };
  using Interval = RegLiveInterval;
  std::unordered_map<RISCVMIR *, int> instNum;                                                  // 记录inst的编号用于计算活跃区间
  std::unordered_map<RISCVBasicBlock *, Liveness *> BlockInfo;                                  // 记录block内部的liveness分析结果
  std::map<RISCVBasicBlock *, std::unordered_map<MOperand, std::vector<Interval>>> RegLiveness; // 记录reg在block中的活跃区间
  std::unordered_map<MOperand, Interval> GlobalLiveRange;                                       // 记录reg在整个函数范围内的活跃区间

private:
  void init();                                                                   // 初始化LiveInterval数据结构
  void computeLiveIntervals();                                                   // 计算活跃区间
  bool verify(std::unordered_map<MOperand, std::vector<Interval>> Liveinterval); // 检查计算出的LiveInterval是否正确

public:
  LiveInterval(RISCVFunction *f) : func(f), Liveness(f) {}
  std::unordered_map<MOperand, std::vector<Interval>> &GetRegLiveInterval(RISCVBasicBlock *block)
  {
    return RegLiveness[block];
  }
  void RunOnFunc_(); // 执行完整的活跃区间分析
  void PrintAnalysis();
  // 判断op1 op2活跃先后
  bool IsOp1LiveBeforeOp2(MOperand op1, MOperand op2)
  {
    return (GlobalLiveRange[op1].start < GlobalLiveRange[op2].start);
  }
  bool IsOp1LiveAfterOp2(MOperand op1, MOperand op2)
  {
    return (GlobalLiveRange[op1].end > GlobalLiveRange[op2].end);
  }
  // 判断op1 op2 是否有活跃区间重叠（干涉）
  bool IsHasInterference(MOperand op1, MOperand op2)
  {
    return !(GlobalLiveRange[op1].end < GlobalLiveRange[op2].start || GlobalLiveRange[op1].start > GlobalLiveRange[op2].end);
  }
};