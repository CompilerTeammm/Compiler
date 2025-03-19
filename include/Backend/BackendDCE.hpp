#include "../../include/Backend/RISCVMIR.hpp"
#include "../../include/Backend/BackEndPass.hpp"
#include "../../include/Backend/Liveness.hpp"
#include "../../include/Backend/RISCVContext.hpp"

class BackendDCE : public Liveness, BackEndPass<RISCVFunction>
{
public:
  BackendDCE(RISCVFunction *func_, RISCVLoweringContext &_ctx) : Liveness(func_), ctx(_ctx), func(func_)
  {
    func = ctx.GetCurFunction(); // 获取基本块中的操作码
  }

  bool RunImpl();
  bool run(RISCVFunction *func);

private:
  RISCVLoweringContext &ctx;
  RISCVFunction *func;
};