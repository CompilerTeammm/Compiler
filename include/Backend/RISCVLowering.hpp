#include "../../include/Backend/BackEndPass.hpp"
#include "../../include/Backend/RISCVContext.hpp"
#include "../../include/Backend/RISCVISel.hpp"
#include "../../include/Backend/RISCVAsmPrinter.hpp"
#include "../../include/Backend/RISCVISel.hpp"
#include "../../include/Backend/RISCVRegister.hpp"
// #include "../../include/Backend/RegAlloc.hpp"
#include "../../include/lib/CFG.hpp"
#include "../../include/Backend/PhiElimination.hpp"
// #include "../../include/Backend/LegalizePass.hpp"
// #include "../../include/Backend/BackendDCE.hpp"

// 定义 LLVM IR 到 RISCV 转换的类
class RISCVModuleLowering : BackEndPass<Module>
{
public:
  bool run(Module *) override;

private:
  RISCVLoweringContext ctx;
  void LowerGlobalArgument(Module *);
};

class RISCVFunctionLowering : BackEndPass<Function>
{
public:
  bool run(Module *);
  RISCVFunctionLowering(RISCVLoweringContext &ctx, RISCVAsmPrinter *&asmprinter);

private:
  RISCVLoweringContext &ctx;
  RISCVAsmPrinter *&asmprinter;
};