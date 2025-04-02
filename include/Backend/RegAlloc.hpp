#pragma once
#include "../../include/backend/RISCVContext.hpp"
#include "../../include/backend/RISCVMIR.hpp"
#include "../../include/backend/RISCVMOperand.hpp"
#include "../../include/backend/RISCVRegister.hpp"
#include "../../include/backend/RISCVType.hpp"
#include "../../include/lib/CFG.hpp"
#include "../../util/my_stl.hpp"
#include "../../include/backend/Liveness.hpp"
#include <algorithm>
#include <cmath>
#include <list>
#include <map>
#include <queue>
#include <set>
#include <stack>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

class GraphColor;
//寄存器分配实现类
class RegAllocImpl {
  RISCVLoweringContext &ctx;

public:
  RegAllocImpl(RISCVFunction *func, RISCVLoweringContext &_ctx)
      : m_func(func), ctx(_ctx) {
    m_func = ctx.GetCurFunction();
  }
  void RunGCpass();//调用GraphColor进行寄存器分配

protected:
  std::vector<PhyRegister *> float_available;
  std::vector<PhyRegister *> int_available;//存储可用的物理寄存器
  RISCVFunction *m_func;
  GraphColor *gc;//指向graphcolor
  int availble;
};
using MOperand = Register *;
class GraphColor : public LiveInterval {
public:
  RISCVLoweringContext &ctx;
  RISCVFunction *m_func;
  using Interval = LiveInterval::RegLiveInterval;//活跃区间
  using IntervalLength = unsigned int;
  GraphColor(RISCVFunction *func, RISCVLoweringContext &_ctx)
      : LiveInterval(func), ctx(_ctx), m_func(func) {}
  void RunOnFunc();

private:
  /// @brief 初始化各个工作表
  void MakeWorklist();
  // 返回vector为0则不是move相关
  std::unordered_set<RISCVMIR *> MoveRelated(MOperand v);
  void CalcmoveList(RISCVBasicBlock *block);//计算每个基本块中move指令
  void CalcIG(RISCVMIR *inst);//计算冲突图
  void New_CalcIG(MOperand u, MOperand v);
  void CalInstLive(RISCVBasicBlock *block);
  void CaculateLiveness();//活跃变量分析
  void CaculateLiveInterval(RISCVBasicBlock *mbb);
  void simplify();//移除低度数节点
  void coalesce();//合并不冲突的变量
  void freeze();//冻结某些变量防止合并
  void spill();//选择溢出变量
  void AssignColors();//为变量分配物理寄存器
  void SpillNodeInMir();
  void RewriteProgram();
  void CaculateTopu(RISCVBasicBlock *mbb);
  void DecrementDegree(MOperand target);
  MOperand HeuristicFreeze();
  MOperand HeuristicSpill();
  PhyRegister *SelectPhyReg(MOperand vreg, RISCVType ty,std::unordered_set<MOperand> &assist);//为虚拟寄存器选择合适的物理寄存器
  bool GeorgeCheck(MOperand dst, MOperand src, RISCVType ty);
  bool BriggsCheck(MOperand dst, MOperand src, RISCVType ty);
  void AddWorkList(MOperand v);
  void CaculateSpillLiveness();
  void combine(MOperand rd, MOperand rs);
  MOperand GetAlias(MOperand v);
  void FreezeMoves(MOperand freeze);
  void SetRegState(PhyRegister *reg, RISCVType ty);
  int GetRegNums(MOperand v);
  int GetRegNums(RISCVType ty);
  void GC_init();//初始化图着色算法，用于寄存器分配
  void LiveInfoInit();//初始化活跃变量信息
  std::set<MOperand> Adjacent(MOperand);
  RISCVMIR *CreateSpillMir(RISCVMOperand *spill,std::unordered_set<VirRegister *> &temps);
  RISCVMIR *CreateLoadMir(RISCVMOperand *load,std::unordered_set<VirRegister *> &temps);
  void Print();
  std::vector<MOperand> SpillStack;
  // 保证Interval vector的顺序
  std::unordered_map<MOperand, IntervalLength> ValsInterval;
  enum MoveState { coalesced, constrained, frozen, worklist, active };
  // 低度数的传送有关节点表
  std::unordered_set<MOperand> freezeWorkList;
  // 低度数的传送无关节点表
  std::vector<MOperand> simplifyWorkList;
  // 高度数的节点表
  std::unordered_set<MOperand> spillWorkList;
  // 本轮中要溢出的节点集合
  std::unordered_set<MOperand> spilledNodes;
  // 已合并的寄存器集合，当合并u<--v，将v加入到这个集合中，u则被放回到某个工作表中(或反之)
  std::unordered_set<MOperand> coalescedNodes;
  // 源操作数和目标操作数冲突的传送指令集合
  std::unordered_set<RISCVMIR *> constrainedMoves;
  // 已经合并的传送指令集合
  std::vector<RISCVMIR *> coalescedMoves;
  // 不再考虑合并的传送指令集合
  std::unordered_set<RISCVMIR *> frozenMoves;
  // 已成功着色的结点集合
  std::unordered_set<MOperand> coloredNode;
  // 从图中删除的临时变量的栈
  std::vector<MOperand> selectstack;
  // 查询每个传送指令属于哪一个集合
  std::unordered_map<RISCVMIR *, MoveState> belongs;
  // 还未做好准备的传送指令集合
  std::unordered_set<RISCVMIR *> activeMoves;
  // 合并后的别名管理
  std::unordered_map<MOperand, MOperand> alias;
  std::unordered_map<PhyRegister *, RISCVType> RegType;
  std::vector<RISCVBasicBlock *> topu;
  std::set<RISCVBasicBlock *> assist;
  std::unordered_map<MOperand, int> SpillToken;
  float LoopWeight = 1;
  float livenessWeight = 2.5;
  float DegreeWeight = 3;
  float SpillWeight = 5;
};