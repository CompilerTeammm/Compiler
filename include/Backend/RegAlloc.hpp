#include "../../include/Backend/RISCVMIR.hpp"
#include "../../include/Backend/RISCVContext.hpp"

class RegAllocImpl
{
public:
  RegAllocImpl(RISCVFunction *func, RISCVLoweringContext &_ctx) : m_func(func), ctx(_ctx)
  {
    m_func = ctx.GetCurFunction();
  }
  void RunGCpass();

private:
  RISCVLoweringContext &ctx;
  RISCVFunction *m_func;
};