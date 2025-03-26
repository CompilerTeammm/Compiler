#pragma once
#include "../../include/backend/RISCVContext.hpp"
#include "../../include/backend/RISCVMIR.hpp"
#include "../../include/backend/RISCVMOperand.hpp"
#include "../../include/backend/RISCVRegister.hpp"
#include "../../include/backend/RISCVType.hpp"
#include "../../include/lib/CFG.hpp"
#include "../../util/my_stl.hpp"
#include <unordered_map>
#include <unordered_set>

using MOperand =Register*;

class Liveness{
  private:
  void GetBlockLivein(RISCVBasicBlock *block);
  void GetBlockLiveout(RISCVBasicBlock *block);
  RISCVFunction *m_func;

  public:
  void CalCulateSucc(RISCVBasicBlock *block);
  std::unordered_map<RISCVBasicBlock*,std::list<RISCVBasicBlock*>> SuccBlocks;
  std::unordered_map<RISCVBasicBlock*,std::unordered_set<MOperand>> BlockLivein;
  std::unordered_map<RISCVBasicBlock*,std::unordered_set<MOperand>> BlockLiveout;
  std::unordered_map<RISCVMIR*,std::unordered_set<MOperand>> InstLive;

  //机器寄存器的集合，每个寄存器预先指派一种颜色
  std::unordered_set<MOperand> Precolored;//reg
  //算法最后会为每一个operand选择的颜色
  std::unordered_map<MOperand,PhyRegister*> color;

  //从一个结点到与该节点相关的传送指令表的映射
  std::unordered_map<MOperand,std::unordered_set<RISCVMIR*>> moveList;//reg2mov
  std::unordered_set<RISCVMIR*> NotMove;
  //有可能合并的传送指令
  std::vector<RISCVMIR*>worklistMoves;
  //图中冲突边（u，v）的集合，如果（u，v）\in adjset,那么（v,u）也
  std::unordered_map<MOperand,std::unordered_set<MOperand>> adjSet;
  //临时寄存器集合，其中的元素既没有被预着色，也没有被处理
  std::unordered_set<MOperand> initial;
  std::unordered_map<MOperand,std::unordered_set<MOperand>> AdjList;
  std::unordered_map<MOperand,int> Degree;
  RegisterList &reglist;
  void RunOnFunction();
  void PrintPass();
  void PrintEdge();
  void Build();
  void AddEdge(Register *u,Register *v);
  bool count(MOperand Op,RISCVMIR *inst){
    return InstLive[lnst].count(Op);
  }
  bool Count(Register *op);
  Liveness(RISCVFunction *f):m_func(f),BlockLivein{},BlockLiveout{},InstLive{},reglist(RegisterList::GetPhyRegList()){}
};