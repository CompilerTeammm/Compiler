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
  
};