#pragma once
#include "../../include/Backend/BackEndPass.hpp"
#include "../../include/Backend/RISCVContext.hpp"
#include "../../include/Backend/RISCVMIR.hpp"
#include "../../include/Backend/RISCVMOperand.hpp"
#include "../../include/Backend/RISCVRegister.hpp"
#include "../../include/Backend/RISCVType.hpp"
#include "../../include/Backend/RegAlloc.hpp"
#include "../../include/lib/CFG.hpp"
#include "../../include/lib/CFG.hpp"
#include "../../util/my_stl.hpp"
class BackendDCE : public Liveness, BackEndPass<RISCVFunction>
{
public:
  BackendDCE(RISCVFunction *func_, RISCVLoweringContext &_ctx) : Liveness(func_), ctx(_ctx), func(func_)
  {
    func = ctx.GetCurFunction(); // 获取基本块中的操作码
  }

  bool RunImpl();

private:
  bool run(RISCVFunction *func);
  RISCVLoweringContext &ctx;
  RISCVFunction *func;
  std::vector<RISCVMIR *> wait_del;
  bool CanHandle(RISCVMIR *inst);
  bool RunOnBlock(RISCVBasicBlock *block);
  bool TryDeleteInst(RISCVMIR *inst, std::unordered_set<MOperand> &live);
  void UpdateLive(RISCVMIR *inst, std::unordered_set<MOperand> &live);
};