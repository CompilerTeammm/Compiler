#include "../../include/Backend/BackEndPass.hpp"
#include "../../include/lib/Module.hpp"

// 定义 LLVM IR 到 RISCV 转换的类
class RISCVModuleLowering : BackEndPass<Module>
{
public:
  bool run(Module *);

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
}